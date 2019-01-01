CFLAGS=-Werror -Wall
LDLIBS=-lcurl -lncurses -ljansson

.PHONY: clean

cweather:

clean:
	rm -f cweather
