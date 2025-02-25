CC = clang
CFLAGS = -Wall -Wextra -std=c2x
FRAMEWORKS = -framework ApplicationServices
TARGET = throttling
SRC = throttling.c

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
else
$(error This game only works on macOS)
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
