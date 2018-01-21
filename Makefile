CFLAGS=-Werror -Wall
LDLIBS=-lcurl -lmenu -lncurses -ljansson

.PHONY: clean

cweather:

clean:
	rm -f cweather
