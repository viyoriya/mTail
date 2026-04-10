# mTail - Log Tailing Utility

## Build & Run
- Build: `make clean && make && sudo make install`
- Run: `./mTail <logfile>`

## Usage
```bash
./mTail <logfile>                      # Show all lines (any regular file)
./mTail /var/log/syslog                # Tail system log file
./mTail -w <logfile>                    # Filter by level (debug, info, warn, error, fatal, trace)
./mTail -i <logfile>                    # INFO only
./mTail -s <string> <logfile>           # Search for string (case-insensitive)
./mTail -si <string> <logfile>          # Combined: search + level filter
./mTail -s str1 -s str2 <logfile>       # Multiple search strings (AND logic)
./mTail <log1> <log2>                   # Multiple files (shows [filename] prefix)
```

## Level Flags
- `-d` - DEBUG
- `-i` - INFO
- `-w` - WARN
- `-e` - ERROR
- `-f` - FATAL
- `-t` - TRACE

## Colors (config.h)
All colors configurable in `config.h` using hex format: `#RRGGBB`

## Safety Constraints
- Read-only: never modify log files
- Only tail regular files (rejects directories, device files, sockets, etc.)
- Use `sudo` only when explicitly requested for system logs
