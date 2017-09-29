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
#include "random_numbers.h"

// map the relevant LEDs
gpio_pin_t led1 = {PC_7, GPIOC, GPIO_PIN_7};		//Led 1 - Top Left
gpio_pin_t led2 = {PC_6, GPIOC, GPIO_PIN_6};		//Led 2 - Middle Left
gpio_pin_t led3 = {PG_6, GPIOG, GPIO_PIN_6};		//Led 3 - Bottom Left
gpio_pin_t led4 = {PB_4, GPIOB, GPIO_PIN_4};		//Led 4 - Middle Center
gpio_pin_t led5 = {PG_7, GPIOG, GPIO_PIN_7};		//Led 5 - Bottom Right
gpio_pin_t led6 = {PI_0, GPIOI, GPIO_PIN_0};		//Led 6 - Middle Right
gpio_pin_t led7 = {PH_6, GPIOH, GPIO_PIN_6};		//Led 7 - Top Right

uint32_t rnd;
int clear_lights();
int x = 1;

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
  init_gpio(led4, OUTPUT);
  init_gpio(led5, OUTPUT);
  init_gpio(led6, OUTPUT);
  init_gpio(led7, OUTPUT);
	
		
	
  // loop forever ...
  while(1)
  {
		//Generate Random number not working at the moment
		//rnd = (get_random_int() % 6) + 1;		//breaking here
		clear_lights();
		
		//Switch back to rnd once fix worked out
		switch (x){
			case 1:
				write_gpio(led4,1);
				break;
			case 2:
				write_gpio(led2,1);
				write_gpio(led6,1);
				break;
			case 3: 
				write_gpio(led2,1);
				write_gpio(led4,1);
				write_gpio(led6,1);
				break;
			case 4:
				write_gpio(led1,1);
				write_gpio(led3,1);
				write_gpio(led5,1);
				write_gpio(led7,1);
				break;
			case 5:
				write_gpio(led1,1);
				write_gpio(led3,1);
				write_gpio(led4,1);
				write_gpio(led5,1);
				write_gpio(led7,1);
				break;
			case 6:
				write_gpio(led1,1);
				write_gpio(led2,1);
				write_gpio(led3,1);
				write_gpio(led5,1);
				write_gpio(led6,1);
				write_gpio(led7,1);
				break;
		}
		HAL_Delay(3000);
		
		//remove me once rnd fixed
		x++;
		if (x >= 7){
			x = 1;
		}
	}
}

int clear_lights(){
	/*There should be an easier and scalable way to reset lights using for loop and array*/
		write_gpio(led1,0);
		write_gpio(led2,0);
		write_gpio(led3,0);
		write_gpio(led4,0);
		write_gpio(led5,0);
		write_gpio(led6,0);
		write_gpio(led7,0);

}
