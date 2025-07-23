#ifndef HW_STM32H7S7XX_PWR_H
#define HW_STM32H7S7XX_PWR_H

#include "hw/sysbus.h"
#include "qom/object.h"


#define STM32H7S7XX_PWR_NREGS ((0x44 >> 2) + 1)
#define STM32H7S7XX_PWR_PERIPHERAL_SIZE 0x400

#define STM32H7S7XX_PWR_CSR2 0x0C

#define TYPE_STM32H7S7XX_PWR "stm32h7s7xx.pwr"

typedef struct STM32H7S7XXPWRState STM32H7S7XXPWRState;

DECLARE_INSTANCE_CHECKER(STM32H7S7XXPWRState, STM32H7S7XX_PWR, TYPE_STM32H7S7XX_PWR)


struct STM32H7S7XXPWRState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t regs[STM32H7S7XX_PWR_NREGS];
};

#endif /* HW_STM32H7S7XX_PWR_H */