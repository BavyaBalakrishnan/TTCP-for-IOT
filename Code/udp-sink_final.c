/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/rpl/rpl.h"
#include "net/linkaddr.h"
#include "net/netstack.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
#include "dev/leds.h"
#if CONTIKI_TARGET_Z1
#include "dev/uart0.h"
#else
#include "dev/uart1.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "collect-common.h"
#include "collect-view.h"
#include "servreg-hack.c"
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
#ifdef UIP_CONF_DS6_DEFAULT_PREFIX
#define UIP_DS6_DEFAULT_PREFIX UIP_CONF_DS6_DEFAULT_PREFIX
#else
/* From RFC4193, section 3.1:
 *  | 7 bits |1|  40 bits   |  16 bits  |          64 bits           |
 *  +--------+-+------------+-----------+----------------------------+
 *  | Prefix |L| Global ID  | Subnet ID |        Interface ID        |
 *  +--------+-+------------+-----------+----------------------------+
 *     Prefix            FC00::/7 prefix to identify Local IPv6 unicast
 *                       addresses.
 *     L                 Set to 1 if the prefix is locally assigned.
 *                       Set to 0 may be defined in the future.  See
 *                       Section 3.2 for additional information.
 *     Global ID         40-bit global identifier used to create a
 *                       globally unique prefix.  See Section 3.2 for
 *                       additional information.
 *
 * We set prefix to 0xfc00 and set the local bit, resulting in 0xfd00.
 * For high probability of network uniqueness, Global ID must be generated
 * pseudo-randomly. As this is a hard-coded default prefix, we simply use
 * a Global ID of 0. For real deployments, make sure to install a pseudo-random
 * Global ID, e.g. in a RPL network, by configuring it at the root.
 */
#define UIP_DS6_DEFAULT_PREFIX 0xfd00
#endif /* UIP_CONF_DS6_DEFAULT_PREFIX */
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UDP_1_PORT 1234
#define UDP_2_PORT 1235
#define UDP_CLIENT_PORT 8775
#define UDP_SERVER_PORT 5688
#define SERVICE_ID 190
#define SERVICE_ID1 191
#define SERVICE_ID2 192
static struct uip_udp_conn *server_conn;
static struct etimer et,et1,et2;
  static struct etimer periodic_timer;
uip_ipaddr_t *addr,*addr1,*addr2;
static int flag=0,flag1=0;
static int flag3=0;
static struct simple_udp_connection unicast_connection;
PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process,&collect_common_process);
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  printf("Data received on port %d from port %d with length %d\n",
         receiver_port, sender_port, datalen);
}

void
collect_common_set_sink(void)
{
}
/*---------------------------------------------------------------------------*/
void
collect_common_net_print(void)
{
  printf("I am sink!\n");
}
/*---------------------------------------------------------------------------*/
void
collect_common_send(void)
{
  /* Server never sends */
}
/*---------------------------------------------------------------------------*/
void
collect_common_net_init(void)
{
#if CONTIKI_TARGET_Z1
  uart0_set_input(serial_line_input_byte);
#else
  uart1_set_input(serial_line_input_byte);
#endif
  serial_line_init();

  PRINTF("I am sink!\n");
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  uint8_t *appdata;
  linkaddr_t sender;
  uint8_t seqno;
  uint8_t hops;

  if(uip_newdata()) {
   
	
    appdata = (uint8_t *)uip_appdata;
//Rime addresses are just the last two bytes of device IEEE addresses
    sender.u8[0] = UIP_IP_BUF->srcipaddr.u8[15];
    sender.u8[1] = UIP_IP_BUF->srcipaddr.u8[14];
    seqno = *appdata;
    hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;
    collect_common_recv(&sender, seqno, hops,
                        appdata + 2, uip_datalen() - 2);
	
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  uip_ipaddr_t ipaddr;
  struct uip_ds6_addr *root_if;

  PROCESS_BEGIN();

  PROCESS_PAUSE();

  SENSORS_ACTIVATE(button_sensor);

  PRINTF("UDP server started\n");

#if UIP_CONF_ROUTER
  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
//Construct an IPv6 address from eight 16-bit words. 
  /* uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr); How the address was acquired: check whether the ADDR_MANUAL was set succefuly or not*/
  uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);//set the ip adress of server as the root of initial DAG 
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
#endif /* UIP_CONF_ROUTER */

  print_local_addresses();

  /* The data sink runs with a 100% duty cycle in order to ensure high
     packet reception rates. */
  NETSTACK_RDC.off(1);

  server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
//udp_new (const uip_ipaddr_t *ripaddr, u16_t port, void *appstate)
  udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));
//Bind a UDP connection to a local port. 
//The port number must be provided in network byte order so a conversion with UIP_HTONS() usually is necessary.

  PRINTF("Created a server connection with remote address ");
  PRINT6ADDR(&server_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
         UIP_HTONS(server_conn->rport));
  
  etimer_set(&et1, CLOCK_SECOND*600);
  etimer_set(&et2, CLOCK_SECOND*500);
  servreg_hack_init();
  //This function initializes and starts the servreg-hack application 
  simple_udp_register(&unicast_connection, UDP_1_PORT,NULL, UDP_1_PORT, receiver); 
  /*simple_udp_register (struct simple_udp_connection *c, uint16_t local_port, uip_ipaddr_t *remote_addr, uint16_t remote_port,      simple_udp_callback receive_callback)*/
  etimer_set(&periodic_timer, CLOCK_SECOND*30);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
  etimer_set(&et, CLOCK_SECOND*240);
  while(1) {
	if(flag==0){	
	//ctimer_set(&backoff_timer, SEND_TIME, send_packet, NULL);
    		addr = servreg_hack_lookup(SERVICE_ID);
    
		if(addr != NULL) {
    			//  static unsigned int message_number;
      			//char buf[20];

      			printf("Sending ON message to sensor ");
      			uip_debug_ipaddr_print(addr);
      			printf("\n");
     			// sprintf(buf, "Message %d", message_number);
    			//  message_number++;
     			// simple_udp_sendto(&unicast_connection, buf, strlen(buf) + 1, addr);
			simple_udp_sendto(&unicast_connection, "ON", 3, addr);
		}
		else {
      			printf("Service %d not found\n", SERVICE_ID);
    		}
		addr1 = servreg_hack_lookup(SERVICE_ID1);
		if(addr1 != NULL) {
			printf("Sending ON message to sensor ");
      			uip_debug_ipaddr_print(addr1);
      			printf("\n");
     			// sprintf(buf, "Message %d", message_number);
      			//message_number++;
      			//simple_udp_sendto(&unicast_connection, buf, strlen(buf) + 1, addr1);
			simple_udp_sendto(&unicast_connection, "ON", 3, addr1);
    		} else {
      			printf("Service %d not found\n", SERVICE_ID);
    		}
		flag=1;
	}
	
	//etimer_set(&et, CLOCK_SECOND*180);
	//etimer_reset(&periodic_timer);
	//PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
  	  PROCESS_YIELD();
	//This event is posted to a process whenever a uIP event has occurred.

    	if(ev == tcpip_event) {
      		tcpip_handler();
    	} else if (ev == sensors_event && data == &button_sensor) {
      		PRINTF("Initiating global repair\n");
      		rpl_repair_root(RPL_DEFAULT_INSTANCE);
    	} 
	//etimer_set(&periodic_timer, CLOCK_SECOND*3);
	//etimer_reset(&periodic_timer);
  //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
	if(etimer_expired(&et)&&flag1==0){
		printf("Sending OFF message to sensor ");
        	uip_debug_ipaddr_print(addr);
		printf("\n");
		simple_udp_sendto(&unicast_connection, "OFF", 4, addr);
		printf("Sending OFF message to sensor ");
        	uip_debug_ipaddr_print(addr1);
		printf("\n");
		simple_udp_sendto(&unicast_connection, "OFF", 4, addr1);
		flag1=1;
		addr2 = servreg_hack_lookup(SERVICE_ID2);
		if(addr2 != NULL) {
			//simple_udp_register(&unicast_connection1, UDP_2_PORT,NULL, UDP_2_PORT, receiver); 
			printf("Sending ON message to sensor ");
      			uip_debug_ipaddr_print(addr2);
      			printf("\n");
     			// sprintf(buf, "Message %d", message_number);
      			//message_number++;
      			//simple_udp_sendto(&unicast_connection, buf, strlen(buf) + 1, addr1);
			//etimer_reset(&periodic_timer);
			etimer_set(&periodic_timer, CLOCK_SECOND*15);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
			simple_udp_sendto(&unicast_connection, "ON", 3, addr2);
			

		}

	}
	if(etimer_expired(&et2)&&flag3==0)
	{
		printf("Sending OFF message to sensor ");
		uip_debug_ipaddr_print(addr2);
		printf("\n");
		etimer_reset(&periodic_timer);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
		simple_udp_sendto(&unicast_connection, "OFF", 4, addr2);
		flag3=1;
		
	}
	if(etimer_expired(&et1))
	{
		printf("Network Mission Time Elapsed \n");
	}
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
