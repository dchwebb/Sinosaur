#include "initialisation.h"
#include "Modulation.h"

volatile uint32_t SysTickVal;
volatile ADCValues adc;

Config config{&modulation.configSaver};		// Construct config handler with list of configSavers

extern "C" {
#include "interrupts.h"
}


extern uint32_t SystemCoreClock;
int main(void)
{
	SystemInit();						// Activates floating point coprocessor and resets clock
	InitClocks();						// Configure the clock and PLL
	InitHardware();
	config.RestoreConfig();
	modulation.Init();
	InitOutputTimer();

	//usb.InitUSB();

	while (1) {
		config.SaveConfig();			// Save any scheduled changes
		//usb.cdc.ProcessCommand();		// Check for incoming USB serial commands
	}
}


