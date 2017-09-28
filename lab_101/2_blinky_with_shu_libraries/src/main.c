/*
 * main.c
 *
 * this is the main blinky application (this code is dependent on the
 * extra shu libraries such as the pinmappings list and the clock configuration
 * library)
 *
 * author:    Dr. Alex Shenfield
 * date:      01/09/2017
 * purpose:   55-604481 embedded computer networks : lab 101
 */

// include the hal drivers
#include "stm32f7xx_hal.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "gpio.h"

// map the led to GPIO PI1 (again, this is the inbuilt led)
gpio_pin_t red1 = {PI_1, GPIOI, GPIO_PIN_1};		//Red LED 1
gpio_pin_t amber1 = {PB_14, GPIOB, GPIO_PIN_14};	//Amber LED 1
gpio_pin_t green1 = {PB_15, GPIOB, GPIO_PIN_15};	//Green LED 1
gpio_pin_t red2 = {PC_7, GPIOC, GPIO_PIN_7};		//Red LED 2
gpio_pin_t amber2 = {PC_6, GPIOC, GPIO_PIN_6};	//Amber LED 2
gpio_pin_t green2 = {PG_6, GPIOG, GPIO_PIN_6};	//Green LED 2

// this is the main method
int main()
{
  // we need to initialise the hal library and set up the SystemCoreClock 
  // properly
  HAL_Init();
  init_sysclk_216MHz();
  
  // initialise the gpio pins
  init_gpio(red1, OUTPUT);
  init_gpio(amber1, OUTPUT);
	init_gpio(green1, OUTPUT);
  init_gpio(red2, OUTPUT);
  init_gpio(amber2, OUTPUT);
	init_gpio(green2, OUTPUT);
	
	//Preset Red on
	toggle_gpio(red1);
	
  // loop forever ...
  while(1)
  {
		/*Cycle 1*/
		//Green2 on
		toggle_gpio(green2);
		
		//Delay 2 seconds
		HAL_Delay(2000);
		
		/*Cycle 2*/
		//Green2 off
		toggle_gpio(green2);
		//Amber2 on
		toggle_gpio(amber2);
		
		//Delay 2 seconds
		HAL_Delay(2000);
		
		/*Cycle 3 */
		//Amber 1 on
		toggle_gpio(amber1);
		//Red 2 on
		toggle_gpio(red2);
		//Amber 2 off
		toggle_gpio(amber2);
		
		//Delay 2 seconds
		HAL_Delay(2000);
		
		/*Cycle 4*/
		//Green 1 on
		toggle_gpio(green1);
		//Red 1 off
		toggle_gpio(red1);
		//Amber 1 off
		toggle_gpio(amber1);
		
		//Delay 2 seconds
		HAL_Delay(2000);
		
		/*Cycle 5*/
		//amber 1 on 
		toggle_gpio(amber1);
		//Green 1 off
		toggle_gpio(green1);
		
		//Delay 2 seconds
		HAL_Delay(2000);
		
		/*Cycle 6*/
		//Red 1 on
		toggle_gpio(red1);
		//amber 1 off
		toggle_gpio(amber1);
		//Amber 2 on
		toggle_gpio(amber2);
		
		//Delay 2 seconds
		HAL_Delay(2000);
		
		/*Reset Cycle*/
		//Amber 2 off
		toggle_gpio(amber2);
		//Red 2 off
		toggle_gpio(red2);
  }
}
