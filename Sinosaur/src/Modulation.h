#pragma once

#include "initialisation.h"

/*
 * DAC3_CH1 OPAMP1	PA2		Sine1_DAC
 * DAC1_OUT2		PA5		Sine2_DAC
 * DAC1_OUT1		PA4		Sine3_DAC
 * DAC3_CH2 OPAMP3	PB1		FM2_DAC
 * DAC2_OUT1		PA6		FM3_DAC
 * DAC4_CH2 OPAMP5	PA8		Swell_DAC
 * DAC4_CH1 OPAMP4	PB12	Ramp_DAC
 */

class Modulation {

	struct Sine {
		uint32_t index;
		uint32_t lfoCosPos = 0;			// Position of cordic cosine wave in q1.31 format

		volatile uint16_t& rate;
		volatile uint16_t& level;

		volatile uint32_t* dac;
		volatile uint32_t* led;

		Sine(uint32_t chn, volatile uint16_t& rate, volatile uint16_t& level, volatile uint32_t* dac, volatile uint32_t* led)
		 : index{chn}, rate{rate}, level{level}, dac{dac}, led{led} {};

	};

	void CalcLFO(Sine sine);

	// Initialise each channel's voices with pointers to the internal DAC and LED PWM setting
	Sine sine[3] = {
		{
			0,
			adc.Sine1_Rate,
			adc.Sine1_Level,
			&DAC3->DHR12R1,
			&TIM3->CCR1,
		},
		{
			1,
			adc.Sine2_Rate,
			adc.Sine2_Level,
			&DAC1->DHR12R2,
			&TIM3->CCR1,
		},
		{
			2,
			adc.Sine3_Rate,
			adc.Sine3_Level,
			&DAC1->DHR12R1,
			&TIM3->CCR1,
		}
	};


};
