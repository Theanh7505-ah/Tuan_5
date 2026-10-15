#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
/* Cortex-M3 FreeRTOS port handler; do not alias it to SysTick_Handler. */
extern void xPortSysTickHandler(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

void HardFault_Handler(void)
{
    for (;;) { __asm volatile ("nop"); }
}
