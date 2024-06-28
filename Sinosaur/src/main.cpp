#include "initialisation.h"

volatile uint32_t SysTickVal;
volatile ADCValues adc;



extern "C" {
#include "interrupts.h"
}


extern uint32_t SystemCoreClock;
int main(void)
{
	SystemInit();						// Activates floating point coprocessor and resets clock
	InitClocks();						// Configure the clock and PLL
	InitHardware();

	//usb.InitUSB();
	//config.RestoreConfig();

	while (1) {
		//usb.cdc.ProcessCommand();		// Check for incoming USB serial commands
	}
}


