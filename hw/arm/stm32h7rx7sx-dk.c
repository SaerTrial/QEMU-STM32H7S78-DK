#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-clock.h"
#include "qemu/error-report.h"
#include "hw/arm/boot.h"
#include "hw/arm/stm32h7rx7sx_soc.h"

/* Main SYSCLK frequency in Hz (24MHz) */
#define SYSCLK_FRQ 24000000ULL

// clock, uart, cpu, nvic

static void stm32h7rx7sx_dk_init(MachineState *machine)
{
    DeviceState *dev;
    Clock *sysclk;

    /* This clock doesn't need migration because it is fixed-frequency */
    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);

    dev = qdev_new(TYPE_STM32H7RX7SX_SOC);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(STM32H7RX7SX_SOC(dev)->armv7m.cpu,
                       machine->kernel_filename,
                       0, FLASH_SIZE);
}

static void stm32h7rx7sx_dk_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m7"),
        NULL
    };

    mc->desc = "ST STM32H7RX7SX-DK (Cortex-M7)";
    mc->init = stm32h7rx7sx_dk_init;
    mc->valid_cpu_types = valid_cpu_types;
}

DEFINE_MACHINE("stm32h7rx7sx_dk", stm32h7rx7sx_dk_machine_init)
