# reference:
# https://www.gnu.org/software/make/manual/html_node/Concept-Index.html
# https://yuukidach.github.io/p/makefile-for-projects-with-subdirectories/

SRCS = $(wildcard *.c)
SUBDIRS = $(wildcard */) # if has sub dirs 'foo, boo', return foo/, boo/
CUR_PATH = $(shell pwd)/
CLEAN_SUBDIRS := $(addsuffix .clean, $(SUBDIRS))
# SUB_PATHS = $(foreach t,$(SUBDIRS),$(addprefix $(CUR_PATH), $t))

# https://www.gnu.org/software/make/manual/html_node/Text-Functions.html
PROGS = $(patsubst %.c,%,$(SRCS))

.DEFAULT_GOAL := all

# make conditional or a receipt: https://stackoverflow.com/questions/64368002/ifdef-conditional-unexpected-behavior
echo:
#ifdef SUB_PATHS: https://stackoverflow.com/questions/22991068/ifdef-make-3-81-invalid-syntax-in-conditional
ifneq ($(strip $(SUBDIRS)),)
	@echo 'subdirs -> [${SUBDIRS}]'
endif
ifneq ($(strip $(CLEAN_SUBDIRS)),)
	@echo 'clean_subdirs -> [${CLEAN_SUBDIRS}]'
endif

all: programs subdirs

programs: $(PROGS)
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
	@echo '$@ compile $< done'

subdirs: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ all

clean: clean_programs clean_subdirs
clean_programs:
	rm -f $(PROGS)

clean_subdirs: $(CLEAN_SUBDIRS)
$(CLEAN_SUBDIRS):
	$(MAKE) -C $(basename $@) clean
	@echo '$@ clean done'


# $(MAKE) -C ${SUB_PATHS} all		https://stackoverflow.com/questions/15661951/makefile-foreach-make-c-call
# https://stackoverflow.com/questions/2145590/what-is-the-purpose-of-phony-in-a-makefile
.PHONY: all programs subdirs $(SUBDIRS) clean clean_programs clean_subdirs $(CLEAN_SUBDIRS)
