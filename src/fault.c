
/* developed from post at 
 * https://community.arm.com/thread/5414 
 * https://community.arm.com/docs/DOC-7835
 */

#include <stdint.h>
#include "kinetis.h"
#include "serialize.h"

#define SCB_HFSR_FORCED ((uint32_t) 1 << 30)
#define SCB_BFSR ((uint8_t) ((SCB_CFSR & 0x0000ff00) >> 8))

/* Program Status Register (PSR) is the commbination of three registers */
#define PSR_APSR_MASK (0xf0000000)  /* Application Program Status Register */
#define PSR_IPSR_MASK (0x000000ff)  /* Interrupt Program Status Register */
#define PSR_EPSR_MASK (0x01000000)  /* Execution Program Status Register */

#define APSR_NEGATIVE   ((uint32_t) 1 << 31)
#define APSR_ZERO       ((uint32_t) 1 << 30)
#define APSR_CARRY      ((uint32_t) 1 << 29)
#define APSR_OVERFLOW   ((uint32_t) 1 << 28)

#define EPSR_THUMB      ((uint32_t) 1 << 24)

#define  BFAR (*((volatile uint32_t *) 0xE000ED38))
#define MMFAR (*((volatile uint32_t *) 0xE000ED34))
#define AFSR  (*((volatile uint32_t *) 0xE000ED3C))

/* on hard fault the cortex-m4 will push to the stack the following registers */
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} fault_state_t;

static void reportregisters(fault_state_t *regs);
static void reportbusfault(uint8_t bfsr);
// static void reportpsr(uint32_t psr);

/* r0-r3,r12,lr,pc,xpsr are pushed on the stack before calling, check if the
 * exception happened in thread mode or handler mode then set r0 to point to 
 * the registers and call `hard_fault_isr_w_registers` */
__attribute__((naked))
void hard_fault_isr(void)
{
    asm("tst lr, #4");
    asm("ite eq");
    asm("mrseq r0, msp");
    asm("mrsne r0, psp");
    asm("b hard_fault_isr_w_registers");
}

void hard_fault_isr_w_registers(fault_state_t *state) 
{
    sprintln("-- FAULT ------------------------------------------------------");
    if (SCB_HFSR & SCB_HFSR_FORCED) 
    {
        reportbusfault(SCB_BFSR);
    }
    reportregisters(state);
}


void reportregisters(fault_state_t *regs) 
{
    serial_printf("    r0 %08x", regs->r0);
    serial_printf("    r1 %08x", regs->r1);
    serial_printf("    r2 %08x", regs->r2);
    serial_printf("    r3 %08x", regs->r3);
    serial_printf("   r12 %08x", regs->r12);
    serial_printf("    lr %08x", regs->lr);
    serial_printf("    pc %08x", regs->pc);
    serial_printf("  xpsr %08x", regs->xpsr);
}


/* Table B3-17 BusFault Status Register (BFSR, 0xE000ED29) */
void reportbusfault(uint8_t bfsr) 
{
    if (bfsr) 
    {
        sprint("  BUS FAULT: ");
             if (bfsr & 0x10) {sprint("Fault on exception entry");}
        else if (bfsr & 0x08) {sprint("Fault on exception return");}
        else if (bfsr & 0x04) {sprint("Imprecise data access error");}
        else if (bfsr & 0x02) {sprint("Precise data acces error");}
        else if (bfsr & 0x01) {sprint("Fault on instruction prefetch");}
        
        /* if the Bus Fault Address (BFAR) register is valid output it */
        if (bfsr & 0x80) 
        {
            serial_printf(" BFAR(%08x)", BFAR);
        }
        sprint("\n");
    }
}

// void reportpsr(uint32_t psr) {
//     int divider = 0;
//     sprint("  APSR ");
//     if (psr & APSR_NEGATIVE) {sprint("NEGATIVE");divider=1;}
//     if (psr & APSR_ZERO) {if(divider)sprint("|");sprint("ZERO");divider=1;}
//     if (psr & APSR_CARRY) {if(divider)sprint("|");sprint("CARRY");divider=1;}
//     if (psr & APSR_OVERFLOW) {if(divider)sprint("|");sprint("OVERFLOW");divider=1;}
//     sprint("\n");
//     
//     sprint("  IPSR ");
//     if ((psr && PSR_IPSR_MASK) >= 16) {
//         sprint("External Interrupt 0x");sprinthexln((uint8_t)(psr && PSR_IPSR_MASK) - 16);
//     } else {
//         sprinthexln((uint8_t)(psr && PSR_IPSR_MASK));
//     }
//     
//     sprint("  EPSR ");if(psr&EPSR_THUMB){sprintln("THUMB");}
// }

