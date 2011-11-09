/** 
* @file client.c
* @brief client network interface
* @author John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
* @version 0.1
* @date 2009-04-12
*/
// client.c

/* Copyright (C) 
* 2009 - John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
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

/* Copyright (C) - modifications of the original program by John Melton
* 2011 - Alex Lee, 9V1Al
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
* Foundation, Inc., 59 Temple Pl
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <samplerate.h>

#include "client.h"
#include "ozy.h"
#include "audiostream.h"
#include "main.h"
#include "soundcard.h"
#include "dttsp.h"
#include "buffer.h"
#include "codec2loc.h"

#include "register.h"

static int timing=0;
static struct timeb start_time;
static struct timeb end_time;

static pthread_t client_thread_id, spectrum_thread_id, tx_thread_id;

static int client_terminate=0;

#define BASE_PORT 8000

static int port=BASE_PORT;

static int serverSocket;
static int clientSocket;
static struct sockaddr_in server;
static struct sockaddr_in client;
static socklen_t addrlen;

#define SAMPLE_BUFFER_SIZE 4096
static float spectrumBuffer[SAMPLE_BUFFER_SIZE];

#define TX_BUFFER_SIZE 1024
// same as BUFFER_SIZE defined in softrock server
// format is float left_buffer[BUFFER_SIZE] followed by right_buffer[BUFFER_SIZE] non-interleaved
static float tx_buffer[TX_BUFFER_SIZE*2];

#define MIC_BUFFER_SIZE  BITS_SIZE
static unsigned char mic_buffer[MIC_BUFFER_SIZE];

//
// samplerate library data structures
//
SRC_STATE *mic_sr_state;
double mic_src_ratio;

static float meter;
static float subrx_meter;
int encoding = 0;

static sem_t network_semaphore, get_spectrum_semaphore, ready_spectrum_semaphore;
static sem_t get_mic_semaphore, ready_mic_semaphore;

void* client_thread(void* arg);
void* tx_thread(void* arg);

void client_send_samples(int size);
void client_set_samples(float* samples,int size);

char* client_samples;
int samples;
int prncountry = -1;

float getFilterSizeCalibrationOffset() {
    int size=1024; // dspBufferSize
    float i=log10((float)size);
    return 3.0f*(11.0f-i);
}


void client_init(int receiver) {
    int rc;

    sem_init(&network_semaphore,0,1);
    signal(SIGPIPE, SIG_IGN);

    fprintf(stderr,"client_init audio_buffer_size=%d\n",audio_buffer_size);

    port=BASE_PORT+receiver;
    clientSocket=-1;
    rc=pthread_create(&client_thread_id,NULL,client_thread,NULL);
    if(rc != 0) {
        fprintf(stderr,"pthread_create failed on client_thread: rc=%d\n", rc);
    }

}

void tx_init(){
    int rc;

    sem_init(&get_mic_semaphore,0,1);
    sem_init(&ready_mic_semaphore,0,1);
    signal(SIGPIPE, SIG_IGN);

	mic_src_ratio = (double) sampleRate / 8000.0;
       // create sample rate subobject
        int sr_error;
        mic_sr_state = src_new (
                             //SRC_SINC_BEST_QUALITY,  // NOT USABLE AT ALL on Atom 300 !!!!!!!
                             //SRC_SINC_MEDIUM_QUALITY,
                             SRC_SINC_FASTEST,
                             //SRC_ZERO_ORDER_HOLD,
                             //SRC_LINEAR,
                             2, &sr_error
                           ) ;

        if (mic_sr_state == 0) { 
            fprintf (stderr, "tx_init: SR INIT ERROR: %s\n", src_strerror (sr_error)); 
        } else {
            fprintf (stderr, "tx_init: sample rate init successful with ratio of : %f\n", mic_src_ratio);
	}

    rc=pthread_create(&tx_thread_id,NULL,tx_thread,NULL);
    if(rc != 0) fprintf(stderr,"pthread_create failed on tx_thread: rc=%d\n", rc);
}

void *tx_thread(void *arg){
   unsigned char bits[BITS_SIZE];
   short codec2_buffer[CODEC2_SAMPLES_PER_FRAME];
   int tx_buffer_counter = 0;
   int rc;
   int j, i;
   float data_in [CODEC2_SAMPLES_PER_FRAME*2];		// stereo
   float data_out[CODEC2_SAMPLES_PER_FRAME*2*24];	// 192khz / 8khz = 24
   SRC_DATA data;
   void *mic_codec2 = codec2_create();

    while (1){
        sem_wait(&get_mic_semaphore);
	// process codec2 encoded mic_buffer
	memcpy(bits, mic_buffer, BITS_SIZE);		// right now only one frame per buffer
	codec2_decode(mic_codec2, codec2_buffer, bits);
	// mic data is mono, so copy to both right and left channels
           for (j=0; j < CODEC2_SAMPLES_PER_FRAME; j++) {
              data_in [j*2] = data_in [j*2+1]   = (float)codec2_buffer[j]/32767.0;
           }

           data.data_in = data_in;
           data.input_frames = CODEC2_SAMPLES_PER_FRAME;

           data.data_out = data_out;
           data.output_frames = CODEC2_SAMPLES_PER_FRAME*24 ;
           data.src_ratio = mic_src_ratio;
           data.end_of_input = 0;

           rc = src_process (mic_sr_state, &data);
           if (rc) {
               fprintf (stderr,"SRATE: error: %s (rc=%d)\n", src_strerror (rc), rc);
           } else {
		for (i=0; i < data.output_frames_gen; i++){
			// tx_buffer is non-interleaved, LEFT followed by RIGHT data
			tx_buffer[tx_buffer_counter] = data_out[2*i];
			tx_buffer[tx_buffer_counter + TX_BUFFER_SIZE] = data_out[2*i+1];
			tx_buffer_counter++;
			if (tx_buffer_counter >= TX_BUFFER_SIZE){
				// send Tx IQ to server, stereo interleaved
				int bytes_written;
				bytes_written=sendto(audio_socket,tx_buffer,sizeof(tx_buffer),0,
					(struct sockaddr *)&server_audio_addr,server_audio_length);
				if(bytes_written<0) {
				   fprintf(stderr,"sendto audio failed: %d\n",bytes_written);
				   exit(1);
				}
				// fprintf(stderr,"bytes written = %d\n",bytes_written);
				tx_buffer_counter = 0;
			}
		}
	    } // end else rc

        sem_post(&ready_mic_semaphore);
	}

	codec2_destroy(mic_codec2);
}

void spectrum_init() {
    int rc;

    sem_init(&get_spectrum_semaphore,0,1);
    sem_init(&ready_spectrum_semaphore,0,1);
    sem_post(&ready_spectrum_semaphore);

    signal(SIGPIPE, SIG_IGN);

    fprintf(stderr,"spectrum_init\n");

    rc=pthread_create(&spectrum_thread_id,NULL,spectrum_thread,NULL);
    if(rc != 0) {
        fprintf(stderr,"pthread_create failed on spectrum_thread: rc=%d\n", rc);
    }

}

void *spectrum_thread(void *x) {
    while (1){
	    sem_wait(&get_spectrum_semaphore);
	    Process_Panadapter(0,spectrumBuffer);
	    meter=CalculateRXMeter(0,0,0)+multimeterCalibrationOffset+getFilterSizeCalibrationOffset();
	    subrx_meter=CalculateRXMeter(0,1,0)+multimeterCalibrationOffset+getFilterSizeCalibrationOffset();
	    client_samples=malloc(BUFFER_HEADER_SIZE+samples);
	    client_set_samples(spectrumBuffer,samples);
	    client_send_samples(samples);
	    free(client_samples);
	    sem_post(&ready_spectrum_semaphore);
    }
}


void client_set_timing() {
    timing=1;
}

void* client_thread(void* arg) {
    int rc;
    char *token;
    int i;
    int bytesRead;
    char message[64];
    int on=1;

fprintf(stderr,"client_thread\n");

    serverSocket=socket(AF_INET,SOCK_STREAM,0);
    if(serverSocket==-1) {
        perror("client socket");
        return NULL;
    }

    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&server,0,sizeof(server));
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    server.sin_port=htons(port);

    if(bind(serverSocket,(struct sockaddr *)&server,sizeof(server))<0) {
        perror("client bind");
        fprintf(stderr,"port=%d\n",port);
        return NULL;
    }

fprintf(stderr,"client_thread: listening on port %d\n",port);
    if (listen(serverSocket, 5) == -1) {
	perror("client listen");
	exit(1);
    }

    while(1) {
        addrlen = sizeof(client); 
	if ((clientSocket = accept(serverSocket,(struct sockaddr *)&client,&addrlen)) == -1) {
		perror("client accept");
	} else {

            time_t tt;
            struct tm *tod;
            time(&tt);
            tod=localtime(&tt);
            if(prncountry == 0){
                printcountry();
            }
            //fprintf(stdout,"wget -O - --post-data 'ip=%s'  http://www.selfseo.com/ip_to_country.php 2>/dev/null |grep ' is assigned to '|perl -ne 'm/border=1> (.*?)</; print \"$1\\n\" '\n",inet_ntoa(client.sin_addr));
            fflush(stdout);
            fprintf(stderr,"%02d/%02d/%02d %02d:%02d:%02d RX%d: client connection from %s:%d\n",tod->tm_mday,tod->tm_mon+1,tod->tm_year+1900,tod->tm_hour,tod->tm_min,tod->tm_sec,receiver,inet_ntoa(client.sin_addr),ntohs(client.sin_port));


            // set timeout on receive
            struct timeval tv;
            tv.tv_sec=10;		// changed from 3 sec to 10 sec for remote connection
            tv.tv_usec=0;
            rc=setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,(char *)&tv,sizeof tv);

            client_terminate=0;
if(timing) ftime(&start_time);
            while(client_terminate==0) {
                bytesRead=recv(clientSocket, message, sizeof(message), 0);
                if(bytesRead==0) {
                    break;
                }
         
                if(bytesRead<0) {
                    if(errno!=EWOULDBLOCK) {
                        continue;
                    }
                    break;
                }
                message[bytesRead-1]=0;			// for Linux strings terminating in NULL

		if(timing) {
		    ftime(&end_time);
		    fprintf(stderr,"%s\n",message);
		    fprintf(stderr,"command after %ld ms\n",\
		    ((end_time.time*1000)+end_time.millitm)-((start_time.time*1000)+start_time.millitm));
		    ftime(&start_time);
		}
                token=strtok(message," ");
 
                    if(strcmp(token,"mic")==0){		// This is incoming microphone data

			/*
			int rc = sem_trywait(&ready_mic_semaphore);
			if (rc == 0){
			memcpy(mic_buffer, &message[4], MIC_BUFFER_SIZE);
			sem_post(&get_mic_semaphore);
			} else fprintf(stderr,"mic data processing blocking...\n");
			*/

			sem_wait(&ready_mic_semaphore);				// this will block
			memcpy(mic_buffer, &message[4], MIC_BUFFER_SIZE);
			sem_post(&get_mic_semaphore);
		    }
                    else {
                    	if(token!=NULL) {
		            	i=0;
		            	while(token[i]!=0) {
		               	token[i]=tolower(token[i]);
		               	i++;
                    		}
 			if(strcmp(token,"getspectrum")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
			    int rc = sem_trywait(&ready_spectrum_semaphore);
			    if (rc == 0){
                            	samples=atoi(token);
 			    	sem_post(&get_spectrum_semaphore);		// unblocks the get_spectrum thread
			    } else fprintf(stderr,"spectrum processing blocking...\n");
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                    	    }
                    	} else if(strcmp(token,"setfrequency")==0) {
                        long long frequency;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            frequency=atoll(token);
                            ozySetFrequency(frequency);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        	}
                    	} else if(strcmp(token,"setpreamp")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            ozySetPreamp(token);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    	} else if(strcmp(token,"setmode")==0) {
		                int mode;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    mode=atoi(token);
		                    SetMode(0,0,mode);
		                    SetMode(0,1,mode);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"setfilter")==0) {
		                int low,high;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    low=atoi(token);
		                    token=strtok(NULL," ");
		                    if(token!=NULL) {
		                      high=atoi(token);
		                      SetRXFilter(0,0,(double)low,(double)high);
		                      SetRXFilter(0,1,(double)low,(double)high);
		                    } else {
		                        fprintf(stderr,"Invalid command: '%s'\n",message);
		                    }
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                            }
                    	} else if(strcmp(token,"setagc")==0) {
		                int agc;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    agc=atoi(token);
		                    SetRXAGC(0,0,agc);
		                    SetRXAGC(0,1,agc);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"setnr")==0) {
		                int nr;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    if(strcmp(token,"true")==0) {
		                        nr=1;
		                    } else {
		                        nr=0;
		                    }
		                    SetNR(0,0,nr);
		                    SetNR(0,1,nr);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                   	} else if(strcmp(token,"setnb")==0) {
		                int nb;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    if(strcmp(token,"true")==0) {
		                        nb=1;
		                    } else {
		                        nb=0;
		                    }
		                    SetNB(0,0,nb);
		                    SetNB(0,1,nb);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"setsdrom")==0) {
		                int state;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    if(strcmp(token,"true")==0) {
		                        state=1;
		                    } else {
		                        state=0;
		                    }
		                    SetSDROM(0,0,state);
		                    SetSDROM(0,1,state);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"setanf")==0) {
		                int anf;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    if(strcmp(token,"true")==0) {
		                        anf=1;
		                    } else {
		                        anf=0;
		                    }
		                    SetANF(0,0,anf);
		                    SetANF(0,1,anf);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"setrxoutputgain")==0) {
		                int gain;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    gain=atoi(token);
		                    SetRXOutputGain(0,0,(double)gain/100.0);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"setsubrxoutputgain")==0) {
		                int gain;
		                token=strtok(NULL," ");
		                if(token!=NULL) {
		                    gain=atoi(token);
		                    SetRXOutputGain(0,1,(double)gain/100.0);
		                } else {
		                    fprintf(stderr,"Invalid command: '%s'\n",message);
		                }
                    	} else if(strcmp(token,"startaudiostream")==0) {
		                token=strtok(NULL," ");
		                if(token==NULL) {
		                    audio_buffer_size= AUDIO_BUFFER_SIZE;
		                } else {
		                    audio_buffer_size=atoi(token);
		                }
		                token=strtok(NULL," ");
		                if(token==NULL) {
		                    audio_sample_rate=8000;
		                } else {
		                    audio_sample_rate=atoi(token);
		                    if(audio_sample_rate!=8000 &&
		                       audio_sample_rate!=48000) {
		                        fprintf(stderr,"Invalid audio sample rate: %d\n",audio_sample_rate);
		                        audio_sample_rate=8000;
		                    }
		                }
		                token=strtok(NULL," ");
		                if(token==NULL) {
		                    audio_channels=1;
                        } else {
                            audio_channels=atoi(token);
                            if(audio_channels!=1 &&
                               audio_channels!=2) {
                                fprintf(stderr,"Invalid audio channels: %d\n",audio_channels);
                                audio_channels=1;
                            }
                        }
                        
			fprintf(stderr,"starting audio stream at %d with %d channels and buffer size %d\n",audio_sample_rate,audio_channels,audio_buffer_size);
			fprintf(stderr,"and with encoding method %d\n", encoding);
 
            		updateStatus("Busy"); 

                        audio_stream_reset();
                        send_audio=1;
                    } else if(strcmp(token,"stopaudiostream")==0) {
                        send_audio=0;
                    } else if(strcmp(token,"setencoding")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            encoding=atoi(token);
			    if (encoding < 0 || encoding > 2) encoding = 0;
			    fprintf(stderr,"encoding changed to %d\n", encoding);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsubrx")==0) {
                        int state;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            state=atoi(token);
                            SetSubRXSt(0,1,state);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsubrxfrequency")==0) {
                        int offset;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            offset=atoi(token);
                            SetRXOsc(0,1,offset - LO_offset);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setpan")==0) {
                        float pan;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            pan=atof(token);
                            SetRXPan(0,0,pan);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsubrxpan")==0) {
                        float pan;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            pan=atof(token);
                            SetRXPan(0,1,pan);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"record")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            ozySetRecord(token);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setanfvals")==0) {
                        int taps;
                        int delay;
                        double gain;
                        double leakage;
                        int error;

                        error=0;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            taps=atoi(token);
                        } else {
                            error=1;
                        }
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            delay=atoi(token);
                        } else {
                            error=1;
                        }
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            gain=atof(token);
                        } else {
                            error=1;
                        }
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            leakage=atof(token);
                        } else {
                            error=1;
                        }
                        if(error) {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        } else {
                            SetANFvals(0,0,taps,delay,gain,leakage);
                            SetANFvals(0,1,taps,delay,gain,leakage);
                        }
                    } else if(strcmp(token,"setnrvals")==0) {
                        int taps;
                        int delay;
                        double gain;
                        double leakage;
                        int error;

                        error=0;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            taps=atoi(token);
                        } else {
                            error=1;
                        }
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            delay=atoi(token);
                        } else {
                            error=1;
                        }
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            gain=atof(token);
                        } else {
                            error=1;
                        }
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            leakage=atof(token);
                        } else {
                            error=1;
                        }
                        if(error) {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        } else {
                            SetNRvals(0,0,taps,delay,gain,leakage);
                            SetNRvals(0,1,taps,delay,gain,leakage);
                        }
                    } else if(strcmp(token,"setnbvals")==0) {
                        double threshold;
                        int error;

                        error=0;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            threshold=atof(token);
                        } else {
                            error=1;
                        }
                        if(error) {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        } else {
                            SetNBvals(0,0,threshold);
                            SetNBvals(0,1,threshold);
                        }
                    } else if(strcmp(token,"setsdromvals")==0) {
                        double threshold;
                        int error;

                        error=0;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            threshold=atof(token);
                        } else {
                            error=1;
                        }
                        if(error) {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        } else {
                            SetSDROMvals(0,0,threshold);
                            SetSDROMvals(0,1,threshold);
                        }
                    } else if(strcmp(token,"setdcblock")==0) {
                        int state;
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            state=atoi(token);
                            SetRXDCBlock(0,0,state);
                            SetRXDCBlock(0,1,state);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"mox")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            if(strcmp(token,"on")==0) {
                                ozySetMox(1);
                            } else if(strcmp(token,"off")==0) {
                                ozySetMox(0);
                            } else {
                                fprintf(stderr,"Invalid command: '%s'\n",message);
                            }
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsquelchval")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            float value=atof(token);
                            SetSquelchVal(0,0,value);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsubrxquelchval")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            float value=atof(token);
                            SetSquelchVal(0,1,value);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsquelchstate")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            if(strcmp(token,"on")==0) {
                                SetSquelchState(0,0,1);
                            } else if(strcmp(token,"off")==0) {
                                SetSquelchState(0,0,0);
                            } else {
                                fprintf(stderr,"Invalid command: '%s'\n",message);
                            }
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setsubrxquelchstate")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            if(strcmp(token,"on")==0) {
                                SetSquelchState(0,1,1);
                            } else if(strcmp(token,"off")==0) {
                                SetSquelchState(0,1,0);
                            } else {
                                fprintf(stderr,"Invalid command: '%s'\n",message);
                            }
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setspectrumpolyphase")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            if(strcmp(token,"true")==0) {
                                SetSpectrumPolyphase(0,1);
                            } else if(strcmp(token,"false")==0) {
                                SetSpectrumPolyphase(0,0);
                            } else {
                                fprintf(stderr,"Invalid command: '%s'\n",message);
                            }
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else if(strcmp(token,"setocoutputs")==0) {
                        token=strtok(NULL," ");
                        if(token!=NULL) {
                            ozySetOpenCollectorOutputs(token);
                        } else {
                            fprintf(stderr,"Invalid command: '%s'\n",message);
                        }
                    } else {
                        fprintf(stderr,"Invalid command: token: '%s'\n",token);
                    	}
                } else {
                    fprintf(stderr,"Invalid command: message: '%s'\n",message);
                	}
		}
if(timing) {
    ftime(&end_time);
    fprintf(stderr,"command processing %ld ms\n",((end_time.time*1000)+end_time.millitm)-((start_time.time*1000)+start_time.millitm));
    ftime(&start_time);
}
            }

            close(clientSocket);
            time(&tt);
            tod=localtime(&tt);
            fprintf(stderr,"%02d/%02d/%02d %02d:%02d:%02d RX%d: client disconnected from %s:%d\n",tod->tm_mday,tod->tm_mon+1,tod->tm_year+1900,tod->tm_hour,tod->tm_min,tod->tm_sec,receiver,inet_ntoa(client.sin_addr),ntohs(client.sin_port));
            updateStatus("Idle"); 
        }
        send_audio=0;
        clientSocket=-1;
//fprintf(stderr,"client disconnected send_audio=%d\n",send_audio);

    }
}

void client_send_samples(int size) {
    int rc;
    if(clientSocket!=-1) {
        sem_wait(&network_semaphore);
            rc=send(clientSocket,client_samples,size+BUFFER_HEADER_SIZE,MSG_NOSIGNAL);
            if(rc<0) {
                // perror("client_send_samples failed");
            }
        sem_post(&network_semaphore);
    } else {
        fprintf(stderr,"client_send_samples: clientSocket==-1\n");
    }
}

void client_send_audio() {
    int rc;
    int audio_buffer_length;

        if(clientSocket!=-1) {
            sem_wait(&network_semaphore);
            if(send_audio && (clientSocket!=-1)) {
	    	if (encoding == 1) audio_buffer_length = audio_buffer_size*audio_channels*2;
	   	else if (encoding == 0) audio_buffer_length = audio_buffer_size*audio_channels;
		else audio_buffer_length = BITS_SIZE*NO_CODEC2_FRAMES;
                rc=send(clientSocket,audio_buffer, audio_buffer_length+AUDIO_BUFFER_HEADER_SIZE,MSG_NOSIGNAL);
                if(rc!=(audio_buffer_length+AUDIO_BUFFER_HEADER_SIZE)) {
                    fprintf(stderr,"client_send_audio sent %d bytes",rc);
                    }
                }
            sem_post(&network_semaphore);
        } else {
            //fprintf(stderr,"client_send_audio: clientSocket==-1\n");
        }
}

void client_set_samples(float* samples,int size) {
    int i,j;
    float slope;
    float max;
    int lindex,rindex;

    // first byte is the buffer type
    client_samples[0]=SPECTRUM_BUFFER;
    sprintf(&client_samples[1],"%f",HEADER_VERSION);

    // next 6 bytes contain the main rx s meter
    sprintf(&client_samples[14],"%d",(int)meter);

    // next 6 bytes contain the subrx s meter
    sprintf(&client_samples[20],"%d",(int)subrx_meter);

    // next 6 bytes contain data length
    sprintf(&client_samples[26],"%d",size);

    // next 8 bytes contain the sample rate
    sprintf(&client_samples[32],"%d",sampleRate);

    // next 8 bytes contain the meter - for compatability
    sprintf(&client_samples[40],"%d",(int)meter);

    slope=(float)SAMPLE_BUFFER_SIZE/(float)size;
    for(i=0;i<size;i++) {
        max=-10000.0F;
        lindex=(int)floor((float)i*slope);
        rindex=(int)floor(((float)i*slope)+slope);
        if(rindex>SAMPLE_BUFFER_SIZE) rindex=SAMPLE_BUFFER_SIZE;
        for(j=lindex;j<rindex;j++) {
            if(samples[j]>max) max=samples[j];
        }
        client_samples[i+BUFFER_HEADER_SIZE]=(unsigned char)-(max+displayCalibrationOffset+preampOffset);
    }

}


void setprintcountry()
{
	prncountry = 0;
	fprintf(stderr,"Country Lookup is On\n");
}

void printcountry(){
	pthread_t lookup_thread;
    int t_ret1;
    t_ret1 = pthread_create( &lookup_thread, NULL, printcountrythread, (void*) NULL);
		
}

void printcountrythread()
{
  // looks for the country for the connecting IP
  FILE *fp;
  int status;
  char path[1035];
  char sCmd[255];
  /* Open the command for reading. */
  sprintf(sCmd,"wget -O - --post-data 'ip=%s'  http://www.selfseo.com/ip_to_country.php 2>/dev/null |grep ' is assigned to '|perl -ne 'm/border=1> (.*?)</; print \"$1\\n\" '",inet_ntoa(client.sin_addr));
  fp = popen(sCmd, "r");
  if (fp == NULL) {
    fprintf(stdout,"Failed to run printcountry command\n" );
    return;
  }

  /* Read the output a line at a time - output it. */
  fprintf(stdout,"\nIP %s is from ", inet_ntoa(client.sin_addr));
  while (fgets(path, sizeof(path)-1, fp) != NULL) {
    fprintf(stdout,"%s",  path);
  }

  /* close */
  pclose(fp);

  return;
}

