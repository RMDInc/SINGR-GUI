/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "lwip/err.h"
#include "lwip/tcp.h"
#include "sleep.h"
#include <xil_io.h>
#include <xil_cache.h>

#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif

struct tcp_pcb *globalTCP_pcb;	//global struct
int transferWFType = 0;			//declared global again
extern int g_menuSel;			//global menu variable

int transfer_data() {
	return 0;
}

void print_app_header()
{
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	if(g_menuSel == 9)	//if we want to transfer data, come here
	{
		int dat;
		int dram_addr = 0;
		int dram_base = 0xa000000;
		int dram_ceiling = 0xa004000;
		int len,a;
		//int i,k;
		char data[8192] = "";
		//char data2[8192];
		char buffer[6];

		a = Xil_In32 (XPAR_AXI_GPIO_11_BASEADDR);	//checks how full the bram buffer is
		if(a==4095)	//buffer is full
		{
			switch(g_mode){
			case 0: case 1: case 2:		//Any WF choice comes here
				Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + 0x48, 0xa000000);
				Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + 0x58, 65536);
				sleep(1);
				dram_base = 0xa000000;
				dram_ceiling = Xil_In32(XPAR_AXI_GPIO_20_BASEADDR) + 4 * dram_base;		//Read the value of the write pointer * 4; it gives the number of ints written
				//xil_printf("c: %d", dram_ceiling);
				g_txcomplete = 0;
				break;
			case 4:
				switch(transferWFType){
				case 0:
					dram_base = 0xa000000;
					dram_ceiling = 0xa004000;
					break;
				case 1:
					dram_base = 0xa004004;
					dram_ceiling = 0xa008004;
					break;
				case 2:
					dram_base = 0xa008008;
					dram_ceiling = 0xa00c008;
					break;
				default:
					g_txcomplete = 0;
					g_menuSel = 101;	//indicates transferWFType is messed up
					return ERR_OK;
					break;
				}
				Xil_Out32 (XPAR_AXI_GPIO_15_BASEADDR, 1);
				Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x48, 0xa000000);
				Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x58 , 65536);
				sleep(1);
				Xil_Out32 (XPAR_AXI_GPIO_15_BASEADDR, 0);

				Xil_Out32(XPAR_AXI_GPIO_9_BASEADDR,1);//reset BRAM buffers
				sleep(1);
				Xil_Out32(XPAR_AXI_GPIO_9_BASEADDR,0);//reset BRAM buffers
				break;
			default:
				g_txcomplete = 0;
				g_menuSel = 102;	//indicates mode is incorrect
				return ERR_OK;
				break;
			}

			for(dram_addr = dram_base; dram_addr <= dram_ceiling; dram_addr+=4)
			{
				dat = Xil_In32(dram_addr);
				sprintf(data, "%d", dat);
				strcat(data, "\r\n");
				len = strlen(data);

				err = tcp_write(tpcb, (void*)data, len, 0x01);
			}

			transferWFType += 1;
			if(transferWFType == 3)
				transferWFType = 0;

			return ERR_OK;
		}
	}
	else	//Otherwise come here for the polling loop
	{
		/* do not read the packet if we are not in ESTABLISHED state */
		char * buffer;//sam
		int iVal = 0;//sam

		if (!p) {
			tcp_close(tpcb);
			tcp_recv(tpcb, NULL);
			return ERR_OK;
		}

		/* indicate that the packet has been received */
		tcp_recved(tpcb, p->len);

		buffer = (char*) malloc(p->len);//sam
		memcpy(buffer,p->payload,p->len);//sam

		if((p->len) < 8)	//check that the packet is int size or less
		{
			sscanf(buffer,"%d",&iVal);	//read one int from buffer into the address of iVal
			g_menuSel = iVal;			//save the value to send back to main
		}
		else
		{
			iVal = 99999;	//indicate that we didn't read a value from the buffer
		}

		/* echo back the payload */
		/* in this case, we assume that the payload is < TCP_SND_BUF */
		if (tcp_sndbuf(tpcb) > p->len) {
			err = tcp_write(tpcb, p->payload, p->len, 1);
			//err = tcp_write(tpcb, 0x1, p->len, 1);
		} else
			xil_printf("no space in tcp_sndbuf\n\r");

		/* free the received pbuf */
		pbuf_free(p);

		return ERR_OK;
	}//eoelse
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)connection);

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}


int start_application()
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new();
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ADDR_ANY, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP echo server started @ port %d\n\r", port);

	return 0;
}
