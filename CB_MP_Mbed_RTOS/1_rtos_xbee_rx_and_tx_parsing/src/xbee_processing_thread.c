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
#include "itm_debug.h"

// include the xbee packet parser
#include "xbee_packet_parser.h"

// include main.h with the mail type declaration
#include "main.h"
#include "gpio.h"



// RTOS DEFINES

// declare the thread function prototypes, thread id, and priority
void xbee_rx_thread(void const *argument);
osThreadId tid_xbee_rx_thread;
osThreadDef(xbee_rx_thread, osPriorityNormal, 1, 0);

void process_ir_thread(void const *argument);
osThreadId tid_process_ir_thread;
osThreadDef(process_ir_thread, osPriorityBelowNormal, 1, 0);

void action_thread(void const *argument);
osThreadId tid_action_thread;
osThreadDef(action_thread, osPriorityAboveNormal, 1, 0);

void thresh_over_thread(void const *argument);
osThreadId tid_thresh_over_thread;
osThreadDef(thresh_over_thread, osPriorityBelowNormal, 1, 0);


// setup a message queue to use for receiving characters from the interrupt
// callback
osMessageQDef(message_q, 512, uint8_t);
osMessageQId msg_q;

// set up the mail queues
osMailQDef(mail_box, 64, mail_t);
osMailQId  mail_box;
osMailQDef(proc_box, 64, proc_mail);
osMailQId  proc_box;
osMailQDef(thresh_over_box, 64, thresh_over_mail);
osMailQId	thresh_over_box;

//Timer definition
void poll_Button_Inputs(void const *arg);
osTimerDef(poll_Button_In, poll_Button_Inputs);

// Semaphores & Mutexes
osMutexDef (thresh_over_state);    
osMutexId  (thresh_over_state_id);
osMutexDef (xbee_rx_lock);
osMutexId  (xbee_rx_lock_id);
//GPIO defines

gpio_pin_t pb1 = {PA_8, GPIOA, GPIO_PIN_8};
gpio_pin_t pb2 = {PA_15, GPIOA, GPIO_PIN_15};
gpio_pin_t pb3 = {PG_6, GPIOG, GPIO_PIN_6};
gpio_pin_t pb4 = {PG_7, GPIOG, GPIO_PIN_7};
gpio_pin_t alarmOutput = {PB_15, GPIOB, GPIO_PIN_15};


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
	uint8_t	lightOverride;
	uint8_t	heatingOverride;
	uint8_t acOverride;
	uint8_t overThreshButtonState;
	uint64_t lastTimeStamp;
	
}node[2];

//Add funciton to pull copy of arrSize in timer ?
int arrSize = sizeof(node) / sizeof(node[0]);
uint8_t armedState = 0, doArmedOnce = 0;

uint64_t systemUptime = 0;

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
	proc_box = osMailCreate(osMailQ(proc_box), NULL);
	thresh_over_box = osMailCreate(osMailQ(thresh_over_box), NULL);

	//create mutexes
	thresh_over_state_id = osMutexCreate(osMutex(thresh_over_state));
	if (thresh_over_state_id != NULL){
    printf("Thresh mutex created \n");
  }   
	xbee_rx_lock_id = osMutexCreate(osMutex(xbee_rx_lock));
	if (xbee_rx_lock_id != NULL){
    printf("Xbee Lock Mutex created \n");
  }   

	// create the threads and get their task id
	tid_xbee_rx_thread = osThreadCreate(osThread(xbee_rx_thread), NULL);
	tid_action_thread = osThreadCreate(osThread(action_thread), NULL);
	tid_thresh_over_thread = osThreadCreate(osThread(thresh_over_thread), NULL);
	tid_process_ir_thread = osThreadCreate(osThread(process_ir_thread), NULL);
	

	//Init GPIO
	init_gpio(pb1, INPUT);
	init_gpio(pb2, INPUT);
	init_gpio(pb3, INPUT);
	init_gpio(pb4, INPUT);
	init_gpio(alarmOutput, OUTPUT);
	
	osTimerId pollButton = osTimerCreate(osTimer(poll_Button_In), osTimerPeriodic, NULL);
	osTimerStart(pollButton, 20);
		
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
	if(!tid_process_ir_thread){
		printf("process ir thread not created!\r\n");
		return(-1);
	}
	if(!tid_thresh_over_thread){
		printf("thresh over thread not created!\r\n");
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
		node[i].upperHeatThreshold = 18;
		node[i].lowerHeatThreshold = 17;
		node[i].lightThreshold = 40;
		node[i].heatingOverride = 0;
		node[i].lightOverride = 0;
		node[i].acOverride = 0;
		node[i].overThreshButtonState = 2;
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
				
				printf("packet TS = %llu\n", systemUptime);
				/*for(int i = 0; i < len; i++)
				{
					printf("%02X ", packet[i]);
				}
				printf("\r\n");
				*/
				
				//Packet Processing
				
				//Packet is MY command implying new node
				if(packet[15] == 0x4D && packet[16] == 0x59){
					static int myId;
					uint32_t slAddress = packet[9];
					
					//itterate to gain SL address
					for(int j = 10; j < 13; j++){
						slAddress = slAddress << 8;
						slAddress = slAddress | packet[j];
					}
					node[myId].slAddress = slAddress; 
					
					uint16_t myAddress = packet[18];
					myAddress = myAddress << 8;
					myAddress = myAddress | packet[19]; 
					node[myId].myAddress = myAddress;
					printf("Node %d SL Address = %08X\n", myId, node[myId].slAddress);
					printf("Node %d Network Address = %04X\n", myId, node[myId].myAddress);
					//Set all low here
					myId++;
				}
				
				//IO Data sample RX Indicator Processing
				if(packet[3] == 0x92){
					static uint64_t timeCheck;
					uint16_t myAddress = packet[12];
					myAddress = myAddress << 8;
					myAddress = myAddress | packet[13];
											
					//Normal Packet
					if(len >= 28){
						//Identify array element tied to address
						int i = 0;
						for (i = 0; i < arrSize; i++){
							if (myAddress == node[i].myAddress){
								break;
							}
						}
						
						//Pass to another thread to process
						proc_mail* procValMail = (proc_mail*) osMailAlloc(proc_box, osWaitForever);
						procValMail->addrArrayElem = i;
						procValMail->slAddress = node[i].slAddress;
						procValMail->myAddress = myAddress;
						
						procValMail->pirVal = (packet[20] & 0x8)>> 3;
						
						uint16_t ldrVal = packet[21];
						ldrVal = ldrVal << 8;
						ldrVal = ldrVal | packet[22];
						procValMail->ldrVal = ldrVal;
						uint16_t tempVal = packet[23];
						tempVal = tempVal << 8;
						tempVal = tempVal | packet[24];
						procValMail->tempVal = tempVal;
						osMailPut(proc_box, procValMail);
					}
					//Button press
					else if(len == 22){
						//CAUSING USART 6 ORE!!! FIND OUT HOW TO FIX!!
						uint8_t buttonCheck = (packet[20] >> 4) & 0x1;
						if(buttonCheck == 0x0 && systemUptime > timeCheck + 1500){
							printf("sending IS\n");
							timeCheck = systemUptime;
							//Identify array element tied to address
							int i = 0;
							for (i = 0; i < arrSize; i++){
								if (myAddress == node[i].myAddress){
									break;
								}
							}
							//propogate and send to mail action thread to create and send IS packetosMutexWait(xbee_rx_lock_id, osWaitForever);
							mail_t* isMail = (mail_t*) osMailAlloc(mail_box, osWaitForever);
							
							isMail->isCommand = 1;
							isMail->acState = 2;
							isMail->heaterState = 2;
							isMail->lightState = 2;
							isMail->slAddress = node[i].slAddress;
							isMail->myAddress = node[i].myAddress;
							osMailPut(mail_box, isMail);
						}
					}
				}
				//IS processing
				
				if(packet[15] == 0x49 && packet[16] == 0x53){
					
					//Get address
					uint16_t myAddress = packet[13];
					myAddress = myAddress << 8;
					myAddress = myAddress | packet[14];
					
					int i = 0;
					for (i = 0; i < arrSize; i++){
						if (myAddress == node[i].myAddress){
							break;
						}
					}
					//Push payload into mailqueue for handling
					thresh_over_mail* threshValMail = (thresh_over_mail*) osMailAlloc(thresh_over_box, osWaitForever);
					
					threshValMail->addrArrayElem = i;
					//Pot payload in [28]-[29]
					uint16_t adcPayload = packet[28];
					adcPayload = adcPayload << 8;
					adcPayload = adcPayload | packet[29];
					threshValMail->adcVal = adcPayload;
					
					osMailPut(thresh_over_box, threshValMail);
				}
			}
		}
		else if (evt.status == osEventTimeout){
				printf("trying to re-enable interrupts\n");
		}		
	}
}


//Set value template packet
//							|  SH bits  | SL bits  || MY |      |pn|st|chk
//7E 00 10 17 01 00 13 A2 00 FF FF FF FF FF FE 02 44 32 04 BD
//
//SL bits = array elements 9 -> 12
//MY bits = array elements 13 -> 14
//pin bits = array element 17 (Light = 35(D5), Heater = 32(D11), AC = 36(D7))
//set bits = array element 18 (low = 04, high = 05)
//Checksum = array element 19

void action_thread(void const *argument){
	uint8_t const hard_Set_Packet[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x13, 0xA2, 0x00, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x44, 0x32, 0x04, 0x00};
	
	uint8_t template_Dig_Out[20] = {0};
	
	
	uint8_t lightPin = 0x35, heaterPin = 0x31, acPin = 0x37, dPrefix = 0x44, pPrefix = 0x50;
	uint8_t setLow = 0x4, setHigh = 0x5;
	
	while(1){
		//idle until action event
		osEvent evt = osMailGet(mail_box, osWaitForever);
		osMutexWait(xbee_rx_lock_id, osWaitForever); 
		if(evt.status == osEventMail){
			mail_t *mail = (mail_t*)evt.value.p;
			
			
			printf("Mutex grabbed for sending \n");
			/*
			Three states - 	0 = turn off
											1 = turn on
											2 = don't care / do nothing
			*/
			
			//send DIO command
			if(mail->isCommand == 0){
				//Create packet
				for (int i = 0; i < 20; i++){
					template_Dig_Out[i] = hard_Set_Packet[i];
				}

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
						printf("Turning light pin ");
						//set pin
						template_Dig_Out[16] = dPrefix;
						template_Dig_Out[17] = lightPin;
						//set pin state based on passed value
						if (mail->lightState == 0){
							template_Dig_Out[18] = setLow;
							printf("off ");
						}
						else if(mail->lightState == 1){
							template_Dig_Out[18] = setHigh;
							printf("on ");
						}
						
						printf("for net addr = %02X%02X\n", template_Dig_Out[13], template_Dig_Out[14]);
						//set checksum
						/*CHANGE TO FUNCTION*/
						uint16_t checksum = 0;
						//itterate through values
						for (int i = 3; i < 19; i++){
							checksum = checksum + template_Dig_Out[i];
						}

						//strip down to 8 bit number
						checksum = checksum & 0xff;
						template_Dig_Out[19] = 0xFF - checksum;
						
						printf("sending: ");
						for(int i = 0; i < 20; i++){
							printf("%02X ", template_Dig_Out[i]);
						}
						printf("\r\n");
						
						send_xbee(template_Dig_Out, 20);
					}
									
					//Send Heater
					if(mail->heaterState != 2){
						printf("Turning heat pin ");
						//set pin
						template_Dig_Out[16] = pPrefix;
						template_Dig_Out[17] = heaterPin;
						//set pin state based on passed value
						if (mail->heaterState == 0){
							template_Dig_Out[18] = setLow;
							printf("off ");
						}
						else if(mail->heaterState == 1){
							template_Dig_Out[18] = setHigh;
							printf("on ");
						}
						
						printf("for net addr = %02X%02X\n", template_Dig_Out[13], template_Dig_Out[14]);
						//set checksum
						uint16_t checksum = 0;
						//itterate through values
						for (int i = 3; i < 19; i++){
							checksum = checksum + template_Dig_Out[i];
						}
						
						//strip down to 8 bit number
						checksum = checksum & 0xff;
						template_Dig_Out[19] = 0xFF - checksum;
						
						printf("sending: ");
						for(int i = 0; i < 20; i++){
							printf("%02X ", template_Dig_Out[i]);
						}
						printf("\r\n");
						send_xbee(template_Dig_Out, 20);
					}
					
					//Send Heater
					if(mail->acState != 2){
						printf("Turning ac pin ");
						//set pin
						template_Dig_Out[16] = dPrefix;
						template_Dig_Out[17] = acPin;
						//set pin state based on passed value
						if (mail->acState == 0){
							template_Dig_Out[18] = setLow;
							printf("off ");
						}
						else if(mail->acState == 1){
							template_Dig_Out[18] = setHigh;
							printf("on ");
						}
						
						printf("for net addr = %02X%02X\n", template_Dig_Out[13], template_Dig_Out[14]);
						//set checksum
						uint16_t checksum = 0;
						//itterate through values
						for (int i = 3; i < 19; i++){
							checksum = checksum + template_Dig_Out[i];
						}
						
						//strip down to 8 bit number
						checksum = checksum & 0xff;
						template_Dig_Out[19] = 0xFF - checksum;
						
						printf("sending: ");
						for(int i = 0; i < 20; i++){
							printf("%02X ", template_Dig_Out[i]);
						}
						printf("\r\n");
						
						send_xbee(template_Dig_Out, 20);
				}
			}
			//send IS packet
			else if(mail->isCommand == 1){
				for (int i = 0; i < 19; i++){
					template_Dig_Out[i] = hard_Set_Packet[i];
				}
				
				//Set length
				template_Dig_Out[2] = 0x0F;
				
				//set SL
				template_Dig_Out[9] = (0xFF000000 & mail->slAddress) >> 24;
				template_Dig_Out[10] = (0x00FF0000 & mail->slAddress) >> 16;
				template_Dig_Out[11] = (0x0000FF00 & mail->slAddress) >> 8;
				template_Dig_Out[12] = (0x000000FF & mail->slAddress);
				
				//Set MY
				template_Dig_Out[13] = (0xFF00 & mail->myAddress) >> 8;
				template_Dig_Out[14] = (0xFF & mail->myAddress);
				
				template_Dig_Out[16] = 0x49;
				template_Dig_Out[17] = 0x53;
				uint16_t checksum = 0;
				
				//itterate through values
				for (int i = 3; i < 18; i++){
					checksum = checksum + template_Dig_Out[i];
				}
				
				//strip down to 8 bit number
				checksum = checksum & 0xff;
				template_Dig_Out[18] = 0xFF - checksum;
				
				printf("sending: ");
				for(int i = 0; i < 19; i++){
					printf("%02X ", template_Dig_Out[i]);
				}
				printf("\r\n");
				
				send_xbee(template_Dig_Out, 19);
			}
			osMailFree(mail_box, mail);
			osMutexRelease(xbee_rx_lock_id);
			printf("mutex released from sending func\n");
		}
	}
}



//Poll buttons every 20ms and generate a bitstream.
//Generate a 4 bit code through shifting 
void poll_Button_Inputs(void const *arg){
	
	static uint8_t button_history[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	static uint16_t passcodeBuffer = 0;
	static uint8_t armingVar = 0, timeTillArm = 10, countdownTimestamp = 1, timeout = 1;
	
	systemUptime = systemUptime + 20;
	
	//Check for timeout on passcode entry
	if(timeout >= 200){
		passcodeBuffer = 0;
		timeout = 1;
	}
	
	//Itterate through each value and store to history
	for (int i = 0; i < 4; i++){
		button_history[i] = button_history[i] << 1;
		uint8_t val;
		switch(i){
			case 0:
				val = read_gpio(pb1);
				break;
			case 1:
				val = read_gpio(pb2);
				break;
			case 2:
				val = read_gpio(pb3);
				break;
			case 3:
				val = read_gpio(pb4);
				break;
		}
		button_history[i] = button_history[i] | val;
		
		if((button_history[i] & 0xDF) == 0x07){
			//Shift on value of pressed button to password buffer
			passcodeBuffer = passcodeBuffer << 3;
			passcodeBuffer = passcodeBuffer | i+1;
			timeout = 1;
			printf("passcode buffer = %02X\r\n", passcodeBuffer);
			//Check if successful combination input
			//Code == 1243 (Designed so that leeway if mistake was made (Don't need to wait for timeout))
			if((passcodeBuffer & 0x2A3) == 0x2A3){
				passcodeBuffer = 0;
				countdownTimestamp = 1;
				armingVar = ~armingVar;
				if(armingVar == 0){
					/*!TURN OFF ARMING SYSTEM HERE!*/
					timeTillArm = 10;
					printf("Disarming System Now!\r\n");
					armedState = 0;
					write_gpio(alarmOutput, 0);
				}
			}
		}
	}
	
	//HAL get tick not working correctly, therefor ~50 entries when arming on = 1s
	//Wait 10 seconds and then set global armed state
	if(armingVar == 0xFF && timeTillArm != 0){
		countdownTimestamp ++;
		if(countdownTimestamp == 50){
			timeTillArm --;
			printf("arming in %d seconds\r\n", timeTillArm);
			if(timeTillArm == 0){
				/*!TURN ON ARMING SYSTEM!*/
				printf("Arming System Now!\r\n");
				armedState = 1;
				doArmedOnce = 0;
			}
			countdownTimestamp = 1;
		}
	}
	timeout ++;
}

void process_ir_thread(void const *argument){
	static uint8_t prevPirLevel[4];
	static uint8_t currentPirLevel[4];
	static uint8_t tempChangeCheck[4];
	static uint8_t lightChangeCheck[4];
	
	for (int i = 0; i < 4; i++){
		prevPirLevel[i] = 0;
		currentPirLevel[i] = 0;
		tempChangeCheck[i] = 0;
		lightChangeCheck[i] = 0;
	}
		
	while(1){
		osEvent evt = osMailGet(proc_box, osWaitForever);
			
		if(evt.status == osEventMail){
			proc_mail *procValMail = (proc_mail*)evt.value.p;
			
			//Process Values
			//Store pir
			prevPirLevel[procValMail->addrArrayElem] = prevPirLevel[procValMail->addrArrayElem]  << 1;
			prevPirLevel[procValMail->addrArrayElem] = currentPirLevel[procValMail->addrArrayElem] | prevPirLevel[procValMail->addrArrayElem];
			currentPirLevel[procValMail->addrArrayElem] = procValMail->pirVal;
			
			//Evaluate Light
			float lightVal = procValMail->ldrVal;
			lightVal = (lightVal - 100) * (100 - 1) / (880 - 168) + 1; 
			if (lightVal < 0){
				lightVal = 0;
			}
			
			float tempVal = procValMail->tempVal;
			tempVal = tempVal * (1200.0 / 1023.0);
			tempVal = (tempVal - 500.0) / 10.0;

			printf("Current PIR:%d, Prev PIR:%d\n",currentPirLevel[procValMail->addrArrayElem], prevPirLevel[procValMail->addrArrayElem]);
			printf("Light: %f, Temp: %f\n",lightVal, tempVal); 
			
			if(armedState == 1){
				static uint8_t doAlertOnce = 0;
				if(doArmedOnce == 0){
					doArmedOnce = 1;
					//Reset PIR
					for (int i = 0; i < 2; i++){
						prevPirLevel[i] = 0;
						currentPirLevel[i] = 0;
						mail_t* armedMail = (mail_t*) osMailAlloc(mail_box, osWaitForever);
						
						//Change to broadcast?
						armedMail->isCommand = 0;
						armedMail->slAddress = node[i].slAddress;
						armedMail->myAddress = node[i].myAddress;
						armedMail->acState = 0;
						armedMail->heaterState = 0;
						armedMail->lightState = 0;
						osMailPut(mail_box, armedMail);
						}
					doAlertOnce = 1;
				}
				if(currentPirLevel[procValMail->addrArrayElem] == 1 && doAlertOnce == 1){
					printf("!!intruder Alert!!\r\n");
					doAlertOnce = 0;
					write_gpio(alarmOutput, 1);
				}
			}
			else{
				uint8_t heaterState = 0, acState = 0, lightState = 0;
				

				//Process based on Light
				if(node[procValMail->addrArrayElem].lightOverride == 0){
					//Someone has entered or room is occupied
					if(currentPirLevel[procValMail->addrArrayElem] == 1){
						//Room is occupied
						if((prevPirLevel[procValMail->addrArrayElem] & 0x1) == 1){
							printf("The room is occupied ");
							if(lightVal < node[procValMail->addrArrayElem].lightThreshold){
								if(lightChangeCheck[procValMail->addrArrayElem] == 0){	
									printf("and the light is too low so turning the lights on.\n");
									lightState = 1;
									lightChangeCheck[procValMail->addrArrayElem] = 1;
								}
								else
								{
									printf("and nothing has changed.\n");
									lightState = 2;
								}
							}
							else{
								if(lightChangeCheck[procValMail->addrArrayElem] == 1){
									printf("and the light is too high so turning the lights off\n");
									lightState = 0;
									lightChangeCheck[procValMail->addrArrayElem] = 0;
								}
								else{
									printf("and nothing has changed.\n");
									lightState = 2;
								}
							}
						}
						//Someone has entered
						else{
							printf("Someone has entered the room ");
							if(lightVal < node[procValMail->addrArrayElem].lightThreshold){
									printf("and the light is too low so turning the lights on.\n");
									lightChangeCheck[procValMail->addrArrayElem] = 1;
									lightState = 1;
								}
								else{
									printf("light is too high so don't need to turn the lights on.\n");
									lightState = 2;
								}
						}
					}
					//Someone has left or room is empty
					else{
						//Room is vacant
						if((prevPirLevel[procValMail->addrArrayElem] | 0x0) == 0){
							printf("The room is vacant ");
							if(lightChangeCheck[procValMail->addrArrayElem] == 1){
									printf("so turning lights off.\n");
									lightState = 0;
									lightChangeCheck[procValMail->addrArrayElem] = 0;
								}
								else{
									printf("and the lights are already off.\n");
									lightState = 2;
								}
						}
						//Someone has left
						else{
							printf("Someone has left or is idle in the room ");
							if(lightVal < node[procValMail->addrArrayElem].lightThreshold){
								if(lightChangeCheck[procValMail->addrArrayElem] == 0){	
									printf("and the light is too low so turning the lights on.\n");
									lightState = 1;
									lightChangeCheck[procValMail->addrArrayElem] = 1;
								}
								else
								{
									printf("and nothing has changed.\n");
									lightState = 2;
								}
							}
							else{
								if(lightChangeCheck[procValMail->addrArrayElem] == 1){
									printf("and the light is too high so turning the lights off\n");
									lightState = 0;
									lightChangeCheck[procValMail->addrArrayElem] = 0;
								}
								else{
									printf("and nothing has changed.\n");
									lightState = 2;
								}
							}
						}
					}
				}
				else{
					printf("override enabled\n");
					lightState = 2;
				}
				//Process based on Temp
				if(node[procValMail->addrArrayElem].heatingOverride == 0 && node[procValMail->addrArrayElem].acOverride == 0){
					//Someone has entered or room is occupied
					if(currentPirLevel[procValMail->addrArrayElem] == 1){
						//Room is occupied
						if((prevPirLevel[procValMail->addrArrayElem] & 0x1) == 1){
							printf("The room is occupied ");
							//Room too cold
							if(tempVal < node[procValMail->addrArrayElem].lowerHeatThreshold){
								//Turning heater on from being off
								if(tempChangeCheck[procValMail->addrArrayElem] == 0){
									printf("and the temp is too low so turning the heating on.\n");
									heaterState = 1;
									acState = 2;
									tempChangeCheck[procValMail->addrArrayElem] = 1;
								}
								//Turning heater on from being on AC
								else if(tempChangeCheck[procValMail->addrArrayElem] == 2){
									printf("and the temp is too low so turning the AC off and the heating on.\n");
									heaterState = 1;
									acState = 0;
									tempChangeCheck[procValMail->addrArrayElem] = 1;
								}
								//Don't need to do anything
								else{
									printf("and nothing has changed.\n");
									acState = 2;
									heaterState = 2;
								}
							}
							//Room too hot
							else if (tempVal > node[procValMail->addrArrayElem].upperHeatThreshold){
								//Turning ac on from being off
								if(tempChangeCheck[procValMail->addrArrayElem] == 0){
									printf("and the temp is too high so turning the AC on.\n");
									heaterState = 2;
									acState = 1;
									tempChangeCheck[procValMail->addrArrayElem] = 2;
								}
								//Turning heater on from being on AC
								else if(tempChangeCheck[procValMail->addrArrayElem] == 1){
									printf("and the temp is too high so turning the heating off and the AC on.\n");
									heaterState = 0;
									acState = 1;
									tempChangeCheck[procValMail->addrArrayElem] = 2;
								}
								//Don't need to do anything
								else{
									printf("and nothing has changed.\n");
									acState = 2;
									heaterState = 2;
								}
							}
							//Room just fine
							else{
								//Turn off heating
								if(tempChangeCheck[procValMail->addrArrayElem] == 1){
									printf("and the temp is fine so turning the heating off.\n");
									heaterState = 0;
									acState = 2;
									tempChangeCheck[procValMail->addrArrayElem] = 0;
								}
								//Turn off ac
								else if(tempChangeCheck[procValMail->addrArrayElem] == 2){
									printf("and the temp is fine so turning the AC off.\n");
									heaterState = 2;
									acState = 0;
									tempChangeCheck[procValMail->addrArrayElem] = 0;
								}
								//Nothing needs to be done
								else{
									printf("and nothing has changed.\n");
									acState = 2;
									heaterState = 2;
								}
							}
						}
						//Someone has entered
						else{
							printf("Someone has entered the room ");
							if(tempVal < node[procValMail->addrArrayElem].lowerHeatThreshold){
								printf("and the temp is too low so turning the heating on.\n");
								heaterState = 1;
								acState = 2;
								tempChangeCheck[procValMail->addrArrayElem] = 1;
							}
							else if(tempVal > node[procValMail->addrArrayElem].upperHeatThreshold){
								printf("and the temp is too high so turning the AC on.\n");
								acState = 1;
								heaterState = 2;
								tempChangeCheck[procValMail->addrArrayElem] = 2;
							}
							else{
								printf("and nothing has changed.\n");
								heaterState = 2;
								acState = 2;
							}
						}
					}
					//Someone has left or room is empty
					else{
						//Room is vacant
						if((prevPirLevel[procValMail->addrArrayElem] | 0x0) == 0){
							printf("The room is vacant ");
							//First time since left then turn off
							if(tempChangeCheck[procValMail->addrArrayElem] == 1){
								printf("so turning the heating off.\n");
								heaterState = 0;
								acState = 2;
								tempChangeCheck[procValMail->addrArrayElem] = 0;
							}
							else if(tempChangeCheck[procValMail->addrArrayElem] == 2){
								printf("so turning the AC off.\n");
								acState = 0;
								heaterState = 2;
								tempChangeCheck[procValMail->addrArrayElem] = 0;
							}
							else{
								printf("and nothing has changed.\n");
								acState = 2;
								heaterState = 2;
							}
						}
						//Someone has left
						else{
							printf("Someone has left or is idle in the room ");
							//Room too cold
							if(tempVal < node[procValMail->addrArrayElem].lowerHeatThreshold){
								//Turning heater on from being off
								if(tempChangeCheck[procValMail->addrArrayElem] == 0){
									printf("and the temp is too low so turning the heating on.\n");
									heaterState = 1;
									acState = 2;
									tempChangeCheck[procValMail->addrArrayElem] = 1;
								}
								//Turning heater on from being on AC
								else if(tempChangeCheck[procValMail->addrArrayElem] == 2){
									printf("and the temp is too low so turning the AC off and the heating on.\n");
									heaterState = 1;
									acState = 0;
									tempChangeCheck[procValMail->addrArrayElem] = 1;
								}
								//Don't need to do anything
								else{
									printf("and nothing has changed.\n");
									acState = 2;
									heaterState = 2;
								}
							}
							//Room too hot
							else if (tempVal > node[procValMail->addrArrayElem].upperHeatThreshold){
								//Turning ac on from being off
								if(tempChangeCheck[procValMail->addrArrayElem] == 0){
									printf("and the temp is too high so turning the AC on.\n");
									heaterState = 2;
									acState = 1;
									tempChangeCheck[procValMail->addrArrayElem] = 2;
								}
								//Turning heater on from being on AC
								else if(tempChangeCheck[procValMail->addrArrayElem] == 1){
									printf("and the temp is too high so turning the heating off and the AC on.\n");
									heaterState = 0;
									acState = 1;
									tempChangeCheck[procValMail->addrArrayElem] = 2;
								}
								//Don't need to do anything
								else{
									printf("and nothing has changed.\n");
									acState = 2;
									heaterState = 2;
								}
							}
							//Room just fine
							else{
								//Turn off heating
								if(tempChangeCheck[procValMail->addrArrayElem] == 1){
									printf("and the temp is fine so turning the heating off.\n");
									heaterState = 0;
									acState = 2;
									tempChangeCheck[procValMail->addrArrayElem] = 0;
								}
								//Turn off ac
								else if(tempChangeCheck[procValMail->addrArrayElem] == 2){
									printf("and the temp is fine so turning the AC off.\n");
									heaterState = 2;
									acState = 0;
									tempChangeCheck[procValMail->addrArrayElem] = 0;
								}
								//Nothing needs to be done
								else{
									printf("and nothing has changed.\n");
									acState = 2;
									heaterState = 2;
								}
							}
						}
					}
				}
				else{
					printf("override enabled\n");
					heaterState = 2;
					acState = 2;
				}
				printf("\nSystem-uptime = %llu\n", systemUptime);
				if (acState + heaterState + lightState != 6){
					mail_t* varMail = (mail_t*) osMailAlloc(mail_box, osWaitForever);
					varMail->isCommand = 0;
					varMail->slAddress = node[procValMail->addrArrayElem].slAddress;
					varMail->myAddress = node[procValMail->addrArrayElem].myAddress;
					varMail->lightState = lightState;
					varMail->acState = acState;
					varMail->heaterState = heaterState;
					printf("slAd: %02X, myAdd: %02x\n", varMail->slAddress, varMail->myAddress);
					osMailPut(mail_box, varMail);
				}
			}
			osMailFree(proc_box, procValMail);
		}
	}
}



void thresh_over_thread(void const *argument){
	static uint8_t threshFlag[2] = {0};
	static uint8_t selector[2] = {0};
	
	while (1){
		
		osEvent evt = osMailGet(thresh_over_box, osWaitForever);
			
		if(evt.status == osEventMail){
			thresh_over_mail *threshValMail = (thresh_over_mail*)evt.value.p;
			uint16_t myAddress = node[threshValMail->addrArrayElem].myAddress;
			printf("Setting for %02X\n", myAddress);
			//remap Potentiometer to percent
			float potVal = threshValMail->adcVal;
			potVal = (potVal - 0) * (100 - 1) / (940 - 0) + 1;
			if (potVal < 0){
				potVal = 0;
			}
			else if (potVal > 100){
				potVal = 100;
			}
			
			if(threshFlag[threshValMail->addrArrayElem] == 1){
				threshFlag[threshValMail->addrArrayElem] = 0;
				printf("Button presed a second time.\n");
				switch(selector[threshValMail->addrArrayElem]){
					case 0:
						printf("light threshold set to %f\n", potVal);
						node[threshValMail->addrArrayElem].lightThreshold = potVal;
						break;
					case 1:
						printf("Heating threshold set to %f\n", potVal);
						node[threshValMail->addrArrayElem].lowerHeatThreshold = potVal;
						if (node[threshValMail->addrArrayElem].lowerHeatThreshold >= node[threshValMail->addrArrayElem].upperHeatThreshold){
							float acSymThresh = potVal + 5;
							node[threshValMail->addrArrayElem].upperHeatThreshold = acSymThresh;
							printf("AC threshold auto adjusted to %f\n",  acSymThresh);
						}
						break;
					case 2:
						printf("AC threshold set to %f\n", potVal);
						node[threshValMail->addrArrayElem].upperHeatThreshold = potVal;
						if (node[threshValMail->addrArrayElem].upperHeatThreshold <= node[threshValMail->addrArrayElem].lowerHeatThreshold){
							float heatSymThresh = potVal - 5;
							node[threshValMail->addrArrayElem].lowerHeatThreshold = heatSymThresh;
							printf("heating threshold auto adjusted to %f\n",  heatSymThresh); 
						}
						break;
				}
			}
			else if(potVal < 25){
			//Start timer
			threshFlag[threshValMail->addrArrayElem] = 1;
				printf("Button presed once, moving to threshold setting\n");
			}
			//Toggle override based on selector
			else if(potVal >= 25 && potVal < 66){
				//Make Mailbox
				mail_t* overrideMail = (mail_t*) osMailAlloc(mail_box, osWaitForever);
				overrideMail->isCommand = 0;
				overrideMail->slAddress = node[threshValMail->addrArrayElem].slAddress;
				overrideMail->myAddress = node[threshValMail->addrArrayElem].myAddress;
				switch(selector[threshValMail->addrArrayElem]){
					case 0:
						printf("Light override ");
						if(node[threshValMail->addrArrayElem].lightOverride == 0){
							node[threshValMail->addrArrayElem].lightOverride = 1;
							overrideMail->lightState = 1;
							overrideMail->acState = 2;
							overrideMail->heaterState = 2;
							printf("on\n");
							//Mail to set light on
						}
						else{
							node[threshValMail->addrArrayElem].lightOverride = 0;
							printf("off\n");
							//Mail to set light off
						}
						break;
					case 1:
						printf("Heating ");
						if(node[threshValMail->addrArrayElem].heatingOverride == 0){
							overrideMail->lightState = 2;
							overrideMail->acState = 0;
							overrideMail->heaterState = 1;
							node[threshValMail->addrArrayElem].heatingOverride = 1;
							node[threshValMail->addrArrayElem].acOverride = 0;
							printf("on\n");
							//Mail to set heater on & AC off
						}
						else{
							node[threshValMail->addrArrayElem].heatingOverride = 0;
							overrideMail->lightState = 2;
							overrideMail->acState = 0;
							overrideMail->heaterState = 0;
							printf("off\n");
							//Mail to set heater off
						}
						break;
					case 2:
						
						printf("AC ");
						if(node[threshValMail->addrArrayElem].acOverride == 0){
							overrideMail->lightState = 2;
							overrideMail->acState = 1;
							overrideMail->heaterState = 0;
							node[threshValMail->addrArrayElem].acOverride = 1;
							node[threshValMail->addrArrayElem].heatingOverride = 0;
							printf("on\n");
							//Mail to set AC on & heater off
						}
						else{
							node[threshValMail->addrArrayElem].acOverride = 0;
							overrideMail->lightState = 2;
							overrideMail->acState = 0;
							overrideMail->heaterState = 0;
							printf("off\n");
							//Mail to set AC off
						}
						break;
				}
				osMailPut(mail_box, overrideMail);
			}
			//itterate through selector
			else if(potVal >= 66){
				selector[threshValMail->addrArrayElem] ++;
				if(selector[threshValMail->addrArrayElem] == 3){
					selector[threshValMail->addrArrayElem] = 0;
				}
				printf("Selector for %02X set to ", myAddress);
				switch(selector[threshValMail->addrArrayElem]){
					case 0:
						printf("light\n");
						break;
					case 1:
						printf("heating\n");
						break;
					case 2:
						printf("AC\n");
						break;
				}
			}
			
			osMailFree(thresh_over_box, threshValMail);
		}
	}
}
