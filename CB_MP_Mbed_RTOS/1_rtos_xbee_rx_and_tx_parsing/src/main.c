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

// Declare Threads Here!!
extern int init_xbee_threads(void);

// OVERRIDE HAL DELAY
// make HAL_Delay point to osDelay (otherwise any use of HAL_Delay breaks things)
void HAL_Delay(__IO uint32_t Delay)
{
  osDelay(Delay);
}

// XBEE

// xbee configuration packets


//Set up Digital Input on DIO 3
uint8_t init_dig_3[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x44, 0x33, 0x03, 0x6F};

// set up adc on dio 0 on all xbees connected to the WPAN - temperature
uint8_t init_adc_0[] = {0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x30, 0x02, 0x74};

// set up adc on dio 1 on all xbees connected to the WPAN - light
uint8_t init_adc_1[] = {0x7E, 0x00, 0x10, 0x17, 0x02, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x44, 0x31, 0x02, 0x72};

// packet to do queried sampling (note - analog / digital ios must be 
// configured before this is sent or we will get an error status)
uint8_t is_packet[]  = {0x7E, 0x00, 0x0F, 0x17, 0x55, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0x02, 0x49, 0x53, 0xFA};
	
uint8_t my_packet[] = {0x7E, 0x00, 0x0F, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x4D, 0x59, 0x43};	

	
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
	
	// print debugging message
	osDelay(50);
	print_debug("initialising xbee thread", 24);
		
	// initialise our threads
	init_xbee_threads();

	// wait for the coordinator xbee to settle down, and then send the 
	// configuration packets
	print_debug("sending configuration packets", 29);
	osDelay(1000);
	send_xbee(init_dig_3, 20);
	osDelay(1000);
	send_xbee(init_adc_0, 20);
	osDelay(1000);
	send_xbee(init_adc_1, 20);
	osDelay(1000);
	print_debug("... done!", 9);
	
	//Setup Addressing for nodes
	osDelay(1000);
	send_xbee(my_packet, 19);
	init_gpio(led, OUTPUT);
	

	// start everything running
	osKernelStart();
	send_xbee(is_packet, 19);
	osDelay(6000);
	send_xbee(is_packet, 19);
	osDelay(6000);
	send_xbee(is_packet, 19);
	osDelay(6000);
	send_xbee(is_packet, 19);
	osDelay(6000);
	send_xbee(is_packet, 19);
	osDelay(6000);
	send_xbee(is_packet, 19);
	osDelay(6000);
	send_xbee(is_packet, 19);
	
}
