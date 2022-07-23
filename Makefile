CFLAGS = -Os
CFLAGS += -Wall -Werror
CFLAGS += -std=c99
CFLAGS += -DUSE_SEPARATE_EXECUTE
CFLAGS += -DUSE_TAILCALL
CFLAGS += -DENABLE_TRACING

DEBUG = 1

# debug stuff
ifneq ($(DEBUG),0)
CC = /usr/local/opt/llvm@13/bin/clang
#CFLAGS += -fsanitize=integer
#CFLAGS += -fno-sanitize-recover=integer
CFLAGS += -fsanitize=address
CFLAGS += -fno-sanitize-recover=undefined
CFLAGS += -g -O0
CFLAGS += -I /usr/local/include
CLINKFLAGS += -L /usr/local/lib
export ASAN_OPTIONS = detect_leaks=1:detect_stack_use_after_return=1
export UBSAN_OPTIONS = print_stacktrace=1
export MallocNanoZone = 0
else
CFLAGS += -DNDEBUG
CFLAGS += -Wno-unused-but-set-variable
CFLAGS += -Wno-unused-variable
CFLAGS += -Wno-return-type
CFLAGS += -fomit-frame-pointer
CFLAGS += -O3
# CFLAGS += -flto
# CLINKFLAGS += -flto
endif

MAINSRCS += main.c
TESTSRCS += test.c

TEST_WASM=./test.wasm

SRCS += context.c
SRCS += decode.c
SRCS += endian.c
SRCS += expr.c
SRCS += exec.c
SRCS += fileio.c
SRCS += import_object.c
SRCS += insn.c
SRCS += instance.c
SRCS += repl.c
SRCS += module.c
SRCS += leb128.c
SRCS += type.c
SRCS += util.c
SRCS += validation.c
SRCS += vec.c
SRCS += xlog.c

OBJS = $(SRCS:.c=.o)
MAINOBJS = $(MAINSRCS:.c=.o)
TESTOBJS = $(TESTSRCS:.c=.o)

MAINBIN = main_bin
TESTBIN = test_bin

.PHONY: run
run: $(MAINBIN)
	./$(MAINBIN) --trace $(TEST_WASM)

.PHONY: test
test: $(TESTBIN)
	./$(TESTBIN)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(MAINBIN): $(MAINOBJS) $(OBJS)
	$(CC) $(CLINKFLAGS) $(CFLAGS) -o $@ $(MAINOBJS) $(OBJS)

$(TESTBIN): $(TESTOBJS) $(OBJS)
	$(CC) $(CLINKFLAGS) $(CFLAGS) -o $@ $(TESTOBJS) $(OBJS) -lcmocka

.PHONY: style
style:
	clang-format -i $(SRCS) $(MAINSRCS) $(TESTSRCS) *.h

.PHONY: clean
clean:
	rm -f $(MAINBIN)
	rm -f $(MAINOBJS)
	rm -f $(TESTBIN)
	rm -f $(TESTOBJS)
	rm -f $(OBJS)
