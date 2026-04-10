#ifndef CONFIG_H
#define CONFIG_H

#define COLOR_RESET   "\033[0m"

static const char col_trace[]   = "#616E88";
static const char col_debug[]   = "#88C0D0";
static const char col_info[]    = "#A3BE8C";
static const char col_warn[]    = "#EBCB8B";
static const char col_error[]   = "#BF616A";
static const char col_fatal[]   = "#B48EAD";
static const char col_white[]   = "#696969";

//for multiple logfiles name prefix colors
static const char *file_colors[] = {
    "#00ff00",
    "#ffffff",
    "#abb2bf,"
    "#ff00ff",
    "#0000ff",
    "#00ffff",
    "#ff0000",
    "#ffff00"
};
#define FILE_COLOR_COUNT 8

typedef enum {
    LOG_WHITE,
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} LogLevel;

#endif
