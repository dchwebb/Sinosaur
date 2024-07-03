#pragma once

#include "initialisation.h"

/*
 * DAC3_CH1 OPAMP1	PA2		Sine1_DAC
 * DAC1_OUT2		PA5		Sine2_DAC
 * DAC1_OUT1		PA4		Sine3_DAC
 * DAC3_CH2 OPAMP3	PB1		FM2_DAC
 * DAC2_OUT1		PA6		FM3_DAC
 * DAC4_CH1 OPAMP4	PB12	Ramp_DAC
 * DAC4_CH2 OPAMP5	PA8		Swell_DAC
 */

class Modulation {
public:
	void Init();
	void CalcLFO();
private:
	void CheckButtons();

	GpioPin Clock = {GPIOC, 12, GpioPin::Type::Input};


	struct Btn {
		GpioPin pin;
		uint32_t down = 0;
		uint32_t up = 0;

		// Debounce button press mechanism
		bool Pressed() {
			if (pin.IsHigh() && down && SysTickVal > down + 100) {
				down = 0;
				up = SysTickVal;
			}

			if (pin.IsLow() && down == 0 && SysTickVal > up + 100) {
				down = SysTickVal;
				return true;
			}
			return false;
		}
	};

	struct Lfo {
		enum Mode {none, ramp, swell};

		uint32_t index;
		uint32_t lfoCosPos = 0;			// Position of cordic cosine wave in q1.31 format

		volatile uint16_t& rate;
		volatile uint16_t& level;

		volatile uint32_t* dac;
		volatile uint32_t* ledPwm;

		Btn rateBtn;
		Btn levelBtn;

		Mode rateMode = Mode::none;
		Mode levelMode = Mode::none;

		GpioPin rateRampLed;
		GpioPin rateSwellLed;
		GpioPin levelRampLed;
		GpioPin levelSwellLed;

		Lfo(uint32_t chn, volatile uint16_t& rate, volatile uint16_t& level, volatile uint32_t* dac, volatile uint32_t* ledPwm,
				GpioPin rateBtn, GpioPin levelBtn,
				GpioPin rateRampLed, GpioPin rateSwellLed,
				GpioPin levelRampLed, GpioPin levelSwellLed)
		 : index{chn}, rate{rate}, level{level}, dac{dac}, ledPwm{ledPwm}, rateBtn{rateBtn}, levelBtn{levelBtn},
		   rateRampLed{rateRampLed}, rateSwellLed{rateSwellLed}, levelRampLed{levelRampLed}, levelSwellLed{levelSwellLed} {};

	} lfos[3] = {
		{
			0, adc.Sine1_Rate, adc.Sine1_Level, &DAC3->DHR12R1, &TIM3->CCR3,
			{GPIOD, 4, GpioPin::Type::InputPullup}, {GPIOD, 1, GpioPin::Type::InputPullup},
			{GPIOC, 9, GpioPin::Type::Output}, {GPIOB, 10, GpioPin::Type::Output},
			{GPIOD, 13, GpioPin::Type::Output}, {GPIOC, 6, GpioPin::Type::Output},
		}, {
			1, adc.Sine2_Rate, adc.Sine2_Level, &DAC1->DHR12R2, &TIM3->CCR4,
			{GPIOD, 5, GpioPin::Type::InputPullup}, {GPIOD, 2, GpioPin::Type::InputPullup},
			{GPIOA, 15, GpioPin::Type::Output}, {GPIOB, 11, GpioPin::Type::Output},
			{GPIOD, 14, GpioPin::Type::Output}, {GPIOC, 7, GpioPin::Type::Output},
		}, {
			2, adc.Sine3_Rate, adc.Sine3_Level, &DAC1->DHR12R1, &TIM2->CCR2,
			{GPIOD, 6, GpioPin::Type::InputPullup}, {GPIOD, 3, GpioPin::Type::InputPullup},
			{GPIOD, 12, GpioPin::Type::Output}, {GPIOF, 9, GpioPin::Type::Output},
			{GPIOD, 15, GpioPin::Type::Output}, {GPIOC, 8, GpioPin::Type::Output},
		}
	};

	struct Envelopes {
		static constexpr float rampInc = 1.0f / 1e8f;
		static constexpr float swellInc = 2.0f * rampInc;
		static constexpr float releaseInc = 0.5f;

		GpioPin Gate {GPIOD, 0, GpioPin::Type::Input};
		float swellDir = 1.0f;					// Switches to negative to reverse swell direction

		struct Env {
			volatile uint16_t& rate;
			volatile uint16_t& level;

			volatile uint32_t* dac;
			volatile uint32_t* ledPwm;

			float output;
		};

		Env ramp = { adc.Ramp_Rate, adc.Ramp_Level, &DAC4->DHR12R1, &TIM3->CCR1 };
		Env swell = { adc.Swell_Rate, adc.Swell_Level, &DAC4->DHR12R2, &TIM3->CCR2 };

	} envelopes;

	uint32_t output;
};


extern Modulation modulation;
