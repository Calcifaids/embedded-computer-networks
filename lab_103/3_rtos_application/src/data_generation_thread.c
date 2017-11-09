/*
 * data_generation_thread.c
 *
 * this is a thread that periodically generates some data and puts it in a 
 * mail queue
 *
 * author:    Dr. Alex Shenfield
 * date:      06/09/2017
 * purpose:   55-604481 embedded computer networks : lab 103
 */
 
// include cmsis_os for the rtos api
#include "cmsis_os.h"

// include main.h (this is where we initialise the mail type and queue)
#include "main.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "random_numbers.h"
#include "gpio.h"
#include "adc.h"

// RTOS DEFINES

// declare the thread function prototypes, thread id, and priority
void data_thread(void const *argument);
osThreadId tid_data_thread;

osThreadDef(data_thread, osPriorityNormal, 1, 0);

// set up the mail queue
osMailQDef(mail_box, 16, mail_t);
osMailQId  mail_box;

// HARDWARE DEFINES

// led is on PI 1 (this is the inbuilt led)
//gpio_pin_t temp = {PF_9, GPIOF, GPIO_PIN_9};

gpio_pin_t temp = {PF_6, GPIOF, GPIO_PIN_6};
gpio_pin_t ldr 	= {PF_7, GPIOF, GPIO_PIN_7};
gpio_pin_t pot  = {PF_8, GPIOF, GPIO_PIN_8};

// THREAD INITIALISATION

// create the data generation thread
int init_data_thread(void)
{
  // initialize peripherals (i.e. the led and random number generator) here
	init_adc(pot);
	init_adc(temp);
	init_adc(ldr);
	
	//init_random();
  
  // create the mailbox
  mail_box = osMailCreate(osMailQ(mail_box), NULL);
  
  // create the thread and get its task id
  tid_data_thread = osThreadCreate(osThread(data_thread), NULL);
	
	
  // check if everything worked ...
  if(!tid_data_thread)
  {
    return(-1);
  }
  return(0);
}

// ACTUAL THREADS

// data generation thread - create some random data and stuff it in a mail 
// queue
void data_thread(void const *argument)
{  
  // infinite loop generating our fake data (one set of samples per second)
  // we also toggle the led so we can see what is going on ...
  while(1)
  {
    // create our mail (i.e. the message container)   
    mail_t* mail = (mail_t*) osMailAlloc(mail_box, osWaitForever);    
     
		// read adc and convert to % 
		uint16_t potStore = (read_adc(pot) * 100) / 4095.0;
    mail->potVal = potStore;
		
    float adcStore;
		//Store and convert temp
		adcStore = read_adc(temp);
		adcStore = (adcStore * 3300) / 4095.0;
		adcStore = (adcStore - 500) / 10.0;
		//Store to mail queue
		mail->tempVal = adcStore;
		
		//Store ldr
		/*For LDR realistic values are as so:
		*	Very Dark Room: 25
		* Averagely Bright Room: ~170
		* Very Bright Room: 470
		* Bright Torch: 2600
		*
		* Rather than calibrating, set values may be better for real world room luminosity
		* Therefore: scaling 25 -> 500
		*/
		
		uint16_t ldrStore = read_adc(ldr);
		// equation as follows: 
		// (actualInput - inputMin) * (outputMax - outputMin) / (inputMax - inputMin) + outputMin
		ldrStore = (ldrStore - 25) * 75 / 475.0;
		if (ldrStore > 100){
				ldrStore = 100;
		}
		
		mail->ldrVal = ldrStore; 
		    
    // put the data in the mail box and wait for one second
    osMailPut(mail_box, mail);
    osDelay(500);
  }
} 

