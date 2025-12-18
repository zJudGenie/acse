BISON:=bison
FLEX:=flex

TARGET_DIR:=../bin
TARGET:=$(TARGET_DIR)/acse

Y_SRC:=parser.y
L_SRC:=scanner.l
C_SRC:=acse.c cfg.c codegen.c errors.c list.c program.c reg_alloc.c \
       target_asm_print.c target_info.c target_transform.c
CFLAGS:=-g --std=gnu99

BUILD_DIR:=build
BISON_OUT:=$(patsubst %,$(BUILD_DIR)/%,$(Y_SRC:.y=.tab.c))
OBJS:=$(patsubst %,$(BUILD_DIR)/%,$(Y_SRC:.y=.tab.o)) \
      $(patsubst %,$(BUILD_DIR)/%,$(L_SRC:.l=.yy.o)) \
      $(patsubst %,$(BUILD_DIR)/%,$(C_SRC:.c=.o))
DEPS:=$(OBJS:.o=.d)

.PHONY: all
all: $(TARGET)

-include $(DEPS)

$(TARGET): $(OBJS) $(TARGET_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.PRECIOUS: $(BUILD_DIR)/%.tab.c
$(BUILD_DIR)/%.tab.c: %.y
	$(BISON) $(YFLAGS) -o $@ -d $<

.PRECIOUS: $(BUILD_DIR)/%.yy.c
$(BUILD_DIR)/%.yy.c: %.l $(BISON_OUT)
	$(FLEX) $(LFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -MMD -c -o $@ $<

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c
	$(CC) $(CFLAGS) -I. -MMD -c -o $@ $<

$(OBJS): | $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(TARGET:=.exe)
