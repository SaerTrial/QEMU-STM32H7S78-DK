#ifndef HW_ARM_STM3H7RX7SX_SOC_H
#define HW_ARM_STM3H7RX7SX_SOC_H

#include "hw/char/stm32h7rx7sx_usart.h"
#include "hw/misc/stm32h7rsxx_rcc.h"
#include "hw/misc/stm32h7s7xx_pwr.h"
#include "hw/arm/armv7m.h"
#include "qom/object.h"
#include "hw/clock.h"


#define TYPE_STM32H7RX7SX_SOC "stm32h7rx7sx-soc"
OBJECT_DECLARE_SIMPLE_TYPE(STM32H7RX7SXSOCState, STM32H7RX7SX_SOC)

#define STM_NUM_USARTS 1

#define FLASH_BASE_ADDRESS 0x08000000
#define FLASH_SIZE (128 * 1024)
#define SRAM_BASE_ADDRESS 0x24000000
#define SRAM_SIZE (8 * 1024)

#define DTCM_BASE_ADDRESS 0x20000000
#define DTCM_SIZE (128 * 1024)

#define PWR_BASE_ADDRESS 0x58024800
#define PWR_SIZE 0x400

struct STM32H7RX7SXSOCState {
    SysBusDevice parent_obj;

    ARMv7MState armv7m;

    STM32H7RSXXRccState rcc;
    STM32H7S7XXPWRState pwr;
    STM32H7RX7SXUsartState usart[STM_NUM_USARTS];

    MemoryRegion sram;
    MemoryRegion dtcm;
    MemoryRegion flash;
    MemoryRegion flash_alias;

    Clock *sysclk;
    Clock *refclk;
};



#endif