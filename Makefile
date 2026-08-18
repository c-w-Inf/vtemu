LIB_PTYTTY := $(shell find /usr/lib /usr/local/lib /lib -name "libptytty.so*" -o -name "libptytty.a" 2>/dev/null | head -n1)
ifeq ($(LIB_PTYTTY),)
$(info could not find libptytty; you can install it by)
$(info sudo apt install libptytty-dev)
$(info or)
$(info sudo dnf install libptytty-devel)
$(info or)
$(info yay -S libptytty)
$(error build aborted)
endif

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

.PHONY: lib_all lib_clean
LIB_INC_DIR := thirdparty/libvterm/include thirdparty/libringbuf
LIB_OBJS    := $(patsubst %.c,%.o,$(shell find thirdparty/libvterm/ thirdparty/libringbuf -type f -name "*.c"))
lib_all:
	$(MAKE) -C thirdparty/libvterm all
	$(MAKE) -C thirdparty/libringbuf all
lib_clean:
	$(MAKE) -C thirdparty/libvterm clean
	$(MAKE) -C thirdparty/libringbuf clean

TARGET_NAME := vtemu

CXXFLAGS := -Wall -Wextra -O2 -g -fPIE
LDFLAGS  := -Wl,-z,relro,-z,now -pie
LDLIBS   := -lptytty

TARGET := $(BIN_DIR)/$(TARGET_NAME)
HDRS   := $(shell find $(SRC_DIR)/ -type f -name "*.h")
INCS   := $(sort $(dir $(shell find src/ $(LIB_INC_DIR)/ -type f -name "*.h")))
SRCS   := $(shell find $(SRC_DIR)/ -type f -name "*.c")
OBJS   := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean debug

all: lib_all $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(addprefix -I, $(INCS)) -o $@ $^ $(LIB_OBJS) $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) -MMD -MP -c $(addprefix -I, $(INCS)) -o $@ $<

DEPS := $(OBJS:.o=.d)
-include $(DEPS)

clean: lib_clean
	@rm -rf $(OBJ_DIR)

debug: CXXFLAGS += --DDEBUG -ggdb3 -O0
debug: $(TARGET)
