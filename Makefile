CFLAGS=-Werror -Wall
LDLIBS=-lcurl -lncurses -ljansson

ifeq ($(PREFIX),)
  PREFIX:=/usr/local
endif

.PHONY: clean install

cweather:

install: cweather
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -D -m 0755 $< $(DESTDIR)$(PREFIX)/bin/cweather

clean:
	rm -f cweather
