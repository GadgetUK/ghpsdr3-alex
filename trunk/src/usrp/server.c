/**
* @file server.c
* @brief USRP server application
* @author John Melton, G0ORX/N6LYT, code for USRP: Andrea Montefusco IW0HDV, Alberto Trentadue IZ0CEZ
* @version 0.1
* @date 2009-10-13
*/


/* Copyright (C)
* 2009 - John Melton, G0ORX/N6LYT
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <getopt.h>
#include <unistd.h>     // needed for sleep()
#else // Windows
#include "pthread.h"
#include <winsock.h>
#endif

#include "client.h"
#include "listener.h"
#include "receiver.h"
#include "bandscope.h"
#include "usrp.h"
#include "usrp_audio.h"

//Server Default I/Q rate on RX
#define DEFAULT_CLIENT_RX_SAMPLE_RATE 48000

static struct option long_options[] = {    
    {"samplerate",required_argument, 0, 0},
    {"subdev",required_argument, 0, 1},
    {"receivers",required_argument, 0, 2},    
	{"audio-to",required_argument, 0, 3},
	{"reverse-iq",no_argument, 0, 4},
	{"help",no_argument, 0, 5},

    {0,0,0,0},
};
static const char* short_options="s:d:";
static int option_index;

//Parameters holders
static char subdev_par[10] = "";    
static int rx_client_rate_par = DEFAULT_CLIENT_RX_SAMPLE_RATE;
static int receivers_par = 1;
static char audio_to[5] = "none";
static int reverse_iq = 0;

void process_args(int argc,char* argv[]);
void set_defaults(void);

//MAIN
int main(int argc,char* argv[]) {

#ifndef __linux__
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;

        wVersionRequested = MAKEWORD(1, 1);
        err = WSAStartup(wVersionRequested, &wsaData);          // initialize Windows sockets
#endif
    
    process_args(argc,argv);    

    if (!usrp_init (subdev_par)) {
		fprintf(stderr,"Failed USRP init or not found. Exiting.\n");
		exit(1);
	};
	usrp_set_receivers(receivers_par);
    usrp_set_client_rx_rate(rx_client_rate_par);
	usrp_set_server_audio(audio_to);
	usrp_set_swap_iq(reverse_iq);

    init_receivers();  //receiver
    init_bandscope();  //bandscope

	//Client listener thread start
    create_listener_thread(); 

    while(1) {
#ifdef __linux__
        sleep(10);
#else   // Windows
        Sleep(10);
#endif
    }
	fprintf(stderr,"Exiting the MAIN starter loop.\n");
}

void process_args(int argc,char* argv[]) {
    int i;

    while((i=getopt_long(argc,argv,short_options,long_options,&option_index))!=EOF) {
        
        switch(i) {
            
            case 0: ;
            case 115:// RX sample rate
                //Note: if the argumenr is a string, is evaluated to 0
                rx_client_rate_par = atoi(optarg);  	     
                break;
                                
            case 1: ;
            case 100: // subdev                
                strcpy(subdev_par, optarg);                
                break;    

            case 2: // receivers
                //Note: if the argumenr is a string, is evaluated to 0
                receivers_par = atoi(optarg);
                break;
				
			case 3: // with-audio: enables the server side audio
                strncpy(audio_to, optarg, 4);
                break;
				
			case 4: // reverse-iq: swaps i stream with q stream
                reverse_iq = 1;
                break;
        
            case 5:
            default:
                fprintf(stderr,"Usage: \n");
                fprintf(stderr,"  usrp_server -s, --samplerate 48000 | 96000 | 192000 (default 48000)\n");
                fprintf(stderr,"              -d, --subdev spec (default \"\")\n");                
                fprintf(stderr,"              --receivers N (default 1)\n");
				fprintf(stderr,"              --audio-to usrp | card | none (default none)\n");
				fprintf(stderr,"              --reverse-iq\n");
				fprintf(stderr,"              --help\n");
                fprintf(stderr,"\n");
                fprintf(stderr,"NOTE: samplerate is towards the client (e.g. dspserver)\n");

                exit(1);
                break;
               
        }
    }
}

