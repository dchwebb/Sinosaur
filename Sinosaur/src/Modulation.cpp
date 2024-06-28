#include <Modulation.h>
#include "Cordic.h"

void Modulation::CalcLFO(Sine sine)
{
	uint16_t speed = sine.rate;

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
	float currentLevel = 1.0f;

	sine.lfoCosPos += (speed + 20) * 200;
	uint32_t output = static_cast<uint32_t>(Cordic::Cos(sine.lfoCosPos) * sine.level * currentLevel);			// Will output value from 0 - 4095
	*sine.dac = output;
	*sine.led = output;
}
