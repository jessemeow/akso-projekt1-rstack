CC = gcc
CFLAGS = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu23 -fPIC -O2

WRAP_FLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
	-Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup \
	-Wl,--wrap=strndup

LDFLAGS_LIB = -shared $(WRAP_FLAGS)
LDFLAGS_EXE = -L. -lrstack -Wl,-rpath=. $(WRAP_FLAGS)

.PHONY: all clean test

all: librstack.so test_runner

librstack.so: rstack.o rgarbage_collector.o memory_tests.o
	$(CC) $(LDFLAGS_LIB) -o $@ $^

rstack.o: rstack.c rstack.h
	$(CC) $(CFLAGS) -c $<

rgarbage_collector.o: rgarbage_collector.c rgarbage_collector.h
	$(CC) $(CFLAGS) -c $<

memory_tests.o: memory_tests.c memory_tests.h
	$(CC) $(CFLAGS) -c $<


test_runner: rstack_example.c librstack.so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_EXE)


test: test_runner
	./test_runner zero
	./test_runner one
	./test_runner two
	./test_runner three
	./test_runner four
	./test_runner five
	./test_runner memory

clean:
	rm -f *.o *.so test_runner
