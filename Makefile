CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = mTail
SRC = mTail.c
INSTALL_PATH = /usr/local/bin

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo install -m 755 $(TARGET) $(INSTALL_PATH)/$(TARGET)

uninstall:
	sudo rm -f $(INSTALL_PATH)/$(TARGET)
