OBJS += \
    $(BUILD)/kernel/test/ktest.o      \
    $(BUILD)/kernel/test/test_pmm.o   \
    $(BUILD)/kernel/test/test_vfs.o

$(BUILD)/kernel/test/%.o: kernel/test/%.c | $(BUILD)/kernel/test
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/kernel/test:
	mkdir -p $@
