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
gpio_pin_t led1 = {PI_1, GPIOI, GPIO_PIN_1};		//Red LED
gpio_pin_t led2 = {PB_14, GPIOB, GPIO_PIN_14};	//Amber LED
gpio_pin_t led3 = {PB_15, GPIOB, GPIO_PIN_15};	//Green LED

// this is the main method
int main()
{
  // we need to initialise the hal library and set up the SystemCoreClock 
  // properly
  HAL_Init();
  init_sysclk_216MHz();
  
  // initialise the gpio pins
  init_gpio(led1, OUTPUT);
  init_gpio(led2, OUTPUT);
	init_gpio(led3, OUTPUT);
  
  // loop forever ...
  while(1)
  {
    // red on
    toggle_gpio(led1);
    
    // wait for 4 second
    HAL_Delay(4000);
		
		// amber on
		toggle_gpio(led2);
		
		//delay 1 second
		HAL_Delay(1000);
			
		//red off
		toggle_gpio(led1);
		//amber off		
    toggle_gpio(led2);
    //Green on
		toggle_gpio(led3);
		
    // wait for 3 second
    HAL_Delay(3000);
		
		//amber on		
    toggle_gpio(led2);
    //Green off
		toggle_gpio(led3);
		
		//delay 1 second
		HAL_Delay(1000);
		
		//amber off		
    toggle_gpio(led2);
		
  }
}
