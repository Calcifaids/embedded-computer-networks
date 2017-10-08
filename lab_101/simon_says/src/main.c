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
uint8_t get_guess(void);
uint8_t setArray(uint8_t maxCurrentLevel);
void readAndShift(uint64_t patternStore[], uint64_t *currentReading, uint8_t currentItteration);
void shiftToFirstValue(uint64_t patternStore[], uint8_t maxCurrentLevel);
void loseFunc(void);
void clear_leds(void);
void endOfSequence(void);
/*New functions*/


int main(){
	//Init Clock and HAL Lib
	HAL_Init();
	init_sysclk_216MHz();
	//Init UART, RNG
	init_uart(9600);
	init_random();
	
	//Setup the GPIO
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
	/*Potentiometer*/
	init_adc(timerPot);
	
	//Variables
	/*Time*/
	uint32_t 	current_time = 0;
	uint32_t 	timeout;
	/*Storage*/
	uint64_t 	patternStore[2] = {0};
	uint64_t 	currentReading;
	uint8_t		guessed_led = 0;
	uint8_t   storeLed = 0;
	/*Game*/
	uint8_t		gameOver = 0;
	uint8_t		currentItteration = 1;
	uint8_t		maxCurrentLevel = 1;
	uint8_t		firstPass = 0;
	
	//Begin main loop
	while(1){
		while(gameOver == 0){
			
			/*START GENERATION OF NEW NUMBER AND STORE*/
			
			//Set/Reset arrays for beggining of game
			uint32_t 	random_num = 0;
			uint8_t 	storeLed = 0;
			uint8_t 	whichArray = 0;
			
			//Generate Random number
			random_num = get_random_int();
			storeLed = (random_num % 4) + 1;
			
			//Preset value to write array
			whichArray = setArray(maxCurrentLevel);
			
			//Shift to prevent value overwrite
			patternStore[whichArray] = patternStore[whichArray] >> 3;
			
			//Mask new LED to MSB side
			switch (storeLed){
				case 1:
					patternStore[whichArray] = patternStore[whichArray] | 0x1000000000000000;
					break;
				case 2:
					patternStore[whichArray] = patternStore[whichArray] | 0x2000000000000000;
					break;
				case 3:
					patternStore[whichArray] = patternStore[whichArray] | 0x3000000000000000;
					break;
				case 4:
					patternStore[whichArray] = patternStore[whichArray] | 0x4000000000000000;
					break;
			}
			
			/*END GENERATION OF NEW NUMBER AND STORE*/
			
			/*START SEQUENCE READING AND DISPLAY*/
			
			//Reset looping variable
			currentItteration = 1;
			shiftToFirstValue(patternStore, maxCurrentLevel);
			
			//Loop while bits still to cycle
			while (currentItteration <= maxCurrentLevel){
				readAndShift(patternStore, &currentReading, currentItteration);
				//flash LED
				switch(currentReading){
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
				//Reset reading , clear, and increment for next reading
				currentReading = 0;
				HAL_Delay(2000);
				clear_leds();
				HAL_Delay(500);
				currentItteration ++;
			}
			//Flash LEDS to signify end of current level displaying
			endOfSequence();
			
			/*END SEQUENCE READING AND DISPLAY*/
			
			/*START SEQUENCE READING AND GUESSING*/
			//Beginning Setup
			currentItteration = 1;
			shiftToFirstValue(patternStore, maxCurrentLevel);
			
			//Make sure we haven't reached gameover or exceeded current Level
			while (gameOver == 0 && (currentItteration <= maxCurrentLevel)){
				//Get this loops value
				whichArray = setArray(maxCurrentLevel);
				readAndShift(patternStore, &currentReading, currentItteration);
				
				//preset Timeout & set current time 
				timeout = read_adc(timerPot);
				timeout = (timeout * 4000) / 1024 + 1000;
				current_time = HAL_GetTick();
				//reset variables
				storeLed = 0;
				firstPass = 0;
				guessed_led = 0;
				//Check a guess hasn't occured and we haven't timed over
				while((HAL_GetTick() < (current_time + timeout)) && (firstPass != 2)){
					//Allow timeout change during guess taking
					timeout = read_adc(timerPot);
					timeout = (timeout * 4000) / 1024 + 1000;
					//Need to fix button debounce for this!
					guessed_led = get_guess();
					if (guessed_led != 0 && firstPass == 0){
						//Led has been pressed for the first time
						firstPass = 1;
						storeLed = guessed_led;
						if(storeLed != currentReading){
							printf("Game Over!");
							gameOver = 1;
						}
					}
					else if (guessed_led == 0 && firstPass == 1){
						//Led has been released
						firstPass = 2;
					}
				}
				//Check for timeout
				if (HAL_GetTick() > (current_time + timeout) && storeLed == 0){
					gameOver = 1;
				}
				currentItteration ++;
			}

			//Increment Level
			maxCurrentLevel ++;
			//Reached maximum level (Could calculate dynamically using (no. of elements x 21) + 1, if letting the player choose and using malloc ?)
			if (maxCurrentLevel == 43){
				gameOver = 1;
			}
			/*END SEQUENCE READING AND GUESSING*/
			
		}
		//Gameover, so reset variables to restart
		loseFunc();
		maxCurrentLevel = 1;
		//change to loop to scale?
		patternStore[0] = 0; 
		patternStore[1] = 0;
		gameOver = 0;
	}
}

uint8_t get_guess(){
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
  HAL_Delay(50);
  
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

void clear_leds(){
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

uint8_t setArray(uint8_t maxCurrentLevel){
	uint8_t x;
	if (maxCurrentLevel > 21){
				//Store to patternStore[1]
				x = 1;
			}
			else{
				//Store to patternStore[0]
				x = 0;
			}
	return x;
}

void shiftToFirstValue(uint64_t patternStore[], uint8_t maxCurrentLevel){
	if (maxCurrentLevel > 21){
					//setup [1]
				int initialShift = 63 - ((maxCurrentLevel - 21) * 3);
				patternStore[1] = patternStore[1] >> initialShift;
				//constant set of 21 for [0]so no shift needed
			}
			else{
				//No shift required for [1] as not in use currently
				int initialShift = 63 - maxCurrentLevel * 3;
				patternStore[0] = patternStore[0] >> initialShift; 
			}
}

void readAndShift(uint64_t patternStore[], uint64_t *currentReading, uint8_t currentItteration){
	if (currentItteration <= 21){
		//Use and mask to determine the current segments flash value
		*currentReading = patternStore[0] & 0x7;
		//Push the pattern value off of the storage
		patternStore[0] = patternStore[0] >> 3;
		//loop the current reading the the MSB section of the storage with OR mask
		patternStore[0] = patternStore[0] | (*currentReading << 60);
	}
	else{
		//Use and mask to determine the current segments flash value
		*currentReading = patternStore[1] & 0x7;
		//Push the pattern value off of the storage
		patternStore[1] = patternStore[1] >> 3;
		//loop the current reading the the MSB section of the storage with OR mask
		patternStore[1] = patternStore[1] | (*currentReading << 60);
	}
}

void endOfSequence(){
	write_gpio(orangeLed, HIGH);	
	write_gpio(greenLed, HIGH);
	write_gpio(redLed, HIGH);
	write_gpio(yellowLed, HIGH);
	HAL_Delay(100);
	write_gpio(orangeLed, LOW);	
	write_gpio(greenLed, LOW);
	write_gpio(redLed, LOW);
	write_gpio(yellowLed, LOW);
	HAL_Delay(100);
	write_gpio(orangeLed, HIGH);	
	write_gpio(greenLed, HIGH);
	write_gpio(redLed, HIGH);
	write_gpio(yellowLed, HIGH);
	HAL_Delay(100);
	write_gpio(orangeLed, LOW);	
	write_gpio(greenLed, LOW);
	write_gpio(redLed, LOW);
	write_gpio(yellowLed, LOW);
	HAL_Delay(1000);
}
