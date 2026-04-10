# mTail - Log Tailing Utility [ AI generated code ]

A fast, colorful log tailing utility for Linux with filtering capabilities.

[ A colorized, filterable log viewer for Linux. Built to replicate the mTail.exe experience in the CLI.]

## Features

- **Real-time tailing**: Follow log files as they're written
- **Log level filtering**: Filter by DEBUG, INFO, WARN, ERROR, FATAL, TRACE
- **String search**: Search log lines for specific text (case-insensitive)
- **Multiple files**: Tail multiple log files simultaneously with colored prefixes
- **Custom colors**: Configure all colors in `config.h`
- **UTF-8 support**: Proper handling of unicode characters and emojis

## Build & Install

```bash
make clean && make
sudo make install
```

## Usage

### Basic Usage
```bash
./mTail app.log                    # Tail single file, show all lines
./mTail /var/log/syslog            # Tail any regular file
./mTail -i app.log                 # Show only INFO level
./mTail -w app.log                 # Show only WARN level
./mTail -e app.log                 # Show only ERROR level
```

### Search Filter
```bash
./mTail -s "error" app.log        # Show lines containing "error"
./mTail -si "USDC" app.log         # "USDC" AND INFO level only
./mTail -s str1 -s str2 app.log    # Multiple searches (AND logic)
```

### Multiple Files
```bash
./mTail app1.log app2.log          # Tail multiple files
./mTail -i app1.log app2.log        # INFO level from both files
./mTail -s "error" app*.log         # Use shell glob
```

### Combined Flags
```bash
./mTail -si term file.log          # -s = search, -i = info level
./mTail -sw error file.log          # -s = search, -w = warn level
./mTail -de                        # -d = debug, -e = error (shows both)
```

## Level Flags

| Flag | Level  |
|------|--------|
| `-d` | DEBUG  |
| `-i` | INFO   |
| `-w` | WARN   |
| `-e` | ERROR  |
| `-f` | FATAL  |
| `-t` | TRACE  |

## Color Configuration

All colors are configurable in `config.h` using hex format:

```c
static const char col_trace[]   = "#616E88";
static const char col_debug[]   = "#88C0D0";
static const char col_info[]    = "#A3BE8C";
static const char col_warn[]    = "#EBCB8B";
static const char col_error[]   = "#BF616A";
static const char col_fatal[]   = "#B48EAD";
static const char col_white[]   = "#5c6370";

// File prefix colors (cycle for multiple files)
static const char *file_colors[] = {
    "#98c379", "#e5c07b", "#61afef", "#e06c75",
    "#56b6c2", "#c678dd", "#abb2bf", "#d19a66"
};
```

## Safety Constraints

- **Read-only**: mTail never modifies log files
- **File type restriction**: Only tails regular files (rejects directories, devices, sockets, etc.)
- **No device files**: Never tails `/dev/mem` or similar
- **Permissions**: Use `sudo` only for system logs

## Example Output

```
[log1.log                 ] [INFO] Message from first file
[log2.log                 ] [WARN] Message from second file
[log1.log                 ] [INFO] Message from first file
[log2.log                 ] [WARN] Message from second file

```

## License

MIT
