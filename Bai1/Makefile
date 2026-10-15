# STM32F103C8Tx / Cortex-M3 / STM32 HAL + FreeRTOS kernel
TARGET := firmware
BUILD := build
CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

HAL := external/STM32F1xx_HAL_Driver
DEV := external/cmsis_device_f1
CMSIS := external/CMSIS_5/CMSIS/Core/Include
RTOS := external/FreeRTOS-Kernel
STARTUP := $(DEV)/Source/Templates/gcc/startup_stm32f103xb.s

INCS := Core/Inc $(HAL)/Inc $(DEV)/Include $(CMSIS) \
        $(RTOS)/include $(RTOS)/portable/GCC/ARM_CM3
CFLAGS := -mcpu=cortex-m3 -mthumb -std=gnu11 -O0 -g3 -Wall -Wextra \
          -ffunction-sections -fdata-sections -DSTM32F103xB -DUSE_HAL_DRIVER \
          -DconfigENABLE_FPU=0 -DconfigENABLE_MPU=0 \
          $(addprefix -I,$(INCS))
ASFLAGS := -mcpu=cortex-m3 -mthumb -x assembler-with-cpp -g3 $(addprefix -I,$(INCS))
LDFLAGS := -mcpu=cortex-m3 -mthumb -Tstm32f103c8.ld -Wl,--gc-sections \
           -Wl,-Map=$(BUILD)/$(TARGET).map -specs=nano.specs -specs=nosys.specs

HAL_SRCS := $(addprefix $(HAL)/Src/stm32f1xx_, \
             hal.c hal_rcc.c hal_rcc_ex.c hal_gpio.c hal_tim.c hal_tim_ex.c \
             hal_cortex.c hal_flash.c hal_flash_ex.c hal_pwr.c hal_dma.c)
RTOS_SRCS := $(addprefix $(RTOS)/, \
             tasks.c queue.c list.c portable/MemMang/heap_4.c \
             portable/GCC/ARM_CM3/port.c)
SRCS := main.c Core/Src/stm32f1xx_it.c $(DEV)/Source/Templates/system_stm32f1xx.c \
        $(HAL_SRCS) $(RTOS_SRCS)
OBJS := $(addprefix $(BUILD)/,$(SRCS:.c=.o)) $(BUILD)/$(STARTUP:.s=.o)

.PHONY: all clean deps check flash
all: check $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin

check:
	@test -f $(STARTUP) || { echo 'ERROR: dependencies missing. Run bash scripts/fetch_deps.sh'; exit 1; }
	@test -f $(RTOS)/portable/GCC/ARM_CM3/port.c || { echo 'ERROR: FreeRTOS port missing'; exit 1; }

deps:
	bash scripts/fetch_deps.sh

$(BUILD)/$(TARGET).elf: $(OBJS) stm32f103c8.ld
	$(CC) $(OBJS) $(LDFLAGS) -Wl,--start-group -lc -lm -Wl,--end-group -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

flash: all
	st-flash write $(BUILD)/$(TARGET).bin 0x08000000

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
