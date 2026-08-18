LIB_PTYTTY := $(shell find /usr/lib /usr/local/lib /lib -name "libptytty.so*" -o -name "libptytty.a" 2>/dev/null | head -n1)
LIB_VTERM := $(shell find /usr/lib /usr/local/lib /lib -name "libvterm.so*" -o -name "libvterm.a" 2>/dev/null | head -n1)
ifeq ($(LIB_PTYTTY),)
$(info could not find libptytty; you can install it by)
$(info sudo apt install libptytty-dev)
$(info or)
$(info sudo dnf install libptytty-devel)
$(info or)
$(info yay -S libptytty)
$(error build aborted)
endif
ifeq ($(LIB_VTERM),)
$(info could not find libvterm; you can install it by)
$(info sudo apt install libvterm-dev)
$(info or)
$(info sudo dnf install libvterm-devel)
$(info or)
$(info yay -S libvterm)
$(error build aborted)
endif

CXX     := gcc
RM      := rm -rf
MKDIR   := mkdir -p

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

TARGET_NAME := vtemu

CXXFLAGS := -Wall -Wextra -O2 -g -fPIE
LDFLAGS  := -Wl,-z,relro,-z,now -pie
LDLIBS   := -lptytty -lvterm

TARGET := $(BIN_DIR)/$(TARGET_NAME)
HELP_TXT := help.txt
HELP_SRC := $(OBJ_DIR)/help_msg.c
HELP_OBJ := $(OBJ_DIR)/help_msg.o
HDRS   := $(shell find $(SRC_DIR)/ -type f -name "*.h")
INCS   := $(sort $(dir $(shell find src/ -type f -name "*.h")))
SRCS   := $(shell find $(SRC_DIR)/ -type f -name "*.c")
OBJS   := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS)) $(HELP_OBJ)

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $(addprefix -I, $(INCS)) -o $@ $^ $(LDLIBS)

$(HELP_SRC): $(HELP_TXT)
	@mkdir -p $(dir $@)
	awk 'BEGIN { print "char help_msg[] =" } { gsub(/\\/, "\\\\"); gsub(/"/, "\\\""); print "    \"" $$0 "\\n\"" } END { print ";" }' $(HELP_TXT) > $@

$(HELP_OBJ): $(HELP_SRC) $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $(addprefix -I, $(INCS)) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $(addprefix -I, $(INCS)) -o $@ $<

DEPS := $(OBJS:.o=.d)
-include $(DEPS)

clean:
	$(RM) $(OBJ_DIR)

run: all
	$(TARGET)

debug: CXXFLAGS += --DDEBUG -ggdb3 -O0
debug: $(TARGET)
