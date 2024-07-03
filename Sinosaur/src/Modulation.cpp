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

	// If gate low (input is inverted) increment ramp and swell
	if (envelopes.Gate.IsLow()) {
		envelopes.ramp.output = std::min(envelopes.ramp.output + Envelopes::rampInc * adc.Ramp_Level * adc.Ramp_Rate, (float)adc.Ramp_Level);
		float swellOutput = envelopes.swell.output + Envelopes::swellInc * adc.Swell_Level * adc.Swell_Rate * envelopes.swellDir;
		if (swellOutput >= adc.Swell_Level) {
			envelopes.swellDir = -1.0f;
		} else {
			envelopes.swell.output = std::max(swellOutput, 0.0f);
		}
	} else {
		if (envelopes.ramp.output > 0.0f) {
			envelopes.ramp.output = std::max(envelopes.ramp.output - Envelopes::releaseInc, 0.0f);
		}
		if (envelopes.swell.output > 0.0f) {
			envelopes.swell.output = std::max(envelopes.swell.output - Envelopes::releaseInc, 0.0f);
		}
		envelopes.swellDir = 1.0f;
	}
	*envelopes.ramp.ledPwm = uint32_t(envelopes.ramp.output);
	*envelopes.ramp.dac = uint32_t(envelopes.ramp.output);
	*envelopes.swell.ledPwm = uint32_t(envelopes.swell.output);
	*envelopes.swell.dac = uint32_t(envelopes.swell.output);

	for (auto& lfo : lfos) {
		uint16_t speed = lfo.rate;

		float currentLevel = 0.5f;
		if (lfo.levelMode == Lfo::Mode::ramp) {
			currentLevel *= reciprocal4096 * envelopes.ramp.output;
		}

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
