/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup cc26xx-uart
 * @{
 *
 * \file
 * Implementation of the CC13xx/CC26xx UART driver.
 */
/*---------------------------------------------------------------------------*/
#define BOARD_CONF_DEBUGGER_DEVPACK     1
#include "board.h"
#include "cc26xx-uart.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "sys_ctrl.h"
#include "prcm.h"
#include "ioc.h"
#include "uart.h"
#include "ti-lib.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
/* Which events to trigger a UART interrupt */
#define CC26XX_UART_RX_INTERRUPT_TRIGGERS (UART_INT_RX | UART_INT_RT)

/* All interrupt masks */
#define CC26XX_UART_INTERRUPT_ALL (UART_INT_OE | UART_INT_BE | UART_INT_PE | \
                                   UART_INT_FE | UART_INT_RT | UART_INT_TX | \
                                   UART_INT_RX | UART_INT_CTS)
/*---------------------------------------------------------------------------*/
static int (*input_handler)(unsigned char c);
/*---------------------------------------------------------------------------*/
static bool
usable_rx(void)
{
  if(BOARD_IOID_UART_RX == IOID_UNUSED) {
    return false;
  }

  return true;
}
/*---------------------------------------------------------------------------*/
static bool
usable_tx(void)
{
  if(BOARD_IOID_UART_TX == IOID_UNUSED) {
    return false;
  }

  return true;
}
/*---------------------------------------------------------------------------*/
static void
power_and_clock(void)
{
  /* Power on the SERIAL PD */
  ti_lib_prcm_power_domain_on(PRCM_DOMAIN_SERIAL);
  while(ti_lib_prcm_power_domain_status(PRCM_DOMAIN_SERIAL)
        != PRCM_DOMAIN_POWER_ON);

  /* Enable UART clock in active mode */
  ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_UART0);
  ti_lib_prcm_load_set();
  while(!ti_lib_prcm_load_get());
}
/*---------------------------------------------------------------------------*/
/*
 * Returns 0 if either the SERIAL PD is off, or the PD is on but the run mode
 * clock is gated. If this function would return 0, accessing UART registers
 * will return a precise bus fault. If this function returns 1, it is safe to
 * access UART registers.
 *
 * This function only checks the 'run mode' clock gate, since it can only ever
 * be called with the MCU in run mode.
 */
static bool
accessible(void)
{
  /* First, check the PD */
  if(ti_lib_prcm_power_domain_status(PRCM_DOMAIN_SERIAL)
     != PRCM_DOMAIN_POWER_ON) {
    return false;
  }

  /* Then check the 'run mode' clock gate */
  if(!(HWREG(PRCM_BASE + PRCM_O_UARTCLKGR) & PRCM_UARTCLKGR_CLK_EN)) {
    return false;
  }

  return true;
}
/*---------------------------------------------------------------------------*/
static void
disable_interrupts(void)
{
  /* Acknowledge UART interrupts */
  ti_lib_int_disable(INT_UART0_COMB);

  /* Disable all UART module interrupts */
  ti_lib_uart_int_disable(UART0_BASE, CC26XX_UART_INTERRUPT_ALL);

  /* Clear all UART interrupts */
  ti_lib_uart_int_clear(UART0_BASE, CC26XX_UART_INTERRUPT_ALL);
}
/*---------------------------------------------------------------------------*/
static void
enable_interrupts(void)
{
  /* Clear all UART interrupts */
  ti_lib_uart_int_clear(UART0_BASE, CC26XX_UART_INTERRUPT_ALL);

  /* Enable RX-related interrupts only if we have an input handler */
  if(input_handler) {
    /* Configure which interrupts to generate: FIFO level or after RX timeout */
    ti_lib_uart_int_enable(UART0_BASE, CC26XX_UART_RX_INTERRUPT_TRIGGERS);

    /* Acknowledge UART interrupts */
    ti_lib_int_enable(INT_UART0_COMB);
  }
}
/*---------------------------------------------------------------------------*/
static void
configure(void)
{
  uint32_t ctl_val = UART_CTL_UARTEN | UART_CTL_TXE;
  /*
   * Make sure the TX pin is output / high before assigning it to UART control
   * to avoid falling edge glitches
   */
  ti_lib_ioc_pin_type_gpio_output(BOARD_IOID_UART_TX);
  ti_lib_gpio_set_dio(BOARD_IOID_UART_TX);

  /*
   * Map UART signals to the correct GPIO pins and configure them as
   * hardware controlled.
   */
  ti_lib_ioc_pin_type_uart(UART0_BASE, BOARD_IOID_UART_RX, BOARD_IOID_UART_TX,
                           BOARD_IOID_UART_CTS, BOARD_IOID_UART_RTS);

  /* Configure the UART for 115,200, 8-N-1 operation. */
  ti_lib_uart_config_set_exp_clk(UART0_BASE, ti_lib_sys_ctrl_clock_get(),
                                 115200,
                                 (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                                  UART_CONFIG_PAR_NONE));

  /*
   * Generate an RX interrupt at FIFO 1/2 full.
   * We don't really care about the TX interrupt
   */
  ti_lib_uart_fifo_level_set(UART0_BASE, UART_FIFO_TX7_8, UART_FIFO_RX4_8);

  /* Enable FIFOs */
  HWREG(UART0_BASE + UART_O_LCRH) |= UART_LCRH_FEN;

  if(input_handler) {
    ctl_val += UART_CTL_RXE;
  }

  /* Enable TX, RX (conditionally), and the UART. */
  HWREG(UART0_BASE + UART_O_CTL) = ctl_val;
}
/*---------------------------------------------------------------------------*/
static void
enable(void)
{
  power_and_clock();

  /* Make sure the peripheral is disabled */
  ti_lib_uart_disable(UART0_BASE);

  /* Disable all UART interrupts and clear all flags */
  disable_interrupts();

  /* Setup pins, Baud rate and FIFO levels */
  configure();

  /* Enable UART interrupts */
  enable_interrupts();
}
/*---------------------------------------------------------------------------*/
void
cc26xx_uart_init()
{
  // bool interrupts_disabled;

  // /* Return early if disabled by user conf or if ports are misconfigured */
  // if(!usable_rx() && !usable_tx()) {
  //   return;
  // }

  // /* Disable Interrupts */
  // interrupts_disabled = ti_lib_int_master_disable();

  // /* Only TX and EN to start with. RX will be enabled only if needed */
  // input_handler = NULL;

  // /*
  //  * init() won't actually fire up the UART. We turn it on only when (and if)
  //  * it gets requested, either to enable input or to send out a character
  //  *
  //  * Thus, we simply re-enable processor interrupts here
  //  */
  // if(!interrupts_disabled) {
  //   ti_lib_int_master_enable();
  // }
}
/*---------------------------------------------------------------------------*/
void
cc26xx_uart_write_byte(uint8_t c)
{
  /* Return early if disabled by user conf or if ports are misconfigured */
  if(usable_tx() == false) {
    return;
  }

  if(accessible() == false) {
    enable();
  }

  ti_lib_uart_char_put(UART0_BASE, c);
}
/*---------------------------------------------------------------------------*/
uint8_t
cc26xx_uart_busy(void)
{
  /* Return early if disabled by user conf or if ports are misconfigured */
  if(usable_tx() == false) {
    return UART_IDLE;
  }

  /* If the UART is not accessible, it is not busy */
  if(accessible() == false) {
    return UART_IDLE;
  }

  return ti_lib_uart_busy(UART0_BASE);
}
/*---------------------------------------------------------------------------*/
/** @} */
