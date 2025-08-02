#include "qemu/osdep.h"
#include "qemu/log.h"
#include "trace.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "hw/misc/stm32h7s7xx_pwr.h"

static void stm32h7s7xx_pwr_reset(DeviceState *dev)
{
    STM32H7S7XXPWRState *s = STM32H7S7XX_PWR(dev);

    for (int i = 0; i < STM32H7S7XX_PWR_NREGS; i++) {
        s->regs[i] = 0;
    }
}

static void stm32h7s7xx_pwr_write(void *opaque, hwaddr addr,
                            uint64_t val64, unsigned int size)
{
    STM32H7S7XXPWRState *s = STM32H7S7XX_PWR(opaque);

    uint32_t value = val64;

    if (addr > 0x44) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                        __func__, addr);
        return ;
    }

    s->regs[addr / 4] = value;
}


#define PWR_CSR2_USB33RDY_Pos          (26U)
#define PWR_CSR2_USB33RDY_Msk          (0x1UL << PWR_CSR2_USB33RDY_Pos)         /*!< 0x04000000 */
#define PWR_CSR2_USB33RDY              PWR_CSR2_USB33RDY_Msk                    /*!< USB supply ready */


static uint64_t stm32h7s7xx_pwr_read(void *opaque, hwaddr addr, unsigned int size)
{
    STM32H7S7XXPWRState *s = STM32H7S7XX_PWR(opaque);
    uint32_t value = s->regs[addr >> 2];
    
    if (addr > 0x44) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                        __func__, addr);
        return 0x0;
    }

    if (addr == STM32H7S7XX_PWR_CSR2)
        return value | PWR_CSR2_USB33RDY;
    
    return value;
}

static const MemoryRegionOps stm32h7s7xx_pwr_ops = {
    .read = stm32h7s7xx_pwr_read,
    .write = stm32h7s7xx_pwr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32h7s7xx_pwr_init(Object *obj)
{
    STM32H7S7XXPWRState *s = STM32H7S7XX_PWR(obj);

    memory_region_init_io(&s->mmio, obj, &stm32h7s7xx_pwr_ops, s,
                          TYPE_STM32H7S7XX_PWR, STM32H7S7XX_PWR_PERIPHERAL_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static const VMStateDescription vmstate_stm32h7s7xx_pwr = {
    .name = TYPE_STM32H7S7XX_PWR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, STM32H7S7XXPWRState, STM32H7S7XX_PWR_NREGS),
        VMSTATE_END_OF_LIST()
    }
};

static void stm32h7s7xx_pwr_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_stm32h7s7xx_pwr;
    device_class_set_legacy_reset(dc, stm32h7s7xx_pwr_reset);
}

static const TypeInfo stm32h7s7xx_pwr_info = {
    .name          = TYPE_STM32H7S7XX_PWR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32H7S7XXPWRState),
    .instance_init = stm32h7s7xx_pwr_init,
    .class_init    = stm32h7s7xx_pwr_class_init,
};

static void stm32h7s7xx_pwr_register_types(void)
{
    type_register_static(&stm32h7s7xx_pwr_info);
}

type_init(stm32h7s7xx_pwr_register_types)
