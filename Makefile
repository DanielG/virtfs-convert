CC=cc

CFLAGS=-g -ggdb3 -Wall -Werror --std=gnu99 -Iavl-2.0.2a
LDFLAGS=

DEPS=$(patsubst %.o,%.d,$(OBJS))

all: virtfs-convert

install: all
	install -m 755 virtfs-convert $(DESTDIR)/usr/bin/

virtfs-convert: convert.o
	@echo "\tLD $@"
	$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	@echo "\tCC $@"
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) -MM $(CFLAGS) $*.c > $*.d

clean:
	rm -f virtfs-convert *.o *.d
