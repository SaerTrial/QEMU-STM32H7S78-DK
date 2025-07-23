
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "trace.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "hw/misc/stm32h7rsxx_rcc.h"

static void stm32h7rsxx_rcc_reset(DeviceState *dev)
{
    STM32H7RSXXRccState *s = STM32H7RSXX_RCC(dev);

    for (int i = 0; i < STM32H7RSXX_RCC_NREGS; i++) {
        s->regs[i] = 0;
    }
}

static uint64_t stm32h7rsxx_rcc_read(void *opaque, hwaddr addr, unsigned int size)
{
    STM32H7RSXXRccState *s = STM32H7RSXX_RCC(opaque);

    uint32_t value = 0;
    if (addr > STM32H7RSXX_RCC_TESTCR) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                      __func__, addr);
    } else {
        value = s->regs[addr >> 2];
    }
    trace_stm32h7rsxx_rcc_read(addr, value);
    return value;
}

static void stm32h7rsxx_rcc_write(void *opaque, hwaddr addr,
                            uint64_t val64, unsigned int size)
{
    STM32H7RSXXRccState *s = STM32H7RSXX_RCC(opaque);
    uint32_t value = val64;
    uint32_t prev_value, new_value, irq_offset;

    trace_stm32h7rsxx_rcc_write(value, addr);

    if (addr > STM32H7RSXX_RCC_TESTCR) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                      __func__, addr);
        return;
    }

    switch (addr) {
    case STM32H7RSXX_RCC_AHB5_RSTR...STM32H7RSXX_RCC_AHB3_RSTR:
        prev_value = s->regs[addr / 4];
        s->regs[addr / 4] = value;

        irq_offset = ((addr - STM32H7RSXX_RCC_AHB5_RSTR) / 4) * 32;
        for (int i = 0; i < 32; i++) {
            new_value = extract32(value, i, 1);
            if (extract32(prev_value, i, 1) && !new_value) {
                trace_stm32h7rsxx_rcc_pulse_reset(irq_offset + i, new_value);
                qemu_set_irq(s->reset_irq[irq_offset + i], new_value);
            }
        }
        return; 
    case STM32H7RSXX_RCC_AHB5_ENR...STM32H7RSXX_RCC_AHB3_ENR:
        prev_value = s->regs[addr / 4];
        s->regs[addr / 4] = value;

        irq_offset = ((addr - STM32H7RSXX_RCC_AHB5_ENR) / 4) * 32;
        for (int i = 0; i < 32; i++) {
            new_value = extract32(value, i, 1);
            if (!extract32(prev_value, i, 1) && new_value) {
                trace_stm32h7rsxx_rcc_pulse_enable(irq_offset + i, new_value);
                qemu_set_irq(s->enable_irq[irq_offset + i], new_value);
            }
        }
        return;
    default:
        qemu_log_mask(
            LOG_UNIMP,
            "%s: The RCC peripheral only supports enable and reset in QEMU\n",
            __func__
        );
        s->regs[addr >> 2] = value;
    }
}

static const MemoryRegionOps stm32h7rsxx_rcc_ops = {
    .read = stm32h7rsxx_rcc_read,
    .write = stm32h7rsxx_rcc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32h7rsxx_rcc_init(Object *obj)
{
    STM32H7RSXXRccState *s = STM32H7RSXX_RCC(obj);

    memory_region_init_io(&s->mmio, obj, &stm32h7rsxx_rcc_ops, s,
                          TYPE_STM32H7RSXX_RCC, STM32H7RSXX_RCC_PERIPHERAL_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    qdev_init_gpio_out(DEVICE(obj), s->reset_irq, STM32H7RSXX_RCC_NIRQS);
    qdev_init_gpio_out(DEVICE(obj), s->enable_irq, STM32H7RSXX_RCC_NIRQS);

    for (int i = 0; i < STM32H7RSXX_RCC_NIRQS; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->reset_irq[i]);
        sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->enable_irq[i]);
    }
}

static const VMStateDescription vmstate_stm32h7rsxx_rcc = {
    .name = TYPE_STM32H7RSXX_RCC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, STM32H7RSXXRccState, STM32H7RSXX_RCC_NREGS),
        VMSTATE_END_OF_LIST()
    }
};

static void stm32h7rsxx_rcc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_stm32h7rsxx_rcc;
    device_class_set_legacy_reset(dc, stm32h7rsxx_rcc_reset);
}

static const TypeInfo stm32h7rsxx_rcc_info = {
    .name          = TYPE_STM32H7RSXX_RCC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32H7RSXXRccState),
    .instance_init = stm32h7rsxx_rcc_init,
    .class_init    = stm32h7rsxx_rcc_class_init,
};

static void stm32h7rsxx_rcc_register_types(void)
{
    type_register_static(&stm32h7rsxx_rcc_info);
}

type_init(stm32h7rsxx_rcc_register_types)
