/*
 * main.c
 *
 * this is the main rtos xbee / uart based application
 *
 * author:		Alex Shenfield
 * date:			08/11/2017
 * purpose:   55-604481 embedded computer networks : lab 104
 */

// include the basic headers for the hal drivers and the rtos library
#include "stm32f7xx_hal.h"
#include "cmsis_os.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "gpio.h"

// include the xbee tx and rx functionality
#include "xbee.h"

// include the itm debugging
#include "itm_debug.h"

// include data proc thread
#include "data_processing.h"



// lets use an led as a message indicator
gpio_pin_t led = {PI_3, GPIOI, GPIO_PIN_3};

// RTOS
void lock_for_receive(void const *arg);
osTimerDef(lock_for_rec,lock_for_receive);

extern uint64_t systemUptime;
// Declare Threads Here!!
extern int init_xbee_threads(void);

//Include Mutex
extern osMutexId(xbee_rx_lock_id);

// OVERRIDE HAL DELAY
// make HAL_Delay point to osDelay (otherwise any use of HAL_Delay breaks things)
void HAL_Delay(__IO uint32_t Delay)
{
  osDelay(Delay);
}

// XBEE PINOUT AS FOLLOWS
// ADC: 	DIO0-LDR / DIO1-Temp / DIO2-Pot
// Din: 	DIO3-PIR / DIO4-Button
// Dout:	DIO5-Light / DIO7-AC / DIO11-Heater



// set up adc on dio 0 on all xbees connected to the WPAN - Light
uint8_t init_adc_0[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x30, 0x02, 0x74};

// set up adc on dio 1 on all xbees connected to the WPAN - Temp
uint8_t init_adc_1[] = {0x7E, 0x00, 0x10, 0x17, 0x02, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x31, 0x02, 0x72};

// set up adc on dio 2 on all xbees connected to the WPAN - Thresh Pot	
uint8_t init_adc_2[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x32, 0x02, 0x72};
	
//set up Din on dio 3 on all xbees connected to the WPAN - PIR
uint8_t init_dig_3[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x33, 0x03, 0x70};	

//set up Din on dio 3 on all xbees connected to the WPAN - Thresh button	
uint8_t init_dig_4[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x34, 0x03, 0x6F};

	//7E 00 10 17 01 00 00 00 00 00 00 FF FF FF FE 02 49 52 30 1F
uint8_t reset_ir_packet[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x49, 0x52, 0x30, 0x1F};
	
//Sampling packet with freq set to 6s
uint8_t ir_packet[]  = {0x7E, 0x00, 0x11, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x49, 0x52, 0x17, 0xFC, 0x3C};

//Sampling pakcet with freq set to 6s
uint8_t ir_addr_0[] = {0x7E, 0x00, 0x11, 0x17, 0x01, 0x00, 0x13, 0xA2, 0x00, 
	0x41, 0x72, 0xFC, 0xC9, 0x79, 0x1A, 0x02, 0x49, 0x52, 0x17, 0x70, 0x03};	

uint8_t ir_addr_1[] = {0x7E, 0x00, 0x11, 0x17, 0x01, 0x00, 0x13, 0xA2, 0x00, 
	0x41, 0x5D, 0x55, 0x3B, 0xEB, 0x47, 0x02, 0x49, 0x52, 0x17, 0x70, 0xAE};

// packet to do queried sampling (note - analog / digital ios must be 
// configured before this is sent or we will get an error status)
uint8_t is_packet[]  = {0x7E, 0x00, 0x0F, 0x17, 0x55, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x49, 0x53, 0xFA};
	
uint8_t my_packet[] = {0x7E, 0x00, 0x0F, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x4D, 0x59, 0x43};	

uint8_t ic_packet[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x49, 0x43, 0x10, 0x4E};
	
// CODE	

// this is the main method
int main()
 {	
	// initialise the real time kernel
	osKernelInitialize();
	
	// we need to initialise the hal library and set up the SystemCoreClock 
	// properly
	HAL_Init();
	init_sysclk_216MHz();
	
	// note also that we need to set the correct core clock in the rtx_conf_cm.c
	// file (OS_CLOCK) which we can do using the configuration wizard
	
	// set up the xbee uart at 9600 baud and enable the rx interrupts
	init_xbee(9600);
	enable_rx_interrupt();
	
	 
	// initialise our threads
	 osDelay(50);
	
	
	print_debug("initialising xbee thread", 24);
	init_xbee_threads();
	
	// wait for the coordinator xbee to settle down, and then send the 
	// configuration packets
	print_debug("sending configuration packets", 29);
	osDelay(1000);
	send_xbee(init_adc_0, 20);
	osDelay(1000);
	send_xbee(init_adc_1, 20);
	osDelay(1000);
	send_xbee(init_adc_2, 20);
	osDelay(1000);
	send_xbee(init_dig_3, 20);
	osDelay(1000);
	send_xbee(init_dig_4, 20);
	osDelay(1000);
	print_debug("... done!", 9);
	

	//Setup Addressing for nodes
	osDelay(1000);
	send_xbee(my_packet, 19);
	osDelay(1000);
	send_xbee(ic_packet, 20);
	osDelay(1000);
	//Change to itterate through
	//send_xbee(ir_packet, 21); 
	//Start Timer
	osKernelStart();
	
	osTimerId lockTimer = osTimerCreate(osTimer(lock_for_rec), osTimerPeriodic, NULL);
	osTimerStart(lockTimer, 3000);
	osStatus status = osMutexWait(xbee_rx_lock_id, osWaitForever);
	if (status == osOK){
		printf("Mutex Grabbed for timer\n");
	}
	else{
		printf("!!MUTEX NOT GRABBED AT START!!\n");
	}

	osDelay(500);
	send_xbee(ir_packet, 21); 
	//send_xbee(ir_addr_0, 21);
	//osDelay(500);
	//send_xbee(ir_addr_1, 21);
	
	// start everything running
	
	
}

//Hold Mutex for 3s to receive, then release for 3s
void lock_for_receive(void const *arg){
	static int i = 0;
	if ( i == 0){
		i = 1;
		
		osStatus status = osMutexRelease(xbee_rx_lock_id);
		if (status == osOK){
			printf("mutex released from timer: %llu\n",systemUptime);
		}
		else{
			printf("Mutex not released from timer\n");
		}
	}
	else{
		i = 0;
		
		osStatus status = osMutexWait(xbee_rx_lock_id, osWaitForever);
		if (status == osOK){
			printf("Mutex Grabbed for timer: %llu\n", systemUptime);
		}
		else{
			printf("mutex was not grabbed in the timer\n");
		}
	}
	
}
