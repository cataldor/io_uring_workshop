# Root Makefile
# command line:
# 	make {-s} {V=[0, 1]} {OPT=-O[0, 1, 2, 3]}
#
# exports:
# 	MAKE_DIR = directory from where make is running from
# 	CC	= gcc compiler
# 	CC2	= clang compiler
# 	OBJDIR	= directory for *.o objects
# 	CFLAGS	= Warning flags supported by CC + CC2
# 	CGCCFLAGS = warning flags supported by CC
# 	CCLANGFLAGS = warning flags supported by CC2
# 	DFLAGS	= debug flags
#	OPT = target for optimization
#	TESTFILES = name for testing scenario (make test)
#	TESTOUTDIR = test output
#	Q = symbol for verbose control
#
MAKE_DIR = $(PWD)

CC	=	gcc
CC2	=	clang

OBJDIR	=	obj

SRC_DIR := blocking
SRC_DIR += raw_uring
SRC_DIR += luring

CFLAGS	= -Wall
CFLAGS	+= -Wmissing-declarations -Wuninitialized -Wtype-limits
CFLAGS 	+= -Wshadow -Wpointer-arith -Wcomment -Wdeprecated -Wmissing-field-initializers
CFLAGS	+= -Wsign-compare -Wvla -Wstrict-overflow=5 -Wwrite-strings -Wcast-qual -Winline

CGCCFLAGS = -Wstack-usage=512 -Wunsafe-loop-optimizations -Wlogical-op
CGCCFLAGS += -Wclobbered

CCLANGFLAGS = -Warray-bounds-pointer-arithmetic -Wassign-enum -Wmismatched-tags
CCLANGFLAGS += -Wc99-compat -Wfor-loop-analysis -Wrange-loop-analysis
CCLANGFLAGS += -Wbad-function-cast -Wbitfield-enum-conversion -Wchar-subscripts
CCLANGFLAGS += -Wcomma -Wconditional-uninitialized -Widiomatic-parentheses
CCLANGFLAGS += -Wkeyword-macro -Wmissing-braces -Wmissing-variable-declarations
CCLANGFLAGS += -Wnull-pointer-arithmetic -Wover-aligned -Woverlength-strings
CCLANGFLAGS += -Wsometimes-uninitialized -Wundef -Wunneeded-internal-declaration
CCLANGFLAGS += -Wshorten-64-to-32

DFLAGS = -g
OPT ?= -O1
V ?= 0

ifeq ($(V), 1)
	Q =
else
	Q = @
endif

TESTDIR = $(shell pwd)/test
TESTFILES = $(TESTDIR)/one_plus_one.txt $(TESTDIR)/four_plus_one.txt $(TESTDIR)/eight.txt
TESTOUTDIR = $(TESTDIR)/out

export MAKE_DIR CC CC2 OBJDIR CFLAGS CGCCFLAGS CCLANGFLAGS DFLAGS OPT
export TESTFILES TESTOUTDIR Q

.PHONY: all help asan set_asan_flags test clean

all:
	@for d in $(SRC_DIR); 			\
	do					\
		$(MAKE) -C $$d -f makefile.mk;	\
	done

help:
	@echo  'Cleaning targets:'
	@echo  '  clean		  - Remove all gelerated files'
	@echo  'Generic targets:'
	@echo  '  all		  - Build all targets'
	@echo  'Static analysers:'
	@echo  '  scan            - clang scan-build'
	@echo  'Runtime analysers:'
	@echo  '  asan            - gcc runtime asan (address + undefined)'
	@echo  'target selftest:'
	@echo  '  test            - Run target selftest'

asan: | set_asan_flags all

set_asan_flags:
	$(eval CFLAGS += -fsanitize=address,undefined)
	$(eval LINKERFLAGS += -fsanitize=address,undefined)

scan:
	@for d in $(SRC_DIR); 					\
	do							\
		scan-build $(MAKE) -C $$d -f makefile.mk check;	\
	done

test:
	cat $(TESTFILES) > $(TESTOUTDIR)/cat
	@for d in $(SRC_DIR);				\
	do						\
		$(MAKE) -C $$d -f makefile.mk test;	\
	done
	$(TESTDIR)/verify_checksum.sh $(TESTOUTDIR)

clean:
	@rm -f $(TESTOUTDIR)/cat
	$(foreach dir, $(SRC_DIR), $(MAKE) -C $(dir) -f makefile.mk clean;)
