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


gpio_pin_t pot = {PA_0, GPIOA, GPIO_PIN_0};
gpio_pin_t redLed = {PI_1, GPIOI, GPIO_PIN_1};
gpio_pin_t orangeLed = {PB_14, GPIOB, GPIO_PIN_14};
gpio_pin_t yellowLed = {PB_15, GPIOB, GPIO_PIN_15};
gpio_pin_t greenLed = {PA_8, GPIOA, GPIO_PIN_8};

// CODE

char led[20];
char barBuffer[27];
	
// this is the main method
int main()
{
  // we need to initialise the hal library and set up the SystemCoreClock 
  // properly
  HAL_Init();
  init_sysclk_216MHz();
	
	//Setup pot & LED's
	init_adc(pot);
	init_gpio(redLed, OUTPUT);
	init_gpio(orangeLed, OUTPUT);
	init_gpio(yellowLed, OUTPUT);
	init_gpio(greenLed, OUTPUT);
	
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

    
  // delay a little ...
  HAL_Delay(2000);
  
  while(1)
  {
		
		// print the welcome message ...
		BSP_LCD_Clear(LCD_COLOR_BROWN);
		BSP_LCD_DisplayStringAtLine(0, (uint8_t *)BOARDER);
		BSP_LCD_DisplayStringAtLine(1, (uint8_t *)welcome_message[0]);
		BSP_LCD_DisplayStringAtLine(2, (uint8_t *)welcome_message[1]);
		BSP_LCD_DisplayStringAtLine(3, (uint8_t *)BOARDER); 
		//Get ADC and convert (Fix this with calibratoin).
		uint16_t adc_val = read_adc(pot);
		adc_val = (adc_val * 100) / 4095.0;
		
		//switch LED
		if (adc_val <= 24){
			clearLeds();
			write_gpio(redLed, HIGH);
			char val[6] = "red";
			sprintf (led, "LED on = %s", val); 
		}
		else if((adc_val > 24) && (adc_val <= 49)){
			clearLeds();
			write_gpio(orangeLed, HIGH);
			char val[6] = "orange";
			sprintf (led, "LED on = %s", val); 
		}
		else if(adc_val > 49 && adc_val <= 74){
			clearLeds();
			write_gpio(yellowLed, HIGH);
			char val[6] = "yellow";
			sprintf (led, "LED on = %s", val); 
		}
		else{
			clearLeds();
			write_gpio(greenLed, HIGH);
			char val[6] = "green";
			sprintf (led, "LED on = %s", val); 
		}
		
    // format a string based around the uptime counter
    char str[12];
    sprintf(str, "ADC = %d%%", adc_val);
    // print the message to the lcd
    BSP_LCD_ClearStringLine(5);
    BSP_LCD_DisplayStringAtLine(5, (uint8_t *)str);
    BSP_LCD_ClearStringLine(6);
		BSP_LCD_DisplayStringAtLine(6, (uint8_t *)led);
		
		//set pixel scope
		adc_val = (adc_val * 480) / 100;
		BSP_LCD_DrawRect(0, 180, 480, 20);
		BSP_LCD_FillRect(0, 180, adc_val, 20);
    HAL_Delay(1000);
  }
}

void clearLeds(){
	write_gpio(redLed, LOW);
	write_gpio(orangeLed, LOW);
	write_gpio(yellowLed, LOW);
	write_gpio(greenLed, LOW);
}
