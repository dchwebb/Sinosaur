#pragma once

#include "initialisation.h"
#include "configManager.h"


class Modulation {
public:
	void Init();
	void CalcLFO();

	enum LfoMode : uint8_t {none = 0, ramp, swell};

	struct Cfg {
		LfoMode rateMode[3];
		LfoMode levelMode[3];
	};
	static Cfg cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = nullptr
	};

private:
	void CheckButtons();
	void CheckClock();

	GpioPin Clock = {GPIOC, 12, GpioPin::Type::Input};

	bool     clockValid;					// True if a clock pulse has been received within a second
	uint32_t clockInterval;					// Clock interval in sample time
	uint32_t clockCounter;					// Counter used to calculate clock times in sample time
	uint32_t lastClock;						// Time last clock signal received in sample time
	bool     clockHigh;						// Record clock high state to detect clock transitions

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
		uint32_t index;							// Index is used to apply fm from previous lfo output
		uint32_t lfoCosPos = 0;					// Position of cordic cosine wave in q1.31 format
		uint32_t clockHysteresis;				// Hysteresis to prevent jumping between multipliers when using clock
		float clockMult;

		float output;
		float fmOutput;
		float outLevel;

		volatile uint16_t& rate;
		volatile uint16_t& level;

		volatile uint32_t* dac;
		volatile uint32_t* fmDac;
		volatile uint32_t* ledPwm;

		Btn rateBtn;
		Btn levelBtn;

		GpioPin rateRampLed;
		GpioPin rateSwellLed;
		GpioPin levelRampLed;
		GpioPin levelSwellLed;

		LfoMode& rateMode = Modulation::cfg.rateMode[index];// = Mode::none;
		LfoMode& levelMode = Modulation::cfg.levelMode[index];// = Mode::none;

		Lfo(uint32_t chn, volatile uint16_t& rate, volatile uint16_t& level,
				volatile uint32_t* dac, volatile uint32_t* fmDac, volatile uint32_t* ledPwm,
				GpioPin rateBtn, GpioPin levelBtn,
				GpioPin rateRampLed, GpioPin rateSwellLed,
				GpioPin levelRampLed, GpioPin levelSwellLed)
		 : index{chn}, rate{rate}, level{level}, dac{dac}, fmDac{fmDac}, ledPwm{ledPwm}, rateBtn{rateBtn}, levelBtn{levelBtn},
		   rateRampLed{rateRampLed}, rateSwellLed{rateSwellLed}, levelRampLed{levelRampLed}, levelSwellLed{levelSwellLed} {};

	} lfos[3] = {
		{
			0, adc.Sine1_Rate, adc.Sine1_Level, &DAC3->DHR12R1, nullptr, &TIM3->CCR3,
			{GPIOD, 4, GpioPin::Type::InputPullup}, {GPIOD, 1, GpioPin::Type::InputPullup},
			{GPIOC, 9, GpioPin::Type::Output}, {GPIOB, 10, GpioPin::Type::Output},
			{GPIOD, 13, GpioPin::Type::Output}, {GPIOC, 6, GpioPin::Type::Output},
		}, {
			1, adc.Sine2_Rate, adc.Sine2_Level, &DAC1->DHR12R2, &DAC3->DHR12R2, &TIM3->CCR4,
			{GPIOD, 5, GpioPin::Type::InputPullup}, {GPIOD, 2, GpioPin::Type::InputPullup},
			{GPIOA, 15, GpioPin::Type::Output}, {GPIOB, 11, GpioPin::Type::Output},
			{GPIOD, 14, GpioPin::Type::Output}, {GPIOC, 7, GpioPin::Type::Output},
		}, {
			2, adc.Sine3_Rate, adc.Sine3_Level, &DAC1->DHR12R1,&DAC2->DHR12R1,  &TIM2->CCR2,
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
};


extern Modulation modulation;
