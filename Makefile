.PHONY: default
default: all

# Build options
#==============

# Compile options
#----------------

DEBUG_SYMBOLS =-g -O1

PROFILING_SYMBOLS =-p $(DEBUG_SYMBOLS)

TO_OBJ = -c
TO_FILE = -o

# Enable print log
PRINT_LOG = -DDEBUG

# Preprocess out asserts
NO_ASSERTS = -DNDEBUG

OPT =-O3 -ffast-math -ftree-vectorize

WARNINGS =-Wall -Wextra -Wredundant-decls

### C
CC =gcc -std=gnu99
CFLAGS = -fPIC -march=native

### C++
CPPFLAGS := $(CFLAGS)
#CLANG_OPTIONS = -fsanitize=memory -fno-optimize-sibling-calls -fno-omit-frame-pointer -fsanitize-memory-track-origins=2
CPP =clang++-3.5 -std=c++14 $(CLANG_OPTIONS)


# Linking options
#----------------
LDLIBS = -lm -lutil


# Structure
#====================

# More precise but typically not necessary and more verbose
THIS_DIR := $(abspath $(CURDIR)/$(dir $(lastword $(MAKEFILE_LIST))))
#THIS_DIR := .

OBJ_DIR = $(THIS_DIR)/obj
$(OBJ_DIR):
	-mkdir $(OBJ_DIR)

VENDOR_DIR := $(THIS_DIR)/vendor
$(VENDOR_DIR):
	-mkdir $(VENDOR_DIR)

# Vendor definitions
#-------------------

### ACPC
ACPC_SRC_DIR :=$(VENDOR_DIR)/project_acpc_server/
ACPC_INCLUDES :=-I$(ACPC_SRC_DIR)

#### Game
GAME_SRC :=$(ACPC_SRC_DIR)/game.c
GAME_OBJ :=$(OBJ_DIR)/project_acpc_server-game.o
$(GAME_OBJ): $(GAME_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(TO_OBJ) $(TO_FILE) $@ $< $(ACPC_INCLUDES)

#### Net
NET_SRC :=$(ACPC_SRC_DIR)/net.c
NET_OBJ :=$(OBJ_DIR)/project_acpc_server-net.o
$(NET_OBJ): $(NET_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(TO_OBJ) $(TO_FILE) $@ $< $(ACPC_INCLUDES)

#### Rng
RNG_SRC :=$(ACPC_SRC_DIR)/rng.c
RNG_OBJ :=$(OBJ_DIR)/project_acpc_server-rng.o
$(RNG_OBJ): $(RNG_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(TO_OBJ) $(TO_FILE) $@ $< $(ACPC_INCLUDES)

ACPC_OBJ :=$(GAME_OBJ) $(NET_OBJ) $(RNG_OBJ)


VENDOR_INCLUDES :=-I$(VENDOR_DIR) $(ACPC_INCLUDES)
VENDOR_OBJS :=$(ACPC_OBJ)

LIB_OBJ  =$(VENDOR_OBJS)

# Project
#--------
INCLUDES =$(VENDOR_INCLUDES)

### Directories
BIN_DIR :=$(THIS_DIR)/bin
$(BIN_DIR):
	-mkdir $(BIN_DIR)

DEP_DIR = $(THIS_DIR)/dep
$(DEP_DIR):
	-mkdir $(DEP_DIR)

SRC_DIR := $(THIS_DIR)/src


### Source files
C_SRC = $(shell find $(SRC_DIR) -type f -name '*.c' 2>/dev/null)
C_HEADERS =$(shell find $(SRC_DIR) -type f -name '*.h' 2>/dev/null)
CPP_SRC = $(shell find $(SRC_DIR) -type f -name '*.cpp' 2>/dev/null)
CPP_HEADERS =$(shell find $(SRC_DIR) -type f -name '*.hpp' 2>/dev/null)

### Intermediate files
OBJ = $(addprefix $(OBJ_DIR)/, $(subst /,-,$($(notdir C_SRC):$(SRC_DIR)/%.c=%.o) $($(notdir CPP_SRC):$(SRC_DIR)/%.cpp=%.o)))
DEP = $(addprefix $(DEP_DIR)/, $(subst /,-,$($(notdir OBJ):$(OBJ_DIR)/%.o=%.d)))

MAIN_OBJ =$(filter %-main.o,$(OBJ))

LIB_OBJ  +=$(filter-out %-main.o,$(OBJ))

### Executables
TO_TARGET=$(patsubst %-main.o,%,$(notdir $(subst tools-,,$1)))
TARGETS  =$(call TO_TARGET,$(MAIN_OBJ))


SRC_INCLUDES := -I$(SRC_DIR)
INCLUDES +=$(SRC_INCLUDES)


# Build rules
#============

# Base rules
#-----------

#### Dealer
DEALER_SRC :=$(SRC_DIR)/lib/dealer.c
DEALER_OBJ :=$(OBJ_DIR)/lib-dealer.o
$(DEALER_OBJ): $(DEALER_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(TO_OBJ) $(TO_FILE) $@ $< $(ACPC_INCLUDES)

$(MAIN_OBJ): $(CPP_SRC) $(CPP_HEADERS) $(C_SRC) $(C_HEADERS) | $(BIN_DIR)
	@if [ ! -d $(@D) ]; then mkdir -p $(@D); fi
	@echo [CPP] $<
	@$(CPP) $(CPPFLAGS) $(TO_OBJ) $< $(TO_FILE) $@ $(INCLUDES)

$(foreach $(OBJ_DIR),$(MAIN_OBJ),$(eval $(call TO_TARGET,$($(OBJ_DIR))): $($(OBJ_DIR))))
$(TARGETS): $(LIB_OBJ) $(MAIN_OBJ)
$(TARGETS):
	@if [ ! -d $(@D) ]; then mkdir -p $(@D); fi
	@echo [LD] $@
	@$(CPP) $(CPPFLAGS) $(LDFLAGS) $(TO_FILE) $@ $^ $(LDLIBS)
	@chmod 755 $@


format: $(CPP_SRC) $(CPP_HEADERS) | $(BIN_DIR)
	@echo [format] $^
	@clang-format $^ -i

# Versions
#---------
.PHONY: release
release: CFLAGS +=$(OPT) $(WARNINGS) $(NO_ASSERTS)
release: CPPFLAGS +=$(OPT) $(WARNINGS) $(NO_ASSERTS)
release: $(TARGETS)

.PHONY: all
all: CFLAGS +=$(OPT) $(WARNINGS)
all: CPPFLAGS +=$(OPT) $(WARNINGS)
all: $(TARGETS)

.PHONY: debug
debug: CFLAGS +=$(DEBUG_SYMBOLS) $(WARNINGS)
debug: CPPFLAGS +=$(DEBUG_SYMBOLS) $(WARNINGS)
debug: $(TARGETS)


# Utilities
#==========

# Clean
#-------
.PHONY: clean
clean:
	-rm -fr $(THIS_DIR)/bin/* $(OBJ_DIR) $(THIS_DIR)/test/bin/* core.* $(TARGETS)

.PHONY: cleandep
cleandep:
	-rm -f $(DEP)
	

# Debugging
#----------
print-%:
	@echo $* = $($*)


# Testing
#===========

# Definitions
#------------
CATCH_DIR :=$(VENDOR_DIR)/catch
TEST_DIR :=$(abspath $(THIS_DIR)/test)
TEST_SUPPORT_DIR :=$(TEST_DIR)/support
INCLUDES +=-I$(CATCH_DIR) -I$(TEST_SUPPORT_DIR)
TEST_SRC_EXTENSION =.cpp
TEST_EXTENSION =.out
TEST_PREFIX =test_
TEST_EXECUTABLE_DIR :=$(TEST_DIR)/bin
TEST_SUBDIRS :=$(filter-out $(TEST_SUPPORT_DIR) $(TEST_DIR)/data,$(wildcard $(TEST_DIR)/*))
TESTS        :=$(shell find $(TEST_SUBDIRS) -type f -name '$(TEST_PREFIX)*$(TEST_SRC_EXTENSION)' 2>/dev/null)
TEST_EXES_TEMP   :=$(TESTS:%$(TEST_SRC_EXTENSION)=%$(TEST_EXTENSION))
TEST_EXES :=$(abspath $(TEST_EXES_TEMP:$(TEST_DIR)%=$(TEST_EXECUTABLE_DIR)%))
TEST_SUPPORT_SRC :=$(TEST_SUPPORT_DIR)/test_helper.cpp


# Rules
#------
$(TEST_EXECUTABLE_DIR):
	@echo [mkdir] $@
	@mkdir -p $@

$(TEST_SUPPORT_DIR):
	@echo [mkdir] $@
	@mkdir -p $@

$(TEST_SUPPORT_SRC): | $(TEST_SUPPORT_DIR)
	@echo [touch] $@
	@touch $@

T = $(abspath $(TEST_EXECUTABLE_DIR))/$(TEST_PREFIX)
$(T)%$(TEST_EXTENSION): $(TEST_DIR)/$(TEST_PREFIX)%$(TEST_SRC_EXTENSION) $(LIB_OBJ) $(TEST_SUPPORT_SRC) | $(TEST_EXECUTABLE_DIR)
	@if [ ! -d $(@D) ]; then mkdir -p $(@D); fi
	@echo [CCLD] $<
	@$(CPP) $(CPPFLAGS) $(LDFLAGS) \
		$(TEST_SUPPORT_SRC) \
		$< \
		-o $@ \
		$(INCLUDES) $(LDLIBS) $(LIB_OBJ)

.PHONY: test
test: OPT =-O1
test: CFLAGS +=$(OPT) $(WARNINGS)
test: CPPFLAGS +=$(OPT) $(WARNINGS)
test: $(TEST_EXES)
	@for test in $^; do echo ; echo [TEST] $$test; \
		echo "===============================================================\n";\
		$$test | grep -v -P '^\s*\#'; done

.PHONY: cleantest
cleantest:
	-rm -f $(TEST_EXECUTABLE_DIR)/*
