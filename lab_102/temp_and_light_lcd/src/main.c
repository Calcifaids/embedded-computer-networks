/*
 * main.c
 *
 * this is the main lcd application
 *
 * author:    Dr. Alex Shenfield
 * date:      04/09/2017
 * purpose:   55-604481 embedded computer networks : lab 102
 */

// include the basic headers and hal drivers
#include "stm32f7xx_hal.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "stm32746g_discovery_lcd.h"
#include "adc.h"
#include "gpio.h"

// LCD DEFINES

// define a message boarder (note the lcd is 28 characters wide using Font24)
#define BOARDER     "****************************"

// specify a welcome message
const char * welcome_message[2] = 
{
  "*     Hello LCD World!     *",
  "*      Welcome to SHU      *"
};

void clearLeds(void);


// Set pins
gpio_pin_t ldr = {PF_10, GPIOF, GPIO_PIN_10};
gpio_pin_t temp = {PA_0, GPIOA, GPIO_PIN_0};

// CODE

char led[20];
	
// this is the main method
int main(){
  // we need to initialise the hal library and set up the SystemCoreClock 
  // properly
  HAL_Init();
  init_sysclk_216MHz();
	
	//Setup temp & LDR
	init_adc(ldr);
	init_adc(temp);

  // initialise the lcd
  BSP_LCD_Init();
  BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER, SDRAM_DEVICE_ADDR);
  BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER);

  // set the background colour to blue and clear the lcd
  BSP_LCD_SetBackColor(LCD_COLOR_BROWN);
  BSP_LCD_Clear(LCD_COLOR_BROWN);
  
  // set the font to use
  BSP_LCD_SetFont(&Font24); 
	
	BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
	BSP_LCD_DisplayStringAtLine(0, (uint8_t *)BOARDER);
	BSP_LCD_DisplayStringAtLine(1, (uint8_t *)welcome_message[0]);
	BSP_LCD_DisplayStringAtLine(2, (uint8_t *)welcome_message[1]);
	BSP_LCD_DisplayStringAtLine(3, (uint8_t *)BOARDER); 
   
	 //Calibrate LDR
  uint16_t calibration = 5000;
  uint16_t timestamp = HAL_GetTick();
	uint16_t ldrReading, maxLdr = 0, minLdr = 4095;
	while(HAL_GetTick() < (timestamp  + calibration)){
		ldrReading = read_adc(ldr);
	
		if (ldrReading > maxLdr){
			maxLdr = ldrReading;
		}
		if (ldrReading < minLdr){
			minLdr = ldrReading;
		}
	
	}
	while(1){	
		char str[12];
		
		sprintf(str, "minval = %d", minLdr);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_DisplayStringAtLine(6, (uint8_t *)str);
		
		
		
		
		//Read temp
		uint16_t tempVal = read_adc(temp);
		tempVal = (tempVal * 5.0) / 4095;
		tempVal = (tempVal - 0.5) * 100;
		sprintf(str, "temp = %d°c", maxLdr);
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_DisplayStringAtLine(5, (uint8_t *)str);
		ldrReading = read_adc(ldr);
		

		
  }
}
