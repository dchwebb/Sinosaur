#include <Modulation.h>
#include "Cordic.h"

Modulation modulation;

void Modulation::Init()
{
	// Switch common anode LEDs to high to turn off
	for (auto& lfo : lfos) {
		lfo.rateRampLed.SetHigh();
		lfo.rateSwellLed.SetHigh();
		lfo.levelRampLed.SetHigh();
		lfo.levelSwellLed.SetHigh();
	}

}


void Modulation::CalcLFO()
{
	CheckButtons();

	for (auto& lfo : lfos) {
		uint16_t speed = lfo.rate;

		/*
		if (adc.fadeIn > 10) {
			if ((gatePort->IDR & (1 << gatePin)) != 0) {			// Gate on
				currentLevel = 1.0f - (1.0f - currentLevel) * (lfos.settings.levelFadeIn + (float)adc.fadeIn * lfos.levelFadeInScale);
				currentSpeed = 1.0f - (1.0f - currentSpeed) * (lfos.settings.speedFadeIn + (float)adc.fadeIn * lfos.speedFadeInScale);
			} else {
				currentLevel = 0.0f;
				currentSpeed = 0.0f;
			}
			if (mode.settings.modeButton) {					// When mode button activated fade in the speed (as well as the level)
				speed *= currentSpeed;
			}

		} else {
			currentLevel = 1.0f;
		}
	*/
		float currentLevel = 0.5f;

		lfo.lfoCosPos += (speed + 20) * 200;
		output = static_cast<uint32_t>(Cordic::SinNormal(lfo.lfoCosPos) * lfo.level * currentLevel);			// Will output value from 0 - 4095
		*lfo.dac = output;
		*lfo.ledPwm = output;
	}
}


void Modulation::CheckButtons()
{
	for (auto& lfo : lfos) {
		if (lfo.rateBtn.Pressed()) {
			switch (lfo.rateMode) {
			case Lfo::Mode::none:
				lfo.rateMode = Lfo::Mode::ramp;
				lfo.rateRampLed.SetLow();
				break;
			case Lfo::Mode::ramp:
				lfo.rateMode = Lfo::Mode::swell;
				lfo.rateRampLed.SetHigh();
				lfo.rateSwellLed.SetLow();
				break;
			case Lfo::Mode::swell:
				lfo.rateMode = Lfo::Mode::none;
				lfo.rateSwellLed.SetHigh();
				break;
			}
		}
		if (lfo.levelBtn.Pressed()) {
			switch (lfo.levelMode) {
			case Lfo::Mode::none:
				lfo.levelMode = Lfo::Mode::ramp;
				lfo.levelRampLed.SetLow();
				break;
			case Lfo::Mode::ramp:
				lfo.levelMode = Lfo::Mode::swell;
				lfo.levelRampLed.SetHigh();
				lfo.levelSwellLed.SetLow();
				break;
			case Lfo::Mode::swell:
				lfo.levelMode = Lfo::Mode::none;
				lfo.levelSwellLed.SetHigh();
				break;
			}
		}
	}
}
