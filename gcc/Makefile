# GCC build for the OTA bootloader (no FreeRTOS, HAL only).
# The application firmware is still built with Keil.

TOOLCHAIN ?= arm-none-eabi-
CC      := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE    := $(TOOLCHAIN)size

BUILD := build
TARGET := $(BUILD)/Bootloader

ROOT := ..

HAL := $(ROOT)/Drivers/STM32F4xx_HAL_Driver/Src

SRCS := \
    startup_stm32f411xe_gcc.c \
    $(ROOT)/bootloader/boot_main.c \
    $(ROOT)/Core/Src/system_stm32f4xx.c \
    $(HAL)/stm32f4xx_hal.c \
    $(HAL)/stm32f4xx_hal_cortex.c \
    $(HAL)/stm32f4xx_hal_rcc.c \
    $(HAL)/stm32f4xx_hal_rcc_ex.c \
    $(HAL)/stm32f4xx_hal_gpio.c \
    $(HAL)/stm32f4xx_hal_uart.c \
    $(HAL)/stm32f4xx_hal_flash.c \
    $(HAL)/stm32f4xx_hal_flash_ex.c \
    $(HAL)/stm32f4xx_hal_pwr.c \
    $(HAL)/stm32f4xx_hal_pwr_ex.c \
    $(HAL)/stm32f4xx_hal_dma.c \
    $(HAL)/stm32f4xx_hal_dma_ex.c

INCLUDES := \
    -I. \
    -I$(ROOT)/Core/Inc \
    -I$(HAL)/../Inc \
    -I$(HAL)/../Inc/Legacy \
    -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
    -I$(ROOT)/Drivers/CMSIS/Include

DEFINES := -DSTM32F411xE -DUSE_HAL_DRIVER -DVECT_TAB_OFFSET=0x00000000U

CFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
          -std=gnu11 -Os -g -Wall -ffunction-sections -fdata-sections \
          $(DEFINES) $(INCLUDES)

LDSCRIPT := boot.ld
LDFLAGS  := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
            -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(TARGET).map \
            -specs=nano.specs -specs=nosys.specs

.PHONY: all clean

all: $(TARGET).hex

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET).elf: $(SRCS) $(LDSCRIPT) | $(BUILD)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
	$(OBJCOPY) -O binary $< $(TARGET).bin

clean:
	rm -rf $(BUILD)
