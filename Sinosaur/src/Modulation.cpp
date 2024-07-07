#include <Modulation.h>
#include "Cordic.h"

Modulation modulation;

Modulation::Cfg Modulation::cfg;


void Modulation::Init()
{
	// Switch common anode LEDs to high to turn off
	for (auto& lfo : lfos) {
		if (lfo.rateMode != LfoMode::ramp)		lfo.rateRampLed.SetHigh();
		if (lfo.rateMode != LfoMode::swell)		lfo.rateSwellLed.SetHigh();
		if (lfo.levelMode != LfoMode::ramp)		lfo.levelRampLed.SetHigh();
		if (lfo.levelMode != LfoMode::swell)	lfo.levelSwellLed.SetHigh();
	}
}


void Modulation::CalcLFO()
{
	debugPin1.SetHigh();

	CheckButtons();
	CheckClock();

	CalculateEnvelopes();


	for (auto& lfo : lfos) {
		// Set output level
		float currentLevel = 0.5f;
		if (lfo.levelMode == LfoMode::ramp) {
			currentLevel *= envelopes.ramp.output;
		} else if (lfo.levelMode == LfoMode::swell) {
			currentLevel *= envelopes.swell.output;
		}
		lfo.outLevel = lfo.level * currentLevel;


		// Set output position in sine wave
		if (clockValid) {
			int32_t clkHyst = lfo.rate;
			if (lfo.rateMode != LfoMode::none) {
				clkHyst = (uint32_t)((float)clkHyst * ((lfo.rateMode == LfoMode::ramp) ? envelopes.ramp.output : envelopes.swell.output));
			}

			if (std::abs(clkHyst - (int32_t)lfo.clockHysteresis) > 20) {
				lfo.clockHysteresis = clkHyst;

				if (clkHyst < 682)				lfo.clockMult = 8.0f;
				else if (clkHyst < 1365) 		lfo.clockMult = 4.0f;
				else if (clkHyst < 2048) 		lfo.clockMult = 2.0f;
				else if (clkHyst < 2731) 		lfo.clockMult = 1.0f;
				else if (clkHyst < 3413) 		lfo.clockMult = 0.5f;
				else 							lfo.clockMult = 0.25f;
			}
			uint32_t clockSpeed = static_cast<uint32_t>(lfo.clockMult * static_cast<float>(clockInterval));

			lfo.lfoCosPos += 4294967295 / clockSpeed;

		} else {
			float speed = lfo.rate * reciprocal4096;
			if (lfo.rateMode != LfoMode::none) {
				speed *= ((lfo.rateMode == LfoMode::ramp) ? envelopes.ramp.output : envelopes.swell.output);
			}
			speed *= speed;			// Square the speed to increase resolution at low settings
			lfo.lfoCosPos += (uint32_t)((speed + 0.001) * 500'000.0f);
		}
		lfo.output = Cordic::Sin(lfo.lfoCosPos);


		// Calculate FM for LFOs 2 and 3
		if (lfo.index > 0) {
			static constexpr float float32Bit = std::pow(2, 32);
			static constexpr float fmScale = std::pow(2, 18);
			float fm;
			if (lfo.index == 1) {		// LFO 2 is modulated by LFO 1's sine out
				fm = lfos[lfo.index - 1].output * lfos[lfo.index - 1].outLevel * fmScale;
			} else {					// LFO 3 is modulated by LFO 2's fm out
				fm = lfos[lfo.index - 1].fmOutput * lfos[lfo.index - 1].outLevel * fmScale;
			}

			float fmPos = fm + (float)lfo.lfoCosPos;
			if (fmPos >= float32Bit) fmPos -= float32Bit;
			if (fmPos < 0.0f) fmPos += float32Bit;

			lfo.fmOutput = Cordic::Sin((uint32_t)fmPos);
			*lfo.fmDac = static_cast<uint32_t>((lfo.fmOutput + 1.0f) * lfo.outLevel);
		}


		// Scale output
		uint32_t out = static_cast<uint32_t>((lfo.output + 1.0f) * lfo.outLevel);			// Will output value from 0 - 4095
		*lfo.dac = out;
		*lfo.ledPwm = out;
	}

	debugPin1.SetLow();
}


void Modulation::CalculateEnvelopes()
{
	// If gate low (input is inverted) increment ramp and swell
	if (envelopes.Gate.IsLow()) {
		envelopes.ramp.output = std::min(envelopes.ramp.output + Envelopes::rampInc * adc.Ramp_Level * adc.Ramp_Rate, reciprocal4096 * adc.Ramp_Level);
		const float swellOutput = envelopes.swell.output + Envelopes::swellInc * adc.Swell_Level * adc.Swell_Rate * envelopes.swellDir;
		if (swellOutput * 4096 >= adc.Swell_Level) {
			envelopes.swellDir = -0.5f;			// Swell down sounds better slower than up
		} else {
			envelopes.swell.output = std::max(swellOutput, 0.0f);
		}
	} else {
		// return to zero values without abrupt change
		if (envelopes.ramp.output > 0.0f) {
			envelopes.ramp.output = std::max(envelopes.ramp.output - Envelopes::releaseInc, 0.0f);
		}
		if (envelopes.swell.output > 0.0f) {
			envelopes.swell.output = std::max(envelopes.swell.output - Envelopes::releaseInc, 0.0f);
		}
		envelopes.swellDir = 1.0f;
	}
	*envelopes.ramp.ledPwm = uint32_t(std::pow(envelopes.ramp.output, 2.0f) * 4095);
	*envelopes.ramp.dac = uint32_t(envelopes.ramp.output * 4095);
	*envelopes.swell.ledPwm = uint32_t(std::pow(envelopes.swell.output, 2.0f) * 4095);
	*envelopes.swell.dac = uint32_t(envelopes.swell.output * 4095);
}


void Modulation::CheckButtons()
{
	for (auto& lfo : lfos) {
		if (lfo.rateBtn.Pressed()) {
			config.ScheduleSave();
			switch (lfo.rateMode) {
			case LfoMode::none:
				lfo.rateMode = LfoMode::ramp;
				lfo.rateRampLed.SetLow();
				break;
			case LfoMode::ramp:
				lfo.rateMode = LfoMode::swell;
				lfo.rateRampLed.SetHigh();
				lfo.rateSwellLed.SetLow();
				break;
			case LfoMode::swell:
				lfo.rateMode = LfoMode::none;
				lfo.rateSwellLed.SetHigh();
				break;
			}
		}
		if (lfo.levelBtn.Pressed()) {
			config.ScheduleSave();
			switch (lfo.levelMode) {
			case LfoMode::none:
				lfo.levelMode = LfoMode::ramp;
				lfo.levelRampLed.SetLow();
				break;
			case LfoMode::ramp:
				lfo.levelMode = LfoMode::swell;
				lfo.levelRampLed.SetHigh();
				lfo.levelSwellLed.SetLow();
				break;
			case LfoMode::swell:
				lfo.levelMode = LfoMode::none;
				lfo.levelSwellLed.SetHigh();
				break;
			}
		}
	}
}


void Modulation::CheckClock()
{
	// Check if clock received
	if (Clock.IsLow()) {		// Clock signal high (inverted)
		if (!clockHigh) {
			clockInterval = clockCounter - lastClock;
			lastClock = clockCounter;
			clockHigh = true;
		}
	} else {
		clockHigh = false;
	}
	clockValid = (clockCounter - lastClock < (SampleRate * 2));					// Valid clock interval is within a second
	++clockCounter;


}
