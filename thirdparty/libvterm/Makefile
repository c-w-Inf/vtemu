override CFLAGS +=-Wall -Iinclude -std=c99 -Wpedantic

ifeq ($(shell uname),SunOS)
  override CFLAGS +=-D__EXTENSIONS__ -D_XPG6 -D__XOPEN_OR_POSIX
endif

ifeq ($(DEBUG),1)
  override CFLAGS +=-ggdb -DDEBUG
endif

ifeq ($(PROFILE),1)
  override CFLAGS +=-pg
  override LDFLAGS+=-pg
endif

CFILES=$(sort $(wildcard src/*.c))
HFILES=$(sort $(wildcard include/*.h))
OBJECTS=$(CFILES:.c=.o)

HFILES_INT=$(sort $(wildcard src/*.h)) $(HFILES)

.PHONY: all clean

all: $(OBJECTS)

src/%.o: src/%.c $(HFILES_INT)
	$(CC) $(CFLAGS) -o $@ -c $<

DEPS := $(OBJS:.o=.d)
-include $(DEPS)

clean:
	rm -f $(OBJECTS)

