SRCS = $(wildcard *.c)
SUBDIRS = $(wildcard */) # if has sub dirs 'foo, boo', return foo/, boo/
CLEAN_SUBDIRS := $(addsuffix .clean, $(SUBDIRS))

EXE_SUFFIX = .out

FILES = $(patsubst %.c, %, $(SRCS))
PROGS := $(addsuffix $(EXE_SUFFIX), $(FILES))

.DEFAULT_GOAL := all

FINAL_CFLAGS+= -g -ggdb

debug:
ifneq ($(strip $(SUBDIRS)),)
	@echo 'subdirs -> [${SUBDIRS}]'
endif
ifneq ($(strip $(CLEAN_SUBDIRS)),)
	@echo 'clean_subdirs -> [${CLEAN_SUBDIRS}]'
endif

all: $(PROGS) $(SUBDIRS)
%.out: %.c
	$(CC) $(FINAL_CFLAGS) -o $*.o -c $<
	$(CC) -o $@ $*.o
	@echo '============= compile $< to $@ done ============='

$(SUBDIRS):
	$(MAKE) -C $@ all



clean: clean_programs clean_subdirs
clean_programs:
	rm -f $(addsuffix .o, $(FILES)) $(PROGS)

clean_subdirs: $(CLEAN_SUBDIRS)
$(CLEAN_SUBDIRS):
	$(MAKE) -C $(basename $@) clean
	@echo '============= $@ clean done ============='

.PHONY: all programs subdirs $(SUBDIRS) clean clean_programs clean_subdirs $(CLEAN_SUBDIRS)
# https://stackoverflow.com/questions/2145590/what-is-the-purpose-of-phony-in-a-makefile

# $(foreach SUB_PATH, $(SUB_PATHS), $(MAKE) -C $(SUB_PATH) all )
# https://stackoverflow.com/questions/15661951/makefile-foreach-make-c-call

# ifdef SUB_PATHS
# https://stackoverflow.com/questions/22991068/ifdef-make-3-81-invalid-syntax-in-conditional

# make conditional or a receipt
# https://stackoverflow.com/questions/64368002/ifdef-conditional-unexpected-behavior

# CUR_PATH = $(shell pwd)/
# SUB_PATHS = $(foreach t,$(SUBDIRS),$(addprefix $(CUR_PATH), $t))
# PROGS = $(patsubst %.c, %, $(SRCS))
# https://www.gnu.org/software/make/manual/html_node/Text-Functions.html

# reference:
# https://www.gnu.org/software/make/manual/html_node/Concept-Index.html
# https://yuukidach.github.io/p/makefile-for-projects-with-subdirectories/
