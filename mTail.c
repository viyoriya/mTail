#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include "config.h"

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))
#define MAX_LOG_LEVEL 16
#define MAX_FILES 32

typedef struct {
    const char *path;
    char *filename;
    int fd;
    int wd;
    off_t size;
    int color_idx;
} WatchedFile;

static void print_hex_color(const char *hex_color, const char *msg) {
    int r, g, b;
    if (sscanf(hex_color + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
        printf("\033[38;2;%d;%d;%dm%s\033[0m", r, g, b, msg);
    } else {
        printf("%s", msg);
    }
}

static void print_colored(const char *msg, LogLevel level) {
    const char *hex_color;
    switch (level) {
        case LOG_TRACE:   hex_color = col_trace; break;
        case LOG_DEBUG:   hex_color = col_debug; break;
        case LOG_INFO:    hex_color = col_info;  break;
        case LOG_WARN:    hex_color = col_warn;  break;
        case LOG_ERROR:   hex_color = col_error; break;
        case LOG_FATAL:   hex_color = col_fatal; break;
        default:          hex_color = col_white; break;
    }
    print_hex_color(hex_color, msg);
}

static LogLevel current_filter = LOG_WHITE;
static int use_prefix = 0;
static char *search_strings[8] = {NULL};
static int search_count = 0;

static LogLevel detect_log_level(const char *msg) {
    if (strcasestr(msg, " FATAL") || strcasestr(msg, "FATAL ")) return LOG_FATAL;
    if (strcasestr(msg, " ERROR") || strcasestr(msg, "ERROR ")) return LOG_ERROR;
    if (strcasestr(msg, " WARN") || strcasestr(msg, "WARN ")) return LOG_WARN;
    if (strcasestr(msg, " INFO") || strcasestr(msg, "INFO ")) return LOG_INFO;
    if (strcasestr(msg, " DEBUG") || strcasestr(msg, "DEBUG ")) return LOG_DEBUG;
    if (strcasestr(msg, " TRACE") || strcasestr(msg, "TRACE ")) return LOG_TRACE;
    return LOG_WHITE;
}

static void process_line(const char *line, const char *filename, int color_idx) {
    char level_str[MAX_LOG_LEVEL] = {0};
    char *msg_start = NULL;
    LogLevel level = detect_log_level(line);

    if (current_filter != LOG_WHITE && level != current_filter) {
        return;
    }

    if (search_count > 0) {
        int all_match = 1;
        for (int i = 0; i < search_count; i++) {
            if (strcasestr(line, search_strings[i]) == NULL) {
                all_match = 0;
                break;
            }
        }
        if (!all_match) return;
    }

    if (use_prefix && filename) {
        char fname[26];
        size_t len = strlen(filename);
        memset(fname, ' ', 25);
        if (len > 25) {
            memcpy(fname, filename, 25);
        } else {
            memcpy(fname, filename, len);
        }
        fname[25] = '\0';
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "[%s] ", fname);
        print_hex_color(file_colors[color_idx], prefix);
    }

    if (sscanf(line, "%15s", level_str) == 1) {
        if (level_str[0] == '[') {
            msg_start = strchr((char*)line, ']');
            if (msg_start) {
                char before_level[1024] = {0};
                size_t prefix_len = msg_start - line + 1;
                if (prefix_len < sizeof(before_level)) {
                    strncpy(before_level, line, prefix_len);
                    print_colored(before_level, LOG_WHITE);
                    print_colored(msg_start + 1, level);
                    printf("\n");
                    return;
                }
            }
        }
    }
    
    print_colored(line, level);
    printf("\n");
}

static size_t get_utf8_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static void decode_and_print(const char *data, size_t len, const char *filename, int color_idx) {
    char *line = malloc(len + 1);
    if (!line) return;
    
    size_t j = 0;
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        if (c >= 32 && c < 127) {
            line[j++] = c;
            i++;
        } else if (c >= 0xC0) {
            size_t char_len = get_utf8_len(c);
            if (i + char_len <= len) {
                memcpy(line + j, data + i, char_len);
                j += char_len;
            }
            i += char_len;
        } else if (c == '\n' || c == '\r') {
            line[j] = '\0';
            if (j > 0) {
                process_line(line, filename, color_idx);
                j = 0;
            }
            i++;
        } else {
            line[j++] = '?';
            i++;
        }
    }
    
    if (j > 0) {
        line[j] = '\0';
        process_line(line, filename, color_idx);
    }
    
    free(line);
}

static off_t get_file_size(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == -1) return -1;
    return st.st_size;
}

static char *get_filename(const char *path) {
    const char *base = strrchr(path, '/');
    return base ? strdup(base + 1) : strdup(path);
}

static void tail_files(WatchedFile *files, int file_count) {
    int ifd = inotify_init();
    if (ifd < 0) {
        perror("inotify_init");
        return;
    }

    for (int i = 0; i < file_count; i++) {
        files[i].size = get_file_size(files[i].path);
        if (files[i].size < 0) {
            fprintf(stderr, "Cannot access file: %s\n", files[i].path);
            continue;
        }

        files[i].fd = open(files[i].path, O_RDONLY);
        if (files[i].fd < 0) {
            perror("open");
            continue;
        }

        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(files[i].fd, buffer, sizeof(buffer))) > 0) {
            decode_and_print(buffer, bytes_read, files[i].filename, files[i].color_idx);
        }

        files[i].wd = inotify_add_watch(ifd, files[i].path, IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
        if (files[i].wd < 0) {
            perror("inotify_add_watch");
            close(files[i].fd);
        }
    }

    fflush(stdout);

    fd_set rfds;
    struct timeval tv;
    char event_buf[EVENT_BUF_LEN];

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(ifd, &rfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(ifd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) break;

        if (FD_ISSET(ifd, &rfds)) {
            ssize_t len = read(ifd, event_buf, EVENT_BUF_LEN);
            if (len > 0) {
                int i = 0;
                while (i < len) {
                    struct inotify_event *event = (struct inotify_event *)&event_buf[i];
                    
                    for (int j = 0; j < file_count; j++) {
                        if (files[j].wd == event->wd) {
                            if (event->mask & IN_DELETE_SELF || event->mask & IN_MOVE_SELF) {
                                close(files[j].fd);
                                files[j].fd = -1;
                            } else if (event->mask & IN_MODIFY) {
                                off_t new_size = get_file_size(files[j].path);
                                if (new_size > files[j].size) {
                                    lseek(files[j].fd, files[j].size, SEEK_SET);
                                    ssize_t to_read = new_size - files[j].size;
                                    char *buf = malloc(to_read);
                                    if (buf) {
                                        ssize_t r = read(files[j].fd, buf, to_read);
                                        if (r > 0) decode_and_print(buf, r, files[j].filename, files[j].color_idx);
                                        free(buf);
                                    }
                                    fflush(stdout);
                                    files[j].size = new_size;
                                } else if (new_size < files[j].size) {
                                    lseek(files[j].fd, 0, SEEK_SET);
                                    files[j].size = 0;
                                }
                            }
                            break;
                        }
                    }
                    i += sizeof(struct inotify_event) + event->len;
                }
            }
        }
    }

    close(ifd);
    for (int i = 0; i < file_count; i++) {
        if (files[i].fd >= 0) close(files[i].fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [options] <logfile> [<logfile>...]\n\n", argv[0]);
        printf("Options:\n");
        printf("  -d, -i, -w, -e, -f, -t  Filter by log level (debug, info, warn, error, fatal, trace)\n");
        printf("  -s <string>              Search for string(s) in log lines (case-insensitive)\n");
        printf("  -h, --help               Show this help message\n\n");
        printf("Combined flags:\n");
        printf("  -si, -sw, -se, etc.      Search + level filter (e.g., -si term = search 'term' in INFO)\n\n");
        printf("Multiple search strings:\n");
        printf("  -s str1 -s str2          Show lines containing BOTH strings (AND logic)\n\n");
        printf("Examples:\n");
        printf("  %s app.log\n", argv[0]);
        printf("  %s -i app.log\n", argv[0]);
        printf("  %s -s error app.log\n", argv[0]);
        printf("  %s -si error app.log\n", argv[0]);
        printf("  %s -s err -s failed app.log\n", argv[0]);
        printf("  %s app1.log app2.log\n", argv[0]);
        printf("  %s /var/log/syslog\n", argv[0]);
        return 0;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [options] <logfile> [<logfile>...]\n\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -d, -i, -w, -e, -f, -t  Filter by log level (debug, info, warn, error, fatal, trace)\n");
        fprintf(stderr, "  -s <string>              Search for string(s) in log lines (case-insensitive)\n");
        fprintf(stderr, "  -h, --help               Show this help message\n\n");
        fprintf(stderr, "Combined flags:\n");
        fprintf(stderr, "  -si, -sw, -se, etc.      Search + level filter (e.g., -si term = search 'term' in INFO)\n\n");
        fprintf(stderr, "Multiple search strings:\n");
        fprintf(stderr, "  -s str1 -s str2          Show lines containing BOTH strings (AND logic)\n");
        return 1;
    }

    int file_start = 1;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
fprintf(stderr, "Error: -s requires a search string\n");
            return 1;
            }
            if (search_count >= 8) {
                fprintf(stderr, "Error: Too many search strings (max 8)\n");
                return 1;
            }
            search_strings[search_count++] = argv[i + 1];
            i += 2;
        } else if (argv[i][0] == '-' && strlen(argv[i]) > 1) {
            int j = 1;
            int has_s = 0;
            
            while (argv[i][j]) {
                if (argv[i][j] == 's') {
                    has_s = 1;
                } else if (argv[i][j] == 'i') {
                    current_filter = LOG_INFO;
                } else if (argv[i][j] == 'w') {
                    current_filter = LOG_WARN;
                } else if (argv[i][j] == 'e') {
                    current_filter = LOG_ERROR;
                } else if (argv[i][j] == 'd') {
                    current_filter = LOG_DEBUG;
                } else if (argv[i][j] == 't') {
                    current_filter = LOG_TRACE;
                } else if (argv[i][j] == 'f') {
                    current_filter = LOG_FATAL;
                } else {
                    fprintf(stderr, "Error: Unknown flag -%c\n", argv[i][j]);
                    return 1;
                }
                j++;
            }
            
            if (has_s) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: -s requires a search string\n");
                    return 1;
                }
                if (search_count >= 8) {
                    fprintf(stderr, "Error: Too many search strings (max 8)\n");
                    return 1;
                }
                search_strings[search_count++] = argv[i + 1];
                i += 2;
            } else {
                i++;
            }
        } else {
            break;
        }
    }

    file_start = i;

    if (file_start >= argc) {
        fprintf(stderr, "Error: No files specified\n");
        return 1;
    }

    int file_count = argc - file_start;
    if (file_count == 0) {
        fprintf(stderr, "Error: No log files specified. Did you mean: mTail -s <string> <logfile>?\n");
        return 1;
    }

    for (int i = file_start; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) == -1) {
            fprintf(stderr, "Error: Cannot access '%s': %s\n", argv[i], strerror(errno));
            return 1;
        }
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Error: '%s' is not a regular file\n", argv[i]);
            return 1;
        }
    }

    WatchedFile files[MAX_FILES];
    int valid_count = 0;

    for (int i = 0; i < file_count; i++) {
        const char *path = argv[file_start + i];
        if (access(path, R_OK) == 0) {
            files[valid_count].path = path;
            files[valid_count].filename = get_filename(path);
            files[valid_count].fd = -1;
            files[valid_count].wd = -1;
            files[valid_count].size = 0;
            files[valid_count].color_idx = valid_count % FILE_COLOR_COUNT;
            valid_count++;
        } else {
            fprintf(stderr, "Warning: File not found or not readable: %s\n", path);
        }
    }

    if (valid_count == 0) {
        fprintf(stderr, "Error: No log files found. Please check the file paths.\n");
        return 1;
    }

    use_prefix = (valid_count > 1);
    if (valid_count == 1 && current_filter != LOG_WHITE) {
        use_prefix = 0;
    }

    tail_files(files, valid_count);

    for (int i = 0; i < valid_count; i++) {
        free(files[i].filename);
    }
    return 0;
}
