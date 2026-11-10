/* Minimal GCC startup for the OTA bootloader (no peripheral IRQs used). */

#include <stdint.h>
#include "stm32f4xx_hal.h"

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern void SystemInit(void);
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

#define WEAK __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void)        WEAK;
void HardFault_Handler(void)  WEAK;
void MemManage_Handler(void)  WEAK;
void BusFault_Handler(void)   WEAK;
void UsageFault_Handler(void) WEAK;
void SVC_Handler(void)        WEAK;
void DebugMon_Handler(void)   WEAK;
void PendSV_Handler(void)     WEAK;

__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))((uint32_t)&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler
};

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0U;
    }

    SystemInit();
    main();

    for (;;) { }
}

void Default_Handler(void)
{
    for (;;) { }
}
