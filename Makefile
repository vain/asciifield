CFLAGS += -Wall -Wextra -O3
LDLIBS += -lm

.PHONY: all clean
all: asciifield

clean:
	rm -f asciifield

