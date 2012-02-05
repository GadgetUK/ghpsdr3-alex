/**
* @file client.c
* @brief Handle client connection
* @author John Melton, G0ORX/N6LYT
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
#include <sys/types.h>
#include <string.h>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#else
#include <winsock.h>
#include "pthread.h"
#endif

#include "client.h"
#include "receiver.h"
#include "messages.h"
#include "bandscope.h"
#include "usrp.h"
#include "usrp_audio.h"
#include "transmitter.h"

short audio_port=AUDIO_PORT; // = 15000 as constant

const char* parse_command(CLIENT* client,char* command);
void* audio_thread(void* arg);

void* client_thread(void* arg) {
    CLIENT* client=(CLIENT*)arg;
    char command[80];
    int bytes_read;
    const char* response;

    client->receiver_state=RECEIVER_DETACHED;
    client->transmitter_state=TRANSMITTER_DETACHED;
    client->receiver=-1;
    client->mox=0;

    while(1) {	
        bytes_read=recv(client->socket,command,sizeof(command),0);
        if(bytes_read<=0) {
            break;
        }
        command[bytes_read]=0;
        response=parse_command(client,command);
        send(client->socket,response,strlen(response),0);

        fprintf(stderr,"Response to DSP client (Rx%d): '%s'\n",client->receiver,response);		
    }
	fprintf(stderr,"Exiting DSP client thread loop (Rx%d)...\n",client->receiver);

    if(client->receiver_state==RECEIVER_ATTACHED) {
        receiver[client->receiver].client=(CLIENT*)NULL;
        client->receiver_state=RECEIVER_DETACHED;
    }

    if(client->transmitter_state==TRANSMITTER_ATTACHED) {
        client->transmitter_state=TRANSMITTER_DETACHED;
    }

    client->mox=0;
    client->bs_port=-1;
    detach_bandscope(client);

#ifdef __linux__
    close(client->socket);
#else
    closesocket(client->socket);
#endif

    fprintf(stderr,"Client Handler: Client disconnected: %s:%d\n",inet_ntoa(client->address.sin_addr),ntohs(client->address.sin_port));

    free(client);
    return 0;
}

const char* parse_command(CLIENT* client,char* command) {
    
    char* token;

    fprintf(stderr,"parse_command(Rx%d): '%s'\n",client->receiver,command);

    token=strtok(command," \r\n");
    if(token!=NULL) {
        if(strcmp(token,"attach")==0) {
			//COMMAND: 'attach <side>'            
            token=strtok(NULL," \r\n");
            if(token!=NULL) {
                if(strcmp(token,"tx")==0) {
					//COMMAND: 'attach tx'
                    return attach_transmitter(client);
                } else {
					//COMMAND: 'attach rx#'
                    int rx=atoi(token);
                    return attach_receiver(rx,client);
                }
            } else {
                return INVALID_COMMAND;
            }
        } else if(strcmp(token,"detach")==0) {
            //COMMAND: 'detach <side>' 
            token=strtok(NULL," \r\n");
            if(token!=NULL) {
                int rx=atoi(token);
                return detach_receiver(rx,client);
            } else {
                return INVALID_COMMAND;
            }
        } else if(strcmp(token,"frequency")==0) {
            //COMMAND: 'frequency <long freq>'
            token=strtok(NULL," \r\n");
            if(token!=NULL) {
               long f=atol(token);
               return set_frequency(client,f);
            } else {
                return INVALID_COMMAND;
            }
        } else if(strcmp(token,"start")==0) {
			//COMMAND: 'start <stream> <port#>'
            token=strtok(NULL," \r\n");
            if(token!=NULL) {
                if(strcmp(token,"iq")==0) {
					//COMMAND: 'start iq <port#>'
                    token=strtok(NULL," \r\n");
                    if(token!=NULL) {
                        client->iq_port=atoi(token);
                    }

                    // starts the thread to acquire USRP samples
                    usrp_start (&receiver[client->receiver]);
                    fprintf(stderr,"Started USRP for RX%d\n",client->receiver);

                    //Start the audio thread 
                    if(pthread_create(&receiver[client->receiver].audio_thread_id,NULL,audio_thread,client)!=0) {
                        fprintf(stderr,"Failed to create audio thread for rx %d\n",client->receiver);
                        exit(1);
                    }
                    
                    return OK;
                } else if(strcmp(token,"bandscope")==0) {
					//COMMAND: 'start bandscope <port#>'
                    token=strtok(NULL," \r\n");
                    if(token!=NULL) {
                        client->bs_port=atoi(token);
                    }
                    attach_bandscope(client);
                    return OK;
                } else {
                    // invalid command string
                    return INVALID_COMMAND;
                }
            } else {
                // invalid command string
                return INVALID_COMMAND;
            }
        } else if(strcmp(token,"stop")==0) {
			//COMMAND: 'stop <stream>'
            token=strtok(NULL," \r\n");
            if(token!=NULL) {
                if(strcmp(token,"iq")==0) {
					//COMMAND: 'stop iq'
                    token=strtok(NULL," \r\n");
                    if(token!=NULL) {
                        client->iq_port=-1;
                    }
                    return OK;
                } else if(strcmp(token,"bandscope")==0) {
					//COMMAND: 'stop bandscope'
                    client->bs_port=-1;
                    detach_bandscope(client);
                } else {
                    // invalid command string
                    return INVALID_COMMAND;
                }
            } else {
                // invalid command string
                return INVALID_COMMAND;
            }
        } else if(strcmp(token,"preamp")==0) {
            return NOT_IMPLEMENTED_COMMAND;
        } else if(strcmp(token,"record")==0) {
            return NOT_IMPLEMENTED_COMMAND;
        } else if(strcmp(token,"mox")==0) {
            return NOT_IMPLEMENTED_COMMAND;
        } else if(strcmp(token,"ocoutput")==0) {
            return NOT_IMPLEMENTED_COMMAND;
        } else {
            // invalid command string
            return INVALID_COMMAND;
        }
    } else {
        // empty command string
        return INVALID_COMMAND;
    }
    return INVALID_COMMAND;
}

/* 
 * The audio thread, consuming audio transferred by the dspserver.
 * 
 * The audio can be used to modulate the carrier or can be diverted to
 * the local audio card (for testing purposes), depending on the
 * command line option.
 */ 
void* audio_thread(void* arg) {
    CLIENT *client=(CLIENT*)arg;
    RECEIVER *rx=&receiver[client->receiver];
    struct sockaddr_in audio;
    int audio_length;
    int old_state, old_type;
    int bytes_read;
    int on=1;

    fprintf(stderr,"audio_thread port=%d\n",audio_port+(rx->id*2));

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&old_state);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,&old_type);

    rx->audio_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(rx->audio_socket<0) {
        perror("Client Handler: create socket failed for server audio socket");
        exit(1);
    }

    setsockopt(rx->audio_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    audio_length=sizeof(audio);
    memset(&audio,0,audio_length);
    audio.sin_family=AF_INET;
    audio.sin_addr.s_addr=htonl(INADDR_ANY);
    audio.sin_port=htons(audio_port+(rx->id*2));

    if(bind(rx->audio_socket,(struct sockaddr*)&audio,audio_length)<0) {
        perror("Client Handler: bind socket failed for server audio socket");
        exit(1);
    }

    fprintf(stderr,"Listening for rx %d audio on port %d\n",rx->id,audio_port+(rx->id*2));

    while(1) {
        // get audio from a client
        bytes_read=recvfrom(rx->audio_socket,
                            rx->output_buffer,
                            sizeof(rx->output_buffer),
                            0,
                            (struct sockaddr*)&audio,
                            (socklen_t*)&audio_length
        );
        //fprintf(stderr,"Audio recv by client: %d bytes\n",bytes_read);
        if(bytes_read<0) {
            perror("Client Handler: recvfrom socket failed for audio buffer");
            exit(1);
        }

        usrp_process_audio_buffer(rx->output_buffer,&rx->output_buffer[BUFFER_SIZE], client->mox);			

		
    }
}
