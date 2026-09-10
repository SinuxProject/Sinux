# SMP & Scheduler Module for Sinux

OBJS += \
    $(BUILD)/kernel/scheduler/sched.o

$(BUILD)/kernel/scheduler/%.o: kernel/scheduler/%.c | $(BUILD)/kernel/scheduler
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/kernel/scheduler:
	mkdir -p $@
