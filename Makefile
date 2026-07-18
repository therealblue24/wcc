# in the near future...
# CC = ./bin/wcc
CC ?= cc
# binary directory
BINDIR = bin
# app name
APP = wcc

# default compiler flags
CFLAGS = -std=c11 -Wall -Wextra -Isrc -Izz/inc/ -Ibird/inc -g3
CFLAGS += -MMD -MP
# default linker flags
LDFLAGS = -Lbin -lbird -lzz

# Optimize code (-Os, etc)
RELEASE ?= no
# -Weverything but sane
CODE_REVIEW ?= no
# debug mode
SANITIZERS ?= no
# foritfy source code?
FORTIFY ?= no

# for release, enable -Os and LTO
ifeq ($(RELEASE), yes)
	CFLAGS += -Os
	LDFLAGS += -flto
endif

# fortifying source code enables stack protector and _FORTIFY_SOURCE
ifeq ($(FORITFY), yes)
	CFLAGS += -fstack-protector -D_FORTIFY_SOURCE=3
	LDFLAGS += -fstack-protector
endif

# -Weverything but better
ifeq ($(CODE_REVIEW), yes)
	# Enable EVERYTHING.
    CFLAGS += -Weverything
    # We are using C
    CFLAGS += -Wno-unsafe-buffer-usage
    # We don't care about trailing ;
    CFLAGS += -Wno-extra-semi-stmt
    # We are using C11
    CFLAGS += -Wno-declaration-after-statement
    # I honestly don't care right know
    CFLAGS += -Wno-padded
    # I don't care
    CFLAGS += -Wno-date-time
endif

# memory debugging
ifeq ($(SANITIZERS), yes)
	CFLAGS += -fsanitize=undefined,address,leak
	LDFLAGS += -fsanitize=undefined,address,leak
endif

# src files
SRC = $(shell find src -name "*.c")
# src/*.c -> bin/*.c
OBJ = $(SRC:src/%.c=$(BINDIR)/%.o)
# dependencies
DEP = $(OBJ:.o=.d)

# arguments to pass to program w/ `make test`
TESTARGS =

# all possible things
.PHONY: dirs build test link test help clean compile

# default action
all: dirs build link

# help screen
help:
	@echo "make build - build (not link) project"
	@echo "make link - link project"
	@echo "make compile - build & link"
	@echo "make clean - remove all binaries"
	@echo "make test - test project"
	@echo "make testsanitizers - test project w/ sanitizers"
	@echo "make dirs - create binary directory"
	@echo "make count - count lines of code for swag points"
	@echo "make help - this screen"
	@echo "make - default: dirs, build, link"
	@echo "==============FLAGS=================="
	@echo "CC\t\tC compiler"
	@echo "RELEASE\t\tyes/no for release build"
	@echo "CODE_REVIEW\tyes/no for code reviewing"
	@echo "SANITIZERS\tyes/no to enable sanitizers"
	@echo "FORTIFY\t\tyes/no to fortify code"
	@echo "TESTARGS\targs to pass to 'make test'"

SRC_PLUS_BIRD_ZZ = $(SRC) $(shell find src -name "*.h") $(shell find bird/src/ -name "*.c") $(shell find bird/inc/ -name "*.h") $(shell find zz/src/ -name "*.c") $(shell find zz/inc/ -name "*.h") 

count:
	@cloc $(SRC_PLUS_BIRD_ZZ)

dirs:
	@# Create bin dir
	@mkdir -p $(BINDIR)

# compile each single file
$(BINDIR)/%.o: src/%.c
	@echo "compiling $<"
	@$(CC) -o $@ -c $< $(CFLAGS)

# include deps
-include $(DEP)

# build target
build: dirs $(OBJ)

# build `bIRd`
build-bird:
	@echo "building bird"
	@make -C bird RELEASE=$(RELEASE) CODE_REVIEW=$(CODE_REVIEW) SANITIZERS=$(SANITIZERS) FORTIFY=$(FORTIFY)

# build `zz`
build-zz:
	@echo "building zz"
	@make -C zz RELEASE=$(RELEASE) CODE_REVIEW=$(CODE_REVIEW) SANITIZERS=$(SANITIZERS) FORTIFY=$(FORTIFY)

# compile target
$(BINDIR)/$(APP): link

compile: build link

# link target
link: build-zz build-bird build
	@cp bird/bin/libbird.a $(BINDIR)/libbird.a
	@cp zz/bin/libzz.a $(BINDIR)/libzz.a
	@echo "linking $(APP)"
	@$(CC) -o $(BINDIR)/$(APP) $(LDFLAGS) $(OBJ)
	@echo "made $(APP)"

# remove bins
clean:
	@echo "cleaning"
	rm -rf $(BINDIR)
	@make -C bird clean
	@make -C zz clean
	@echo "cleaned"

# test app
test: $(BINDIR)/$(APP)
	@./test.sh

testfast:
	@make -B RELEASE=yes fast
	@make test

fast:
	@make -j$(shell nproc)

testsanitizers:
	@make -B SANITIZERS=yes
	@ASAN_OPTIONS=detect_leaks=1 ./test.sh
	@make -B clean
