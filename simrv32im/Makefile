TARGET_DIR:=../bin
TARGET:=$(TARGET_DIR)/simrv32im

C_SRC:=simrv32im.c cpu.c debugger.c isa.c loader.c memory.c supervisor.c
CFLAGS:=-g --std=gnu99

BUILD_DIR:=build
OBJS:=$(patsubst %,$(BUILD_DIR)/%,$(C_SRC:.c=.o))
DEPS:=$(OBJS:.o=.d)

.PHONY: all
all: $(TARGET)

-include $(DEPS)

$(TARGET): $(OBJS) $(TARGET_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(OBJS): | $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET_DIR):
	mkdir -p $@

.PHONY: check
check:
	$(MAKE) -C tests

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(TARGET:=.exe)
