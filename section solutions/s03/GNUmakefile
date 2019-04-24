PROGRAMS = factorial stacksmash stacksmash.safe attackme unattackable
all: $(PROGRAMS)

O = 0
include ../common/rules.mk

SAFE_CFLAGS := $(CFLAGS) $(SANITIZEFLAGS)
UNSAFE_CFLAGS := $(CFLAGS) -U_FORTIFY_SOURCE -fno-stack-protector

%.o: %.c $(BUILDSTAMP)
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) $(O) $(DEPCFLAGS) -o $@ -c $<

%.safe.o: %.c $(BUILDSTAMP)
	$(CC) $(CFLAGS) $(SAFE_CFLAGS) $(O) $(DEPCFLAGS) -o $@ -c $<

factorial: factorial.o hexdump.o
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) $(O) -o $@ $^

attackme: attackme.o evil.o hexdump.o
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) -o $@ $^

unattackable: unattackable.o evil.o hexdump.o
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) -o $@ $^

stacksmash: stacksmash.o hexdump.o
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) -o $@ $^

stacksmash.safe: stacksmash.safe.o hexdump.o
	$(CC) $(CFLAGS) $(SAFE_CFLAGS) -o $@ $^

stacksmash2: stacksmash2.o
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) -o $@ $^

stacksmash2.safe: stacksmash2.safe.o
	$(CC) $(CFLAGS) $(SAFE_CFLAGS) -o $@ $^


ifeq ($(wildcard secret.c),secret.c)
secret.o: %.o: %.c $(BUILDSTAMP)
	$(CC) $(CFLAGS) $(UNSAFE_CFLAGS) -O1 $(DEPCFLAGS) -o $@ -c $<

evil.o: secret.o
	cp secret.o evil.o
endif


clean:
	rm -f $(filter-out evil.o,$(wildcard *.o)) $(PROGRAMS) *.unsafe *.safe *.core
	rm -rf $(DEPSDIR) *.dSYM

.PHONY: all clean
