/*
 * xbee_processing_thread.c
 *
 * xbee data processing thread which pulls the bytes out of the message queue 
 * (put in by the uart 1 irq handler) and uses the xbee packet parser state 
 * machine to turn them into complete packets 
 *
 * author:    Alex Shenfield
 * date:      08/11/2017
 * purpose:   55-604481 embedded computer networks : lab 104
 */

// include the relevant header files (from the c standard libraries)
#include <stdio.h>
#include <string.h>

// include the rtos api
#include "cmsis_os.h"

// include the serial configuration files
#include "vcom_serial.h"
#include "xbee.h"

// include the xbee packet parser
#include "xbee_packet_parser.h"

// include main.h with the mail type declaration
#include "main.h"

// RTOS DEFINES

// declare the thread function prototypes, thread id, and priority
void xbee_rx_thread(void const *argument);
osThreadId tid_xbee_rx_thread;
osThreadDef(xbee_rx_thread, osPriorityAboveNormal, 1, 0);

void action_thread(void const *argument);
osThreadId tid_action_thread;
osThreadDef(action_thread, osPriorityNormal, 1, 0);

// setup a message queue to use for receiving characters from the interrupt
// callback
osMessageQDef(message_q, 128, uint8_t);
osMessageQId msg_q;

// set up the mail queues
osMailQDef(mail_box, 16, mail_t);
osMailQId  mail_box;

// Semaphores & Mutexes
osMutexDef(address_mutex);
osMutexId(address_val_mutex);
osMutexDef(sensor_mutex);
osMutexId(sensor_val_mutex);
osMutexDef(armed_mutex);
osMutexId(armed_state_mutex);

// process packet function
void process_packet(uint8_t* packet, int length);

// STRUCT & VARIABLE DEFINES


struct Room {
	uint8_t id;
	uint16_t myAddress;
	uint32_t slAddress;
	uint8_t upperHeatThreshold;
	uint8_t lowerHeatThreshold;
	uint8_t lightThreshold;
	uint8_t lightPwm;
	uint8_t	lightOverride;
	uint8_t	heatingOverride;
	uint8_t acOverride;
	uint8_t	lightLevel;
	uint8_t	tempLevel;
	uint8_t	pirLevel;
	uint8_t	prevPirLevel;
	uint8_t changeCheck[2];
	
}node[2];

//Add funciton to pull copy of arrSize in timer ?
int arrSize = sizeof(node) / sizeof(node[0]);
int armedState = 0;
// THREAD INITIALISATION

// create the uart thread(s)
int init_xbee_threads(void)
{
	// print a status message to the vcom port
	init_uart(9600);
	printf("we are alive!\r\n");

	// create the message queue
	msg_q = osMessageCreate(osMessageQ(message_q), NULL);

	// create the mailbox
	mail_box = osMailCreate(osMailQ(mail_box), NULL);

	//create mutexes
	sensor_val_mutex = osMutexCreate(osMutex(sensor_mutex));
	address_val_mutex = osMutexCreate(osMutex(address_mutex));
	armed_state_mutex = osMutexCreate(osMutex(armed_mutex));

	// create the threads and get their task id
	tid_xbee_rx_thread = osThreadCreate(osThread(xbee_rx_thread), NULL);
	tid_action_thread = osThreadCreate(osThread(action_thread), NULL);

	// check if everything worked ...
	if(!tid_xbee_rx_thread)
	{
		printf("xbee thread not created!\r\n");
		return(-1);
	}
	if(!tid_action_thread){
		printf("action thread not created!\r\n");
		return(-1);
	}
	
	return(0);
}

// ACTUAL THREADS

// xbee receive thread
void xbee_rx_thread(void const *argument)
{
	// print some status message ...
	printf("xbee rx thread running!\r\n");


	//Itterate through struct and assign defaults
	
	for(int i = 0; i < arrSize; i++){
		node[i].id = i;
		node[i].slAddress = 0;
		node[i].myAddress = 0;
		node[i].upperHeatThreshold = 24;
		node[i].lowerHeatThreshold = 10;
		node[i].lightThreshold = 40;
		node[i].lightPwm = 255;
		node[i].heatingOverride = 0;
		node[i].lightOverride = 0;
		node[i].acOverride = 0;
		node[i].lightLevel = 0;
		node[i].tempLevel = 0;
		node[i].pirLevel = 0;
		node[i].prevPirLevel = 0;
		node[i].changeCheck[0] = 0;
		node[i].changeCheck[1] = 0;
	}	
	
	// infinite loop ...
	while(1)
	{
		// wait for there to be something in the message queue
		osEvent evt = osMessageGet(msg_q, osWaitForever);
		
		// process the message queue ...
		if(evt.status == osEventMessage)
		{
			// get the message and increment the counter
			uint8_t byte = evt.value.v;
			
			// feed the packet 1 byte at a time to the xbee packet parser
			int len = xbee_parse_packet(byte);
			
			// if len > 0 then we have a complete packet so dump it to the virtual 
			// com port
			if(len > 0)
			{
				printf(">> packet received\r\n");
				
				// get the packet
				uint8_t packet[len];
				get_packet(packet);
				
				// display the packet
				int i = 0;
				for(i = 0; i < len; i++)
				{
					printf("%02X ", packet[i]);
				}
				printf("\r\n");
				
				//Packet Processing
				
				//Packet is MY command implying new node
				if(packet[15] == 0x4D && packet[16] == 0x59){
				/*MUTEX HERE?*/
					
					static int i;
					uint32_t slAddress = packet[9];
					
					//itterate to gain SL address
					for(int j = 10; j < 13; j++){
						slAddress = slAddress << 8;
						slAddress = slAddress | packet[j];
					}
					node[i].slAddress = slAddress; 
					
					uint16_t myAddress = packet[18];
					myAddress = myAddress << 8;
					myAddress = myAddress | packet[19]; 
					node[i].myAddress = myAddress;
					printf("Node %d SL Address = %08X\r\n", i, node[i].slAddress);
					printf("Node %d Network Address = %04X\r\n", i, node[i].myAddress);
					i++;
					
				/*RELEASE MUTEX HERE?*/
				}
				
				//IS packet processing
				if(packet[15] == 0x49 && packet[16] == 0x53){
					uint16_t address = packet[13];
					address = address << 8;
					address = address | packet[14]; 
					
					/*MUTEX HERE?*/
					int i = 0;
					for (i = 0; i < arrSize; i++){
						if (address == node[i].myAddress){
							break;
						}
					}
					
					/*ANOTHER MUTEX HERE?*/
					
					//Set PIR val
					node[i].prevPirLevel = node[i].pirLevel;
					node[i].pirLevel = packet[23] >> 3;
					
					//Set light val
					//Mask
					uint16_t adcVal = packet[24];
					adcVal = adcVal << 8;
					adcVal = adcVal | packet[25];
					// remap (x - in min) * (out max - out min) / (in max - in min) + out min
					adcVal = (adcVal - 168.0) * (100 - 1) / (880 - 168) + 1;
					node[i].lightLevel = adcVal;
					printf("light level = %d\r\n", node[i].lightLevel);
					
					//set temp val
					int16_t tempVal = packet[26];
					tempVal = tempVal << 8;
					tempVal = tempVal | packet[27];
					tempVal = tempVal * (1200.0 / 1023.0);
					tempVal = (tempVal - 500.0) / 10.0;
					node[i].tempLevel = tempVal;
					printf("temp level = %d\r\n", node[i].tempLevel);
					/*Add logic for Armed state here?*/
					mail_t* varMail = (mail_t*) osMailAlloc(mail_box, osWaitForever);
					
					//Used to stop constant sending of turn light off in vacant state
					varMail->slAddress = node[i].slAddress;
					varMail->myAddress = node[i].myAddress;
					//Determine lights
					if(node[i].lightOverride == 0){
												
						//Someone has entered or left
						if(node[i].prevPirLevel != node[i].pirLevel){
							//Someone has left
							if(node[i].pirLevel == 0){
								printf("Someone has left\r\n");
								varMail->lightState = 2;
							}
							//Someone has entered
							else{
								printf("Someone has entered and ");
								if(node[i].lightLevel < node[i].lightThreshold){
									printf("light is to low so lights on\r\n");
									node[i].changeCheck[0] = 1;
									varMail->lightState = 1;
								}
								else{
									printf("light is to high so don't care about state\r\n");
									varMail->lightState = 2;
								}
							}
						}
						//room is vacant or occupied
						else{
							//Room is vacant
							if(node[i].pirLevel == 0){
								printf("room is vacant and ");
								//First time since left then turn off
								if(node[i].changeCheck[0] == 1){
									printf("turning lights off\r\n");
									varMail->lightState = 0;
									node[i].changeCheck[0] = 0;
								}
								else{
									printf("don't care\r\n");
									varMail->lightState = 2;
								}
							}
							//Room is occupied
							else{
								printf("room is occupied and ");
								if(node[i].lightLevel < node[i].lightThreshold){
									if(node[i].changeCheck[0] == 0){	
										printf("light is too low so lights on\r\n");
										node[i].changeCheck[0] = 1;
									}
									else
									{
										printf("don't care\r\n");
										varMail->lightState = 2;
									}
								}
								else{
									if(node[i].changeCheck[0] == 1){
										printf("light is too high so turning off\r\n");
										varMail->lightState = 0;
										node[i].changeCheck[0] = 0;
									}
									else{
										printf("don't care\r\n");
										varMail->lightState = 2;
									}
								}
							}
						}
					}
					else{
						varMail->lightState = 2;
					}
					
					
					//determine AC/Heater
				//Checker state 0 = off, 1 = heater on, 2 = ac on
				if(node[i].heatingOverride == 0 && node[i].acOverride == 0){
					if(node[i].prevPirLevel != node[i].pirLevel){
							//Someone has left
							if(node[i].pirLevel == 0){
								printf("Someone has left\r\n");
								varMail->heaterState = 2;
								varMail->acState = 2;
							}
							//Someone has entered
							else{
								printf("Someone has entered and ");
								if(node[i].tempLevel < node[i].lowerHeatThreshold){
									printf("temp is to low so heater on\r\n");
									varMail->heaterState = 1;
									varMail->acState = 2;
									node[i].changeCheck[1] = 1;
								}
								else if(node[i].tempLevel > node[i].upperHeatThreshold){
									printf("temp is to high so ac on\r\n");
									varMail->acState = 1;
									varMail->heaterState = 2;
									node[i].changeCheck[1] = 2;
								}
								else{
									printf("temp is just right so doing nothing\r\n");
									varMail->heaterState = 2;
									varMail->acState = 2;
								}
							}
						}
						//room is vacant or occupied
						else{
							//Room is vacant
							if(node[i].pirLevel == 0){
								printf("room is vacant and ");
								//First time since left then turn off
								if(node[i].changeCheck[1] == 1){
									printf("turning heater off\r\n");
									varMail->heaterState = 0;
									varMail->acState = 2;
									node[i].changeCheck[1] = 0;
								}
								else if(node[i].changeCheck[1] == 2){
									printf("turning ac off\r\n");
									varMail->acState = 0;
									varMail->heaterState = 2;
									node[i].changeCheck[1] = 0;
								}
								else{
									printf("don't care\r\n");
									varMail->acState = 2;
									varMail->heaterState = 2;
								}
							}
							//Room is occupied
							else{
								printf("room is occupied and ");
								//Room too cold
								if(node[i].tempLevel < node[i].lowerHeatThreshold){
									//Turning heater on from being off
									if(node[i].changeCheck[1] == 0){
										printf("temp is to low so heater on\r\n");
										varMail->heaterState = 1;
										varMail->acState = 2;
										node[i].changeCheck[1] = 1;
									}
									//Turning heater on from being on AC
									else if(node[i].changeCheck[1] == 2){
										printf("temp is to low, turning ac off and heater on\r\n");
										varMail->heaterState = 1;
										varMail->acState = 0;
										node[i].changeCheck[1] = 1;
									}
									//Don't need to do anything
									else{
										printf("don't care\r\n");
										varMail->acState = 2;
										varMail->heaterState = 2;
									}
								}
								//Room too hot
								else if (node[i].tempLevel > node[i].upperHeatThreshold){
									//Turning ac on from being off
									if(node[i].changeCheck[1] == 0){
										printf("temp is to high so ac on\r\n");
										varMail->heaterState = 2;
										varMail->acState = 1;
										node[i].changeCheck[1] = 2;
									}
									//Turning heater on from being on AC
									else if(node[i].changeCheck[1] == 1){
										printf("temp is to high, turning heater off and ac on\r\n");
										varMail->heaterState = 0;
										varMail->acState = 1;
										node[i].changeCheck[1] = 2;
									}
									//Don't need to do anything
									else{
										printf("don't care\r\n");
										varMail->acState = 2;
										varMail->heaterState = 2;
									}
								}
								//Room just fine
								else{
									//Turn off heating
									if(node[i].changeCheck[1] == 1){
										printf("temp is fine so turn off heating\r\n");
										varMail->heaterState = 0;
										varMail->acState = 2;
										node[i].changeCheck[1] = 0;
									}
									//Turn off ac
									else if(node[i].changeCheck[1] == 2){
										printf("temp is fine so turn off ac\r\n");
										varMail->heaterState = 2;
										varMail->acState = 0;
										node[i].changeCheck[1] = 0;
									}
									//Nothing needs to be done
									else{
										printf("don't care\r\n");
										varMail->acState = 2;
										varMail->heaterState = 2;
									}
								}
							}
						}
				}	
				else{
					varMail->heaterState = 2;
					varMail->acState = 2;
				}
					
					
						/*RELEASE MUTEXES HERE?*/
					
						osMailPut(mail_box, varMail);
					
					
				}
			}			
		}
	}
}

//Set value template packet
//							|  SH bits  | SL bits  || MY |      |pn|st|chk
//7E 00 10 17 01 00 13 A2 00 FF FF FF FF FF FE 02 44 32 04 BD
//
//SL bits = array elements 9 -> 12
//MY bits = array elements 13 -> 14
//pin bits = array element 17 (Light = 32(D2), Heater = 35(D5), AC = 36(D6))
//set bits = array element 18 (low = 04, high = 05)
//Checksum = array element 19


void action_thread(void const *argument){
	uint8_t template_Dig_Out[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x13, 0xA2, 0x00, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x44, 0x32, 0x04, 0x00};
	uint8_t lightPin = 0x32, heaterPin = 0x35, acPin = 0x36;
	uint8_t setLow = 0x4, setHigh = 0x5;
	while(1){
		//idle until action event
		osEvent evt = osMailGet(mail_box, osWaitForever);
		
		if(evt.status == osEventMail){
			mail_t *mail = (mail_t*)evt.value.p;
			/*MUTEX HERE?*/
			/*
			Three states - 	0 = turn off
											1 = turn on
											2 = don't care / do nothing
			*/
			if(armedState == 0){
				
				//Create packet
				//Set SH
				template_Dig_Out[9] = (0xFF000000 & mail->slAddress) >> 24;
				template_Dig_Out[10] = (0x00FF0000 & mail->slAddress) >> 16;
				template_Dig_Out[11] = (0x0000FF00 & mail->slAddress) >> 8;
				template_Dig_Out[12] = (0x000000FF & mail->slAddress);
				
				//Set MY
				template_Dig_Out[13] = (0xFF00 & mail->myAddress) >> 8;
				template_Dig_Out[14] = (0xFF & mail->myAddress);
				
				//Send Light
				if(mail->lightState != 2){
					printf("Attempting to turn light pin ");
					//set pin
					template_Dig_Out[17] = lightPin;
					//set pin state based on passed value
					if (mail->lightState == 0){
						template_Dig_Out[18] = setLow;
						printf("off\r\n");
					}
					else if(mail->lightState == 1){
						template_Dig_Out[18] = setHigh;
						printf("on\r\n");
					}
					
					printf("for net addr = %02X%02X\r\n", template_Dig_Out[13], template_Dig_Out[14]);
					//set checksum
					uint16_t checksum = 0;
					//itterate through values
					for (int i = 3; i < 19; i++){
						checksum = checksum + template_Dig_Out[i];
					}
					
					printf("checksum before strip = %02X", checksum);
					//strip down to 8 bit number
					checksum = checksum & 0xff;
					template_Dig_Out[19] = 0xFF - checksum;
					printf("checksum after strip = %02X", template_Dig_Out[19]);
					
					for (int i = 0; i < 20; i++){
						printf("%02X ",template_Dig_Out[i]);
					}
					printf("\r\n");
					send_xbee(template_Dig_Out, 20);
				}
				
				/*ADD IN OTHER PIN PROCS*/
				
				printf("SL Address = %08X\r\n", mail->slAddress);
				printf("MY Address = %02X\r\n", mail->myAddress);
				printf("lighting state = %d\r\n", mail->lightState);
				printf("heater state = %d\r\n", mail->heaterState);
				printf("ac state = %d\r\n", mail->acState);
				
			}
			//Alarm stuff
			else{
				
			}
			/*FREE MUTEX HERE?*/
			osMailFree(mail_box, mail);
		}
	}
}