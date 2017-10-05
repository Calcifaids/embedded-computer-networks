/*
 * main.c
 *
 * this is the main button game application for two buttons (this code is 
 * dependent on the extra shu libraries such as the pinmappings list and the
 * clock configuration library)
 *
 * author:    Alex Shenfield
 * date:      04/09/2017
 * purpose:   16-6498 embedded computer networks : lab 101
 */

// include the hal drivers
#include "stm32f7xx_hal.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "random_numbers.h"
#include "gpio.h"
#include "adc.h"

// include the serial configuration files
#include "serial.h"

// DECLARATIONS

// leds
gpio_pin_t orangeLed = {PI_1, GPIOI, GPIO_PIN_1};
gpio_pin_t greenLed = {PB_14, GPIOB, GPIO_PIN_14};
gpio_pin_t redLed = {PB_15, GPIOB, GPIO_PIN_15};
gpio_pin_t yellowLed = {PA_8, GPIOA, GPIO_PIN_8};

// buttons
gpio_pin_t orangePush = {PG_7, GPIOG, GPIO_PIN_7};
gpio_pin_t greenPush = {PI_0, GPIOI, GPIO_PIN_0};
gpio_pin_t redPush = {PH_6, GPIOH, GPIO_PIN_6};
gpio_pin_t yellowPush = {PI_3, GPIOI, GPIO_PIN_3};
gpio_pin_t restartButton = {PF_6, GPIOF, GPIO_PIN_6};

//Potentiometer
gpio_pin_t timerPot = {PA_0, GPIOA, GPIO_PIN_0};

// local game functions
uint8_t get_led(void);
uint8_t get_guess(void);
uint8_t resetLives(int j);
uint8_t getPot(void);
void loseFunc(void);
void clear_leds(void);


// CODE

// this is the main method
int main()
{
  // we need to initialise the hal library and set up the SystemCoreClock 
  // properly
  HAL_Init();
  init_sysclk_216MHz();
  
  // initialise the uart and random number generator
  init_uart(9600);
  init_random();
	init_adc(timerPot);
	
  // set up the gpio
	/*Output LED's*/
  init_gpio(orangeLed, 0);
  init_gpio(greenLed, 0);
	init_gpio(redLed, 0);
  init_gpio(yellowLed, 0);
	/*Input Buttons*/
  init_gpio(orangePush, 1);
  init_gpio(greenPush, 1);
	init_gpio(redPush, 1);
  init_gpio(yellowPush, 1);
  init_gpio(restartButton, 1);
	
  // print an initial status message
  printf("we are alive!\r\n");
  
  // game variables ...
  
  // initialise time value
  uint32_t current_time = 0;

  // initialise some game variables
  uint32_t  timeout = 3000;
  uint8_t   current_led = 0;
  uint8_t   guessed_led = 0;
  uint8_t		lives = 9;
	uint32_t	difficulty = 0;
  // game loop ...
	
	//Set initial timeout from Pot
	timeout = read_adc(timerPot);
	timeout = (timeout * 4000) / 1024 + 1000;
  
  // loop forever ...
  while(1)
  {
    // set the current time and switch on an led
    current_time = HAL_GetTick();
    current_led = get_led();
		
	//	timeout = getPot();
    // while we've not run out of time ...
    while(HAL_GetTick() < (current_time + timeout - difficulty))
    {
      // get the current guess
			timeout = read_adc(timerPot);
			timeout = (timeout * 4000) / 1024 + 1000;
      guessed_led = get_guess();

      // check if we have actually made a guess
      if(guessed_led != 0)
      {
        // were we correct?
        if(guessed_led == current_led)
        {
          // reset the timer and get a new led
          printf("excellent - you win!\r\n");
					//decrease allowed time by 10ms
					difficulty += 10;
          current_led = get_led();
          current_time = HAL_GetTick();
        }
        else
        {
          // reset the timer and get a new led
          printf("sorry - you lose!\r\n");
					//Flash sequence
					loseFunc();
					lives --;
					printf("Lives left = %d\r\n",lives);
					lives = resetLives(lives);
					if (lives < 1){
						printf("Game over!\r\n");
						difficulty = 0;
						lives = 9;
						printf("Lives = %d", lives);
					}
					current_led = get_led();
          current_time = HAL_GetTick();
        }
      }
    }

    // we ran out of time
    printf("you ran out of time :(\r\n");
		lives --;
		printf("Lives left = %d\r\n",lives);
		if (lives < 1){
			printf("Game over!\r\n");
			lives = 9;
			difficulty = 0;
			printf("Lives = %d", lives);
		}
  }
}

// GAME FUNCTIONS

// get a new led
uint8_t get_led()
{
  // declare a random number variable and a current led variable
  uint32_t random_num = 0;
  uint8_t  current_led = 0;

  // make sure all leds are off
  clear_leds();
  HAL_Delay(500);

  // get a random number
  random_num = get_random_int();
  printf("Random number = %d\r\n", (random_num % 4) + 1);

  // use the random number to calculate which led to switch on
  current_led = (random_num % 4) + 1;
	switch(current_led){
		case 1:
			write_gpio(orangeLed, HIGH);
		break;
		case 2:
			write_gpio(greenLed, HIGH);
		break;
		case 3:
			write_gpio(redLed, HIGH);
		break;
		case 4:
			write_gpio(yellowLed, HIGH);
		break;
	}

  // return the chosen led
  return current_led;
}

// get led guess
uint8_t get_guess()
{
  // initialise variables for the button states
  uint8_t orangeButtonState = 0;
  uint8_t greenButtonState = 0;
	uint8_t interRedState = 0;
	uint8_t redButtonState = 0;
  uint8_t yellowButtonState = 0;

  // read the buttons
  orangeButtonState = read_gpio(orangePush);
  greenButtonState = read_gpio(greenPush);
	interRedState = read_gpio(redPush);
	yellowButtonState = read_gpio(yellowPush);
  
  // wait for a short period of time
  HAL_Delay(30);
  
  // read the buttons again
  orangeButtonState = orangeButtonState & read_gpio(orangePush);
  greenButtonState = greenButtonState & read_gpio(greenPush);
	interRedState = interRedState & read_gpio(redPush);
	if (interRedState == 0x1){
		redButtonState = 0x3;
	}
	yellowButtonState = yellowButtonState & read_gpio(yellowPush);
	
  // return a bit mask representing which led we guessed 
  // 
  return ((orangeButtonState) | (greenButtonState << 1) | (redButtonState) | (yellowButtonState << 2));
	
}

void clear_leds()
{
  // remember to clear all leds
  write_gpio(orangeLed, LOW);
  write_gpio(greenLed, LOW);
	write_gpio(redLed, LOW);
	write_gpio(yellowLed, LOW);
}

void loseFunc(){
	clear_leds();
	write_gpio(orangeLed, HIGH);
	HAL_Delay(200);
	write_gpio(greenLed, HIGH);
	HAL_Delay(200);
	write_gpio(redLed, HIGH);
	HAL_Delay(200);
	write_gpio(yellowLed, HIGH);
	HAL_Delay(200);
	write_gpio(orangeLed, LOW);
	HAL_Delay(200);
	write_gpio(greenLed, LOW);
	HAL_Delay(200);
	write_gpio(redLed, LOW);
	HAL_Delay(200);
	write_gpio(yellowLed, LOW);
	HAL_Delay(200);
}

//Passed timestamp and lives
uint8_t resetLives(int j){
	//Could Do with timer
	uint8_t stateChanged = 0;
	printf("To restart please press the restart button in the next 3 seconds!\r\n");
	long timeStamp = HAL_GetTick() + 3000;
	while(HAL_GetTick() < timeStamp){
		if(read_gpio(restartButton) == HIGH && stateChanged == 0){
			j = 9;
			printf("Restarted!\r\n Lives = %d\r\n",j);
			stateChanged = 1;
		}
	}
	return j;	
}

uint8_t getPot(){
	long time = read_adc(timerPot);
	time = (time * 4000) / 1024 + 1000;
	return time;
}
