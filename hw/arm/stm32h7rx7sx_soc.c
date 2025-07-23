#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/arm/stm32h7rx7sx_soc.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"
#include "system/system.h"

static const uint32_t usart_addr[STM_NUM_USARTS] = { 0x40004C00 };
#define RCC_ADDR                       0x58024400
// STM32H7Rx/7Sx Arm<Sup>®</Sup>-based 32-bit MCUs - Reference manual
// page 863
static const int usart_irq[STM_NUM_USARTS] = {85};

static void stm32h7rx7sx_soc_initfn(Object *obj)
{
    STM32H7RX7SXSOCState *s = STM32H7RX7SX_SOC(obj);
    int i;

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);
    object_initialize_child(obj, "rcc", &s->rcc, TYPE_STM32H7RSXX_RCC);
    object_initialize_child(obj, "pwr", &s->pwr, TYPE_STM32H7S7XX_PWR);

    for (i = 0; i < STM_NUM_USARTS; i++) {
        object_initialize_child(obj, "usart[*]", &s->usart[i],
                                TYPE_STM32H7RX7SX_USART);
    }

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);
}


static void stm32h7rx7sx_soc_realize(DeviceState *dev_soc, Error **errp)
{
    STM32H7RX7SXSOCState *s = STM32H7RX7SX_SOC(dev_soc);
    DeviceState *dev, *armv7m;
    SysBusDevice *busdev;
    int i;

    MemoryRegion *system_memory = get_system_memory();

    if (clock_has_source(s->refclk)) {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
    }

    if (!clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    clock_set_mul_div(s->refclk, 8, 1);
    clock_set_source(s->refclk, s->sysclk);

    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "stm32h7rx7sx.flash",
                           FLASH_SIZE, &error_fatal);
    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc),
                             "stm32h7rx7sx.flash.alias", &s->flash, 0, FLASH_SIZE);
    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, &s->flash);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    memory_region_init_ram(&s->sram, NULL, "stm32h7rx7sx.sram", SRAM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory, SRAM_BASE_ADDRESS, &s->sram);
    
    memory_region_init_ram(&s->dtcm, NULL, "stm32h7rx7sx.dtcm", DTCM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory, DTCM_BASE_ADDRESS, &s->dtcm);


    /* Init ARMv7m */
    armv7m = DEVICE(&s->armv7m);

    // page 866
    qdev_prop_set_uint32(armv7m, "num-irq", 156);
    qdev_prop_set_uint8(armv7m, "num-prio-bits", 4);
    qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m7"));
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }


    /* Reset and clock controller */
    dev = DEVICE(&s->rcc);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->rcc), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, RCC_ADDR);
    

    /*Power controller*/
    dev = DEVICE(&s->pwr);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->pwr), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, PWR_BASE_ADDRESS);


    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < STM_NUM_USARTS; i++) {
        dev = DEVICE(&(s->usart[i]));
        // -chardev stdio,id=char0
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->usart[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, usart_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, usart_irq[i]));
    }

    /* It is necessary to create dummy devices to avoid unassigned memory accesses */
    create_unimplemented_device("GPIOA",       0x58020000, 0x400);
    create_unimplemented_device("GPIOB",       0x58020400, 0x400);
    create_unimplemented_device("GPIOC",       0x58020800, 0x400);
    create_unimplemented_device("GPIOD",       0x58020C00, 0x400);
    create_unimplemented_device("GPIOE",       0x58021000, 0x400);
    create_unimplemented_device("GPIOF",       0x58021400, 0x400);
    create_unimplemented_device("GPIOG",       0x58021800, 0x400);
    create_unimplemented_device("GPIOH",       0x58021C00, 0x400);
    create_unimplemented_device("GPIOM",       0x58023000, 0x400);
    create_unimplemented_device("GPION",       0x58023800, 0x400);
    create_unimplemented_device("GPIOO",       0x58023800, 0x400);
    create_unimplemented_device("GPIOP",       0x58023C00, 0x400);

}

static void stm32h7rx7sx_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32h7rx7sx_soc_realize;
    /* No vmstate or reset required: device has no internal state */
}

static const TypeInfo stm32h7rx7sx_soc_info = {
    .name          = TYPE_STM32H7RX7SX_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32H7RX7SXSOCState),
    .instance_init = stm32h7rx7sx_soc_initfn,
    .class_init    = stm32h7rx7sx_soc_class_init,
};

static void stm32h7rx7sx_soc_types(void)
{
    type_register_static(&stm32h7rx7sx_soc_info);
}

type_init(stm32h7rx7sx_soc_types)


