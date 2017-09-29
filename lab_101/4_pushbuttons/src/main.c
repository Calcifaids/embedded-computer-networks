/*
 * main.c
 *
 * this is the main push button application (this code is dependent on the
 * extra shu libraries such as the pinmappings list and the clock configuration
 * library)
 *
 * author:    Alex Shenfield
 * date:      01/09/2017
 * purpose:   55-604481 embedded computer networks : lab 101
 */

// include the hal drivers
#include "stm32f7xx_hal.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "gpio.h"

// map the led to GPIO PI_1 (the inbuilt led) and the push button to PI_11 
// (the user button)
gpio_pin_t led1 = {PI_1, GPIOI, GPIO_PIN_1};
gpio_pin_t led2 = {PB_14, GPIOB, GPIO_PIN_14};
gpio_pin_t pb1 = {PA_8, GPIOA, GPIO_PIN_8}; //Changed Pb pin as previously set to PI11

unsigned long last_debounce;
int current_state, last_state, button_state;
int debounce_delay = 40;

// this is the main method
int main()
{
  // we need to initialise the hal library and set up the SystemCoreClock 
  // properly
  HAL_Init();
  init_sysclk_216MHz();
	//HAL_GetTick();
	
  
  // initialise the gpio pins
  init_gpio(led1, OUTPUT);
	init_gpio(led2, OUTPUT);
  init_gpio(pb1, INPUT);
	write_gpio(led1, HIGH);
  write_gpio(led2, LOW);
  // loop forever ...
  while(1)
  {
		//Read pin state
		current_state = read_gpio(pb1);
		
		//Check for fluctuation on pin
		if (current_state != last_state){
				last_debounce = HAL_GetTick();
		}
		

		//Debounce check
		if ((HAL_GetTick() - last_debounce) > debounce_delay){
			//check if changed
			if(current_state != button_state){
				button_state = current_state;
				//If latched then toggle leds
				if(button_state == HIGH){
						toggle_gpio(led1);
						toggle_gpio(led2);
				}
			}
		}
		//store current state as last for beggining of next loop
		last_state = current_state;
  }
}
