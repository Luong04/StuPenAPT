TARGET_EXEC ?= aapt
BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
LIBSRC_DIRS ?= lib/include/
SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := ./include $(LIBSRC_DIRS)
INC_FLAGS := $(addprefix -I,$(INC_DIRS)) 

CFLAGS ?= $(INC_FLAGS) -std=c11 -fPIC -O2   -Wall  -pedantic  -Wextra
LDFLAGS ?= -L./ -lpthread -lm -ldl -lyaae -lconnection_cache  -Wl,-rpath,./,-fuse-ld=gold 
#LDFLAGS ?= -lpthread -lm -ldl -shared -fsanitize=address 

$(TARGET_EXEC): $(OBJS)
	cp lib/libyaae.so .
	cp lib/libconnection_cache.so .
	$(CC) ${LDFLAGS} -o $@ $^

$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@  

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)
	rm $(TARGET_EXEC)

-include $(DEPS)

MKDIR_P ?= mkdir -p

