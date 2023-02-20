# system libraries to include
LIBS := raylib m

# final executable name
EXEC_NAME := rag

# where "all" target looks for source files
SRC_DIRS := ./src

# directories to pass as -I to the compiler
INC_DIRS := ./include

# compiler and linker flags
CFLAGS += -Wall -Wextra -Werror -std=c17 $(addprefix -I,$(INC_DIRS))
LDFLAGS += $(addprefix -l,$(LIBS))

# configurable build profile
DEBUG ?= 1

# configure build profile
ifeq ($(DEBUG), 1)
	CFLAGS += -g -Og -fsanitize=undefined
	LDFLAGS += -fsanitize=undefined
else
	CFLAGS += -O3 -flto
endif

# where build files live
BUILD_DIR := ./build
EXEC_FILE := $(BUILD_DIR)/$(EXEC_NAME)

# what build files are
SRCS := $(shell find $(SRC_DIRS) -type f -name '*.c')
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# compile targets
$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(dir $@)"
	@echo "... Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(EXEC_FILE): $(OBJS)
	@mkdir -p "$(dir $@)"
	@echo "... Linking $(EXEC_FILE)"
	$(CC) $(LDFLAGS) $^ -o $@

#
# top-level targets
#
.PHONY: all run clean

# build executable
all: $(OBJS) $(EXEC_FILE)

# run executable
run: $(EXEC_FILE)
	@$(EXEC_FILE)

# remove build files
clean:
	@rm -rf $(BUILD_DIR)
