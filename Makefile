CC = clang
CFLAGS = -Wall -Wextra
FRAMEWORKS = -framework CoreGraphics -framework IOKit -framework CoreFoundation -framework ApplicationServices
TARGET = throttling
SRC = throttling.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
