PROGRAMS = membench-arena membench-cheatalloc membench-malloc
ifeq ($(strip $(shell $(CC) -ljemalloc 2>&1 | grep -- -ljemalloc)),)
PROGRAMS += membench-jemalloc
endif
ifeq ($(strip $(shell $(CC) -ltcmalloc 2>&1 | grep -- -ltcmalloc)),)
PROGRAMS += membench-tcmalloc
endif
all: $(PROGRAMS)

CPPFLAGS += -pthread
include ../common/rules.mk

%.o: %.c $(BUILDSTAMP)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) $(O) -o $@ -c $<

membench-%: membench.o %.o
	$(CC) $(CFLAGS) $(O) -o $@ $^ -lpthread

membench-jemalloc: membench.o malloc.o
	$(CC) $(CFLAGS) $(O) -o $@ $^ -ljemalloc -lpthread

membench-tcmalloc: membench.o malloc.o
	$(CC) $(CFLAGS) $(O) -o $@ $^ -ltcmalloc -lpthread

clean:
	rm -f *.o $(PROGRAMS) membench-jemalloc membench-tcmalloc
	rm -rf $(DEPSDIR) *.dSYM

.PHONY: all clean
