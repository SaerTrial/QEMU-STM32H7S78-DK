#ifndef HW_STM32H7RX7SX_USART_H
#define HW_STM32H7RX7SX_USART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define USART_CR1   0x00 // USART Control register 1
#define USART_CR2   0x04 // USART Control register 2
#define USART_CR3   0x08 // USART Control register 3
#define USART_BRR   0x0C // USART Baud rate register
#define USART_GTPR  0x10 // USART Guard time and prescaler register
#define USART_RTOR  0x14 // USART Receiver Time Out register
#define USART_RQR   0x18 // USART Request register
#define USART_ISR   0x1C // USART Interrupt and status register
#define USART_ICR   0x20 // USART Interrupt flag Clear register
#define USART_RDR   0x24 // USART Receive Data register
#define USART_TDR   0x28 // USART Transmit Data register
#define USART_PRESC 0x2C // USART Prescaler register


#define TYPE_STM32H7RX7SX_USART "stm32h7rx7sx-usart"
OBJECT_DECLARE_SIMPLE_TYPE(STM32H7RX7SXUsartState, STM32H7RX7SX_USART)

// FIFO mode enabled
#define USART_ISR_TXFNF     (1 << 7) 
#define USART_ISR_RXFNE     (1 << 5)


// FIFO mode disabled
#define USART_ISR_TXE       (1 << 7) 
#define USART_ISR_RXNE      (1 << 5)


#define USART_ISR_TC        (1 << 6)


#define USART_ISR_TEACK_Pos             (21U)
#define USART_ISR_TEACK_Msk             (0x1UL << USART_ISR_TEACK_Pos)          /*!< 0x00200000 */
#define USART_ISR_TEACK                 USART_ISR_TEACK_Msk

#define USART_ISR_REACK_Pos             (22U)
#define USART_ISR_REACK_Msk             (0x1UL << USART_ISR_REACK_Pos)          /*!< 0x00400000 */
#define USART_ISR_REACK                 USART_ISR_REACK_Msk                     /*!< Receive Enable Acknowledge Flag */


#define USART_CR1_UE        (1 << 0)
#define USART_CR1_RE        (1 << 2)
#define USART_CR1_TE        (1 << 3)

#define IS_9BIT_MODE(CR1)  ( (( (0x10000000 & CR1) >> 28) << 1 | ((0x1000 & CR1) >> 12)) == 0b01)
#define IS_FIFO_MODE(CR1)   ( ((CR1 >> 29) & 1) == 0b1 )


struct STM32H7RX7SXUsartState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t usart_cr1;
    uint32_t usart_cr2;
    uint32_t usart_cr3;
    uint32_t usart_brr;
    uint32_t usart_gtpr;
    uint32_t usart_rtor;
    uint32_t usart_rqr;
    uint32_t usart_isr;
    uint32_t usart_icr;
    uint32_t usart_rdr;
    uint32_t usart_tdr;
    uint32_t usart_presc;
    bool is9bitmode;

    CharBackend chr;
    qemu_irq irq;
};
#endif /* HW_STM32H7RX7SX_USART_H */
