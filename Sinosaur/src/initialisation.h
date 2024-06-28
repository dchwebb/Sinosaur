#pragma once

#include "stm32g4xx.h"
#include "GpioPin.h"
#include <algorithm>
#include <Array>

extern volatile uint32_t SysTickVal;


struct ADCValues {
	uint16_t Sine3_Rate;		// PC2	ADC12_IN8	1

	uint16_t Sine2_Rate;		// PE11	ADC345_IN15	3
	uint16_t Sine1_Level;		// PE9	ADC3_IN2	3
	uint16_t Sine2_Level;		// PE10	ADC345_IN14	3
	uint16_t Sine3_Level;		// PE7	ADC3_IN4,	3

	uint16_t Sine1_Rate;		// PE8	ADC345_IN6	4
	uint16_t Swell_Rate;		// PD9	ADC4_IN13	4
	uint16_t Swell_Level;		// PE12	ADC345_IN16	4
	uint16_t Ramp_Rate;			// PE14	ADC4_IN1	4
	uint16_t Ramp_Level;		// PE15	ADC4_IN2	4
};



extern volatile ADCValues adc;
extern uint16_t calibZeroPos;

#define sysTickInterval 1000						// 1ms
#define SAMPLERATE 48000.0f

void InitClocks();
void InitHardware();
void InitSysTick();
void InitDAC();
void InitIO();
void InitADC1(volatile uint16_t* buffer, uint16_t channels);
void InitADC3(volatile uint16_t* buffer, uint16_t channels);
void InitADC4(volatile uint16_t* buffer, uint16_t channels);
void InitUart();
void InitCordic();
void InitPWMTimer();
void InitOutputTimer();
