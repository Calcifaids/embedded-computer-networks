// include the relevant header files (from the c standard libraries)
#include <stdio.h>
#include <string.h>

// include the serial configuration files and the rtos api
// include serial.h?
#include "cmsis_os.h"

// include the shu bsp libraries for the stm32f7 discovery board
#include "pinmappings.h"
#include "clock.h"
#include "gpio.h"
#include "adc.h"

//declare thread
void main_thread(void const *argument);


//declare thread id
osThreadId tid_main_thread;

//Define thread prio
osThreadDef(main_thread, osPriorityNormal, 1, 0);

int init_main_thread(void){
	

}