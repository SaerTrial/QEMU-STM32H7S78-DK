#include "qemu/osdep.h"
#include "hw/char/stm32h7rx7sx_usart.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

#include "trace.h"

// hack: due to stm32h7rx7sx implementing a FIFO buffer, 
// we do not wanna complicate emulation and simply reduce this buffer as only storing one element

static void stm32h7rx7sx_update_irq(STM32H7RX7SXUsartState *s)
{
    uint32_t mask = s->usart_isr & s->usart_cr1;

    if (mask & (USART_ISR_TXFNF | USART_ISR_TC | USART_ISR_RXFNE)) {
        // an ISR usually reads data in data register into a buffer for later access by peripheral or MCU
        qemu_set_irq(s->irq, 1);
    } else {
        qemu_set_irq(s->irq, 0);
    }
}

static int stm32h7rx7sx_usart_can_receive(void *opaque)
{
    STM32H7RX7SXUsartState *s = opaque;

    // as mentioned before, the FIFO buffer only stores one element
    // data is only accpeted when RX is empty
    if (!(s->usart_isr & USART_ISR_RXFNE)) {
        return 1;
    }
    return 0;
}

static void stm32h7rx7sx_usart_receive(void *opaque, const uint8_t *buf, int size)
{
    STM32H7RX7SXUsartState *s = opaque;
    // DeviceState *d = DEVICE(s);

    if (!(s->usart_cr1 & USART_CR1_UE && s->usart_cr1 & USART_CR1_RE)) {
         /* USART not enabled - drop the chars */
    //     trace_stm32f2xx_usart_drop(d->id);
         return;
    }

    s->usart_rdr = *buf;
    s->usart_isr |= USART_ISR_RXFNE;

    stm32h7rx7sx_update_irq(s);

    // trace_stm32f2xx_usart_receive(d->id, *buf);
}


static uint64_t stm32h7rx7sx_usart_read(void *opaque, hwaddr addr,
                                       unsigned int size)
{
    STM32H7RX7SXUsartState *s = opaque;
    // DeviceState *d = DEVICE(s);
    uint64_t retvalue = 0;

    switch (addr) {
    case USART_ISR:
        if(IS_FIFO_MODE(s->usart_cr1)) {
            s->usart_isr |= USART_ISR_TXFNF;
        }
        else {
            s->usart_isr |= USART_ISR_TXE;
        }
        retvalue = s->usart_isr;
        s->usart_isr &= ~USART_ISR_TC;
        // make receive available
        s->usart_isr |= USART_ISR_RXFNE;
        break;
    case USART_RDR:
        srand(time(NULL));
        retvalue = (rand()%256) & 0xFF;
        // retvalue = s->usart_rdr & 0xFF;
        if(IS_FIFO_MODE(s->usart_cr1)){
            s->usart_isr &= ~USART_ISR_RXFNE;
        }
        else{
            s->usart_isr &= ~USART_ISR_RXNE;
        }
        // qemu_chr_fe_accept_input(&s->chr);
        break;
    case USART_BRR:
        retvalue = s->usart_brr;
        break;
    case USART_CR1:     
        retvalue = s->usart_cr1;
        break;
    case USART_CR2:
        retvalue = s->usart_cr2;
        break;
    case USART_CR3:
        retvalue = s->usart_cr3;
        break;
    case USART_GTPR:
        retvalue = s->usart_gtpr;
        break;
    case USART_PRESC:
        retvalue = s->usart_presc;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        retvalue = 0;
    }

    // trace_stm32h7rx7sx_usart_read(d->id, size, addr, retvalue);

    return retvalue;
}

static void stm32h7rx7sx_usart_write(void *opaque, hwaddr addr,
                                  uint64_t val64, unsigned int size)
{
    STM32H7RX7SXUsartState *s = opaque;
    // DeviceState *d = DEVICE(s);
    uint32_t value = val64;
    unsigned char ch;

    // trace_stm32h7rx7sx_usart_write(d->id, size, addr, val64);
    switch (addr) {
    case USART_CR1:
        s->usart_cr1 = value;
        return;
    case USART_CR2:
        s->usart_cr2 = value;
        return;
    case USART_CR3:
        s->usart_cr3 = value;
        return;
    case USART_BRR:
        s->usart_brr = value;
        return;
    case USART_GTPR:
        s->usart_gtpr = value;
        return;
    case USART_RTOR:
        s->usart_rtor = value;
        return;   
    case USART_RQR:
        s->usart_rqr = value;
        return;
    case USART_ISR:
        s->usart_isr &= value;
        return;
    case USART_ICR:
        s->usart_icr = value;
    case USART_RDR:
        return;
    case USART_TDR:
        ch = value;

        if(IS_FIFO_MODE(s->usart_cr1)) {
            s->usart_isr &= (~USART_ISR_TXFNF);
        }
        else {
            s->usart_isr &= (~USART_ISR_TXE);
        }

        if (IS_9BIT_MODE(s->usart_cr1)) {
            qemu_chr_fe_write_all(&s->chr, &ch, 2);
            qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: uart char: 0x%x\n", __func__, ch);
        }
        else {
            qemu_chr_fe_write_all(&s->chr, &ch, 1);
                        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: uart char: %c\n", __func__, ch);
        }

        s->usart_isr |= USART_ISR_TC;
        return;
    case USART_PRESC:
        s->usart_presc = value;
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    }
}

static void stm32h7rx7sx_usart_reset(DeviceState *dev)
{
    STM32H7RX7SXUsartState *s = STM32H7RX7SX_USART(dev);
    
    // do hardware setup things
    s->usart_isr = USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_TXE | USART_ISR_TC;
    s->usart_icr = 0x0;
    s->usart_tdr = 0x00000000;
    s->usart_rdr = 0x00000000;
    s->usart_brr = 0x00000000;
    s->usart_cr1 = 0x00000000;
    s->usart_cr2 = 0x00000000;
    s->usart_cr3 = 0x00000000;
    s->usart_gtpr = 0x00000000;
    s->usart_presc = 0x0;

    stm32h7rx7sx_update_irq(s);
}


static const MemoryRegionOps stm32h7rx7sx_usart_ops = {
    .read = stm32h7rx7sx_usart_read,
    .write = stm32h7rx7sx_usart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const Property stm32h7rx7sx_usart_properties[] = {
    DEFINE_PROP_CHR("chardev", STM32H7RX7SXUsartState, chr),
};

static void stm32h7rx7sx_usart_init(Object *obj)
{
    STM32H7RX7SXUsartState *s = STM32H7RX7SX_USART(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio, obj, &stm32h7rx7sx_usart_ops, s,
                          TYPE_STM32H7RX7SX_USART, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void stm32h7rx7sx_usart_realize(DeviceState *dev, Error **errp)
{
    STM32H7RX7SXUsartState *s = STM32H7RX7SX_USART(dev);

    qemu_chr_fe_set_handlers(&s->chr, stm32h7rx7sx_usart_can_receive,
                             stm32h7rx7sx_usart_receive, NULL, NULL,
                             s, NULL, true);
}

static void stm32h7rx7sx_usart_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, stm32h7rx7sx_usart_reset);
    device_class_set_props(dc, stm32h7rx7sx_usart_properties);
    dc->realize = stm32h7rx7sx_usart_realize;
}

static const TypeInfo stm32h7rx7sx_usart_info = {
    .name          = TYPE_STM32H7RX7SX_USART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32H7RX7SXUsartState),
    .instance_init = stm32h7rx7sx_usart_init,
    .class_init    = stm32h7rx7sx_usart_class_init,
};

static void stm32h7rx7sx_usart_register_types(void)
{
    type_register_static(&stm32h7rx7sx_usart_info);
}

type_init(stm32h7rx7sx_usart_register_types)
