# set this to your favorite compiler
CC = clang
# binary directory
BINDIR = bin
# lib name
LIBNAM = libzz.a

# default compiler flags
CFLAGS = -std=c11 -Wall -Wextra -Iinc/zz/ -g3
CFLAGS += -MMD -MP
# default linker flags
LDFLAGS = 

# Optimize code (-O2, etc)
RELEASE ?= no
# -Weverything but sane
CODE_REVIEW ?= no
# debug mode
SANITIZERS ?= no
# foritfy source code?
FORTIFY ?= no

# for release, enable -O2 and LTO
ifeq ($(RELEASE), yes)
	CFLAGS += -O2
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
# src/*.c -> *.o
OBJ = $(SRC:src/%.c=$(BINDIR)/%.o)
# dependencies
DEP = $(OBJ:.o=.d)

# all possible things
.PHONY: dirs build test link test clean compile

# default action
all: dirs build link

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

# compile target
$(BINDIR)/$(LIBNAM): link

compile: build link

# link target
link: build
	@echo "linking library"
	@ar rcs $(BINDIR)/$(LIBNAM)	$(OBJ)
	@echo "made lib $(LIBNAM)"

# remove bins
clean:
	@echo "cleaning"
	rm -rf $(BINDIR)
	@echo "cleaned"
