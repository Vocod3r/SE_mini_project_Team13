CC      ?= gcc
CFLAGS  ?= -std=c11 -g -Wall -Wextra -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lsqlite3

LIB_SRC  := $(filter-out $(wildcard test_*.c) crash.c,$(wildcard *.c))
TEST_SRC := $(wildcard test_*.c)
TEST_BIN := $(TEST_SRC:%.c=out/%)

.PHONY: ci compile test asan clean
ci: compile test asan        # run this before every push

compile: $(TEST_BIN)

out/%: %.c $(LIB_SRC) | out
	$(CC) $(CFLAGS) $< $(LIB_SRC) -o $@ $(LDLIBS)

out:
	mkdir -p out

test: compile
	@fail=0; for t in $(TEST_BIN); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer"

clean:
	rm -rf out *.db *.db-wal *.db-shm
