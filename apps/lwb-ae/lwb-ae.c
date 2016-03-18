/*
 * Copyright (c) 2016, Swiss Federal Institute of Technology (ETH Zurich).
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
 *
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
 *
 * Author:  Reto Da Forno
 *          Marco Zimmerling
 */
 
/**
 * @brief Low-Power Wireless Bus Test Application
 * 
 * The burst scheduler is used in this example. It is designed for scenarios
 * where most of the time no data is transmitted, but occasionally (upon an
 * event) a node needs to send a large amount of data to the host.
 * 
 * This demo application is not designed to run on Flocklab. 
 */


#include "contiki.h"
#include "platform.h"

/*---------------------------------------------------------------------------*/
#ifdef APP_TASK_ACT_PIN
#define TASK_ACTIVE             PIN_SET(APP_TASK_ACT_PIN)
#define TASK_SUSPENDED          PIN_CLR(APP_TASK_ACT_PIN)
#else
#define TASK_ACTIVE
#define TASK_SUSPENDED
#endif /* APP_TASK_ACT_PIN */
#ifdef FLOCKLAB
#warning "--------------------- COMPILED FOR FLOCKLAB ---------------------"
#endif 
/*---------------------------------------------------------------------------*/
static uint16_t events_sent = 0;
static uint16_t acks_rcvd = 0;
/*---------------------------------------------------------------------------*/
PROCESS(app_process, "Application Task");
AUTOSTART_PROCESSES(&app_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(app_process, ev, data) 
{   
  static uint16_t pkt_buffer[(LWB_CONF_MAX_DATA_PKT_LEN + 1) / 2];
  static uint16_t round_cnt = 0;
  
  PROCESS_BEGIN();
          
  SVS_DISABLE;
  
  /* all other necessary initialization is done in contiki-cc430-main.c */
    
  /* start the LWB thread */
  lwb_start(0, &app_process);
  
  /* INIT code */
#if !defined(FLOCKLAB) && HOST_ID != NODE_ID
  /* configure port interrupt */
  PIN_CFG_INT(DEBUG_SWITCH);
  PIN_PULLUP_EN(DEBUG_SWITCH); 
#endif
  
  /* MAIN LOOP of this application task */
  while(1) {
    /* the app task should not do anything until it is explicitly granted 
     * permission (by receiving a poll event) by the LWB task */
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
    TASK_ACTIVE;      /* application task runs now */
        
    if(HOST_ID == node_id) {
      /* HOST node */
      /* print out the received data */
      static uint16_t pkt_cnt = 0;
      uint16_t cnt = 0;
      while(1) {
        uint8_t pkt_len = lwb_get_data((uint8_t*)pkt_buffer);
        if(pkt_len) {
          cnt++;
        } else {
          break;
        }
      }
      if(cnt) {
        pkt_cnt += cnt;
        DEBUG_PRINT_INFO("rcvd=%u", pkt_cnt);
      }
    } else {
      /* SOURCE node */
      uint8_t pkt_len = lwb_get_data((uint8_t*)pkt_buffer);
      if(pkt_len && *pkt_buffer == node_id) {
        acks_rcvd++;
        DEBUG_PRINT_INFO("sent=%u ack=%u", events_sent, acks_rcvd);
      } 
#ifdef FLOCKLAB
      if(round_cnt == 1 &&
         (node_id == 6 || node_id == 28 || node_id == 22)) {
        /* generate a dummy packet to 'register' this node at the host */
        lwb_put_data((uint8_t*)&node_id, 2);
        events_sent++;
      }
      //uint16_t elapsed_time = lwb_get_time(0);
      /* initiator nodes start to send data after a certain time */
      if((node_id == 6 || node_id == 28 || node_id == 22) && 
         round_cnt > 12) {      /* (elapsed_time % 20 == 0) && */
        /* generate an event */
        lwb_put_data((uint8_t*)&node_id, 2);
        events_sent++;
        DEBUG_PRINT_INFO("sent=%u ack=%u", events_sent, acks_rcvd);
      }      
#endif /* FLOCKLAB */
    }
    round_cnt++;
    
    /* IMPORTANT: This process must not run for more than a few hundred
     * milliseconds in order to enable proper operation of the LWB */
    
    /* since this process is only executed at the end of an LWB round, we 
     * can now configure the MCU for minimal power dissipation for the idle
     * period until the next round starts */
#if LWB_CONF_USE_LF_FOR_WAKEUP
  #if FRAM_CONF_ON
    fram_sleep();
  #endif /* FRAM_CONF_ON */
    /* disable all peripherals, reconfigure the GPIOs and disable XT2 */
    TA0CTL &= ~MC_3; /* stop TA0 */
    DISABLE_XT2();
  #ifdef MUX_SEL_PIN
    PIN_CLR(MUX_SEL_PIN);
  #endif /* MUX_SEL_PIN */
    P1SEL = 0; /* reconfigure GPIOs */
    P1DIR |= (BIT2 | BIT3 | BIT4 | BIT5 | BIT6 | BIT7);
    /* dont clear BIT6 and 7 on olimex board */
    P1OUT &= ~(BIT2 | BIT3 | BIT4 | BIT5); //  | BIT6 | BIT7
    /* set clock source to DCO */
    UCSCTL4 = SELA__XT1CLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;
#endif /* LWB_CONF_USE_LF_FOR_WAKEUP */
    
    TASK_SUSPENDED;
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#if !defined(FLOCKLAB) && HOST_ID != NODE_ID
ISR(PORT1, port1_interrupt) 
{
  if(PIN_IFG(DEBUG_SWITCH)) {
    if(!lwb_put_data((uint8_t*)&node_id, 2)) {
      DEBUG_PRINT_WARNING("can't queue data");
    }
    events_sent++;
    DEBUG_PRINT_INFO("event triggered");
    PIN_CLR_IFG(DEBUG_SWITCH);
  } 
}
#endif
/*---------------------------------------------------------------------------*/