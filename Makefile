

#CC = clang-6.0 -fblocks -lBlocksRuntime
CC = gcc
CPPFLAGS = -iquote src/
CFLAGS = -std=gnu17 -g -O2 -Wall -Wextra -fsanitize=undefined -fsanitize-undefined-trap-on-error
ARFLAGS = rsU


TYPESRC := $(wildcard src/type/*.c)
TYPEHDR := $(wildcard src/type/*.h)
TYPEOBJ := $(TYPESRC:.c=.o)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

(%): %
	$(AR) $(ARFLAGS) $@ $%

libtype.a: libtype.a($(TYPEOBJ))

