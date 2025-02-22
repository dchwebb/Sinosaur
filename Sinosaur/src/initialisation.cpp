#include "initialisation.h"

GpioPin debugPin1	{GPIOC, 4, GpioPin::Type::Output};			// PC4: Debug (Marked TX)
GpioPin debugPin2	{GPIOC, 5, GpioPin::Type::Output};			// PC5: Debug (Marked RX)

// 12MHz / 3(M) * 85(N) / 2(R) = 170MHz
#define PLL_M 2
#define PLL_N 85
#define PLL_R 0			//  00: PLLR = 2, 01: PLLR = 4, 10: PLLR = 6, 11: PLLR = 8



void InitClocks(void) {
	// See page 236 for clock configuration
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;		// SYSCFG + COMP + VREFBUF + OPAMP clock enable
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;		// Enable Power Control clock
	PWR->CR5 &= ~PWR_CR5_R1MODE;				// Select the Range 1 boost mode

	RCC->CR |= RCC_CR_HSEON;					// HSE ON
	while ((RCC->CR & RCC_CR_HSERDY) == 0);		// Wait till HSE is ready

	// Configure PLL
	RCC->PLLCFGR = ((PLL_M - 1) << RCC_PLLCFGR_PLLM_Pos) | (PLL_N << RCC_PLLCFGR_PLLN_Pos) | (PLL_R << RCC_PLLCFGR_PLLR_Pos) | (RCC_PLLCFGR_PLLSRC_HSE);
	RCC->CR |= RCC_CR_PLLON;					// Enable the main PLL
	RCC->PLLCFGR = RCC_PLLCFGR_PLLREN;			// Enable PLL R (drives AHB clock)
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);		// Wait till the main PLL is ready

	// Configure Flash prefetch and wait state. NB STM32G473 is a category 3 device
	FLASH->ACR |= FLASH_ACR_LATENCY_4WS | FLASH_ACR_PRFTEN;
	FLASH->ACR &= ~FLASH_ACR_LATENCY_1WS;

	// The system clock must be divided by 2 using the AHB prescaler before switching to a higher system frequency.
	RCC->CFGR |= RCC_CFGR_HPRE_DIV2;			// HCLK = SYSCLK / 2
	RCC->CFGR |= RCC_CFGR_SW_PLL;				// Select the main PLL as system clock source

	// Wait till the main PLL is used as system clock source
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS ) != RCC_CFGR_SWS_PLL);

	// Reset the AHB clock (previously divided by 2) and set APB clocks
	RCC->CFGR &= ~RCC_CFGR_HPRE_Msk;
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;			// PCLK1 = HCLK / 1 (APB1)
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;			// PCLK2 = HCLK / 1 (APB2)

	SystemCoreClockUpdate();					// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL
}


void InitHardware()
{
	InitSysTick();
	InitDAC();
	InitPWMTimer();
	InitADC1(&adc.Sine3_Rate, 1);
	InitADC3(&adc.Sine2_Rate, 4);
	InitADC4(&adc.Sine1_Rate, 5);
	InitCordic();
}


void InitSysTick()
{
	SysTick_Config(SystemCoreClock / sysTickInterval);		// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 0);
}


void InitDAC()
{
	// Configure 4 DAC outputs PA4 and PA5 are regular DAC1 buffered outputs; PA2 and PB1 are DAC3 via OpAmp1 and OpAmp3 (Manual p.782)

	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;			// Enable GPIO Clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;			// Enable GPIO Clock
	RCC->AHB2ENR |= RCC_AHB2ENR_DAC1EN | RCC_AHB2ENR_DAC2EN | RCC_AHB2ENR_DAC3EN | RCC_AHB2ENR_DAC4EN;				// Enable DAC Clock

	DAC1->MCR &= ~DAC_MCR_MODE1_Msk;				// Set to normal mode: DAC1 channel1 is connected to external pin with Buffer enabled
	DAC1->CR |= DAC_CR_EN1;							// Enable DAC using PA4 (DAC_OUT1)

	DAC1->MCR &= ~DAC_MCR_MODE2_Msk;				// Set to normal mode: DAC1 channel2 is connected to external pin with Buffer enabled
	DAC1->CR |= DAC_CR_EN2;							// Enable DAC using PA5 (DAC_OUT2)

	DAC2->MCR &= ~DAC_MCR_MODE1_Msk;				// Set to normal mode: DAC2 channel1 is connected to external pin with Buffer enabled
	DAC2->CR |= DAC_CR_EN1;							// Enable DAC using PA6 (DAC_OUT1)

	// output triggered with DAC->DHR12R1 = x;

	// Opamp for DAC3 Channel 1: Follower configuration mode - output on PA2
	DAC3->MCR |= DAC_MCR_MODE1_0 | DAC_MCR_MODE1_1;	// 011: DAC channel1 is connected to on chip peripherals with Buffer disabled
	DAC3->CR |= DAC_CR_EN1;							// Enable DAC

	OPAMP1->CSR |= OPAMP_CSR_VMSEL;					// 11: Opamp_out connected to OPAMPx_VINM input
	OPAMP1->CSR |= OPAMP_CSR_VPSEL;					// 11: DAC3_CH1  connected to OPAMPx VINP input
	OPAMP1->CSR |= OPAMP_CSR_OPAMPxEN;				// Enable OpAmp: voltage on pin OPAMPx_VINP is buffered to pin OPAMPx_VOUT (PA2)

	// Opamp for DAC3 Channel 2: Follower configuration mode - output on PB1
	DAC3->MCR |= DAC_MCR_MODE2_0 | DAC_MCR_MODE2_1;	// 011: DAC channel2 is connected to on chip peripherals with Buffer disabled
	DAC3->CR |= DAC_CR_EN2;							// Enable DAC

	OPAMP3->CSR |= OPAMP_CSR_VMSEL;					// 11: Opamp_out connected to OPAMPx_VINM input
	OPAMP3->CSR |= OPAMP_CSR_VPSEL;					// 11: DAC3_CH2  connected to OPAMPx VINP input
	OPAMP3->CSR |= OPAMP_CSR_OPAMPxEN;				// Enable OpAmp: voltage on pin OPAMPx_VINP is buffered to pin OPAMPx_VOUT (PB1)

	// Opamp for DAC4 Channel 1: Follower configuration mode - output on PB12
	DAC4->MCR |= DAC_MCR_MODE1_0 | DAC_MCR_MODE1_1;	// 011: DAC channel1 is connected to on chip peripherals with Buffer disabled
	DAC4->CR |= DAC_CR_EN1;							// Enable DAC

	OPAMP4->CSR |= OPAMP_CSR_VMSEL;					// 11: Opamp_out connected to OPAMPx_VINM input
	OPAMP4->CSR |= OPAMP_CSR_VPSEL;					// 11: DAC4_CH1  connected to OPAMPx VINP input
	OPAMP4->CSR |= OPAMP_CSR_OPAMPxEN;				// Enable OpAmp: voltage on pin OPAMPx_VINP is buffered to pin OPAMPx_VOUT (PB12)

	// Opamp for DAC4 Channel 2: Follower configuration mode - output on PA8
	DAC4->MCR |= DAC_MCR_MODE2_0 | DAC_MCR_MODE2_1;	// 011: DAC channel2 is connected to on chip peripherals with Buffer disabled
	DAC4->CR |= DAC_CR_EN2;							// Enable DAC

	OPAMP5->CSR |= OPAMP_CSR_VMSEL;					// 11: Opamp_out connected to OPAMPx_VINM input
	OPAMP5->CSR |= OPAMP_CSR_VPSEL;					// 11: DAC4_CH2  connected to OPAMPx VINP input
	OPAMP5->CSR |= OPAMP_CSR_OPAMPxEN;				// Enable OpAmp: voltage on pin OPAMPx_VINP is buffered to pin OPAMPx_VOUT (PA8)

}


void InitPWMTimer()
{
	// TIM3: PE2 - PE5
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;

	GpioPin::Init(GPIOE, 2, GpioPin::Type::AlternateFunction, 2);		// Enable channel 1 PWM output pin on PE2
	GpioPin::Init(GPIOE, 3, GpioPin::Type::AlternateFunction, 2);		// Enable channel 2 PWM output pin on PE3
	GpioPin::Init(GPIOE, 4, GpioPin::Type::AlternateFunction, 2);		// Enable channel 3 PWM output pin on PE4
	GpioPin::Init(GPIOE, 5, GpioPin::Type::AlternateFunction, 2);		// Enable channel 4 PWM output pin on PE5

	TIM3->CCMR1 |= TIM_CCMR1_OC1PE;					// Output compare 1 preload enable
	TIM3->CCMR1 |= TIM_CCMR1_OC2PE;					// Output compare 2 preload enable
	TIM3->CCMR2 |= TIM_CCMR2_OC3PE;					// Output compare 3 preload enable
	TIM3->CCMR2 |= TIM_CCMR2_OC4PE;					// Output compare 4 preload enable

	TIM3->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2);	// 0110: PWM mode 1 - In upcounting, channel 1 active if TIMx_CNT<TIMx_CCR1
	TIM3->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);	// 0110: PWM mode 1 - In upcounting, channel 2 active if TIMx_CNT<TIMx_CCR2
	TIM3->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2);	// 0110: PWM mode 1 - In upcounting, channel 3 active if TIMx_CNT<TIMx_CCR3
	TIM3->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2);	// 0110: PWM mode 1 - In upcounting, channel 3 active if TIMx_CNT<TIMx_CCR3

	TIM3->CCR1 = 0;									// Initialise PWM level to 0
	TIM3->CCR2 = 0;
	TIM3->CCR3 = 0;
	TIM3->CCR4 = 0;

	// Timing calculations: Clock = 170MHz / (PSC + 1) = 21.25m counts per second
	// ARR = number of counts per PWM tick = 4095
	// 21.25m / ARR ~= 5.2kHz of PWM square wave with 4095 levels of output

	TIM3->ARR = 4095;								// Total number of PWM ticks
	TIM3->PSC = 7;									// Should give ~5.2kHz
	TIM3->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM3->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E);		// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM3->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers

	TIM3->CR1 |= TIM_CR1_CEN;						// Enable counter

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// TIM2: PB3 TIM2_CH2 (AF1)

	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

	GpioPin::Init(GPIOB, 3, GpioPin::Type::AlternateFunction, 1);		// Enable channel 2 PWM output pin on PB3

	TIM2->CCMR1 |= TIM_CCMR1_OC2PE;					// Output compare 2 preload enable
	TIM2->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);	// 0110: PWM mode 1 - In upcounting, channel 2 active if TIMx_CNT<TIMx_CCR2
	TIM2->CCR2 = 0x0;

	// Timing calculations: Clock = 170MHz / (PSC + 1) = 21.25m counts per second
	// ARR = number of counts per PWM tick = 2047
	// 21.25m / ARR ~= 5.2kHz of PWM square wave with 2047 levels of output

	TIM2->ARR = 4095;								// Total number of PWM ticks
	TIM2->PSC = 7;									// Should give ~5.2kHz
	TIM2->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E);		// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM2->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers

	TIM2->CR1 |= TIM_CR1_CEN;						// Enable counter
}


//	Setup Timer 5 on an interrupt to trigger outputs
void InitOutputTimer()
{
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;			// Enable Timer 5
	TIM5->PSC = 16;									// Set prescaler
	TIM5->ARR = 250; 								// Set auto reload register - 170Mhz / (PSC + 1) / ARR = ~40kHz

	TIM5->DIER |= TIM_DIER_UIE;						// DMA/interrupt enable register
	NVIC_EnableIRQ(TIM5_IRQn);
	NVIC_SetPriority(TIM5_IRQn, 0);					// Lower is higher priority

	TIM5->CR1 |= TIM_CR1_CEN;
	TIM5->EGR |= TIM_EGR_UG;						//  Re-initializes counter and generates update of registers
}


void InitAdcPins(ADC_TypeDef* ADC_No, std::initializer_list<uint8_t> channels) {
	uint8_t sequence = 1;

	for (auto channel: channels) {
		// Set conversion sequence to order ADC channels are passed to this function
		if (sequence < 5) {
			ADC_No->SQR1 |= channel << ((sequence) * 6);
		} else if (sequence < 10) {
			ADC_No->SQR2 |= channel << ((sequence - 5) * 6);
		} else if (sequence < 15) {
			ADC_No->SQR3 |= channel << ((sequence - 10) * 6);
		} else {
			ADC_No->SQR4 |= channel << ((sequence - 15) * 6);
		}

		// 000: 3 cycles, 001: 15 cycles, 010: 28 cycles, 011: 56 cycles, 100: 84 cycles, 101: 112 cycles, 110: 144 cycles, 111: 480 cycles
		if (channel < 10)
			ADC_No->SMPR1 |= 0b010 << (3 * channel);
		else
			ADC_No->SMPR2 |= 0b010 << (3 * (channel - 10));

		sequence++;
	}
}


/*--------------------------------------------------------------------------------------------
Configure ADC Channels to be converted:
	Sine3_Rate	 PC2	ADC12_IN8	1

	Sine2_Rate	 PE11	ADC345_IN15	3
	Sine1_Level	 PE9	ADC3_IN2	3
	Sine2_Level	 PE10	ADC345_IN14	3
	Sine3_Level	 PE7	ADC3_IN4,	3

	Sine1_Rate	 PE8	ADC345_IN6	4
	Swell_Rate	 PD9	ADC4_IN13	4
	Swell_Level	 PE12	ADC345_IN16	4
	Ramp_Rate	 PE14	ADC4_IN1	4
	Ramp_Level	 PE15	ADC4_IN2	4
*/

void InitADC1(volatile uint16_t* buffer, uint16_t channels)
{
	// Initialize Clocks
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
	RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;
	RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
	RCC->CCIPR |= RCC_CCIPR_ADC12SEL_1;				// 00: no clock, 01: PLL P clk clock, *10: System clock

	DMA1_Channel1->CCR &= ~DMA_CCR_EN;
	DMA1_Channel1->CCR |= DMA_CCR_CIRC;				// Circular mode to keep refilling buffer
	DMA1_Channel1->CCR |= DMA_CCR_MINC;				// Memory in increment mode
	DMA1_Channel1->CCR |= DMA_CCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel1->CCR |= DMA_CCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel1->CCR |= DMA_CCR_PL_0;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	DMA1->IFCR = 0x3F << DMA_IFCR_CGIF1_Pos;		// clear all five interrupts for this stream

	DMAMUX1_Channel0->CCR |= 5; 					// DMA request MUX input 5 = ADC1 (See p.427)
	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF0; // Channel 1 Clear synchronization overrun event flag

	ADC1->CR &= ~ADC_CR_DEEPPWD;					// Deep power down: 0: ADC not in deep-power down	1: ADC in deep-power-down (default reset state)
	ADC1->CR |= ADC_CR_ADVREGEN;					// Enable ADC internal voltage regulator

	// Wait until voltage regulator settled
	volatile uint32_t wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}
	while ((ADC1->CR & ADC_CR_ADVREGEN) != ADC_CR_ADVREGEN) {}

	ADC12_COMMON->CCR |= ADC_CCR_CKMODE;			// adc_hclk/4 (Synchronous clock mode)
	ADC1->CFGR |= ADC_CFGR_CONT;					// 1: Continuous conversion mode for regular conversions
	ADC1->CFGR |= ADC_CFGR_OVRMOD;					// Overrun Mode 1: ADC_DR register is overwritten with the last conversion result when an overrun is detected.
	ADC1->CFGR |= ADC_CFGR_DMACFG;					// 0: DMA One Shot Mode selected, 1: DMA Circular Mode selected
	ADC1->CFGR |= ADC_CFGR_DMAEN;					// Enable ADC DMA

	// For scan mode: set number of channels to be converted
	ADC1->SQR1 |= (channels - 1);

	// Start calibration
	ADC1->CR &= ~ADC_CR_ADCALDIF;					// Calibration in single ended mode
	ADC1->CR |= ADC_CR_ADCAL;
	while ((ADC1->CR & ADC_CR_ADCAL) == ADC_CR_ADCAL) {};


	/*
	Configure ADC Channels to be converted:
	Sine3_Rate: PC2	ADC12_IN8
	*/

	InitAdcPins(ADC1, {8});

	// Enable ADC
	ADC1->CR |= ADC_CR_ADEN;
	while ((ADC1->ISR & ADC_ISR_ADRDY) == 0) {}

	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF0; // Channel 1 Clear synchronization overrun event flag
	DMA1->IFCR = 0x3F << DMA_IFCR_CGIF1_Pos;		// clear all five interrupts for this stream

	DMA1_Channel1->CNDTR |= channels;				// Number of data items to transfer (ie size of ADC buffer)
	DMA1_Channel1->CPAR = (uint32_t)(&(ADC1->DR));	// Configure the peripheral data register address 0x40022040
	DMA1_Channel1->CMAR = (uint32_t)(buffer);		// Configure the memory address (note that M1AR is used for double-buffer mode) 0x24000040

	DMA1_Channel1->CCR |= DMA_CCR_EN;				// Enable DMA and wait
	wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}

	ADC1->CR |= ADC_CR_ADSTART;						// Start ADC
}


void InitADC3(volatile uint16_t* buffer, uint16_t channels)
{
	// Initialize Clocks
	RCC->AHB2ENR |= RCC_AHB2ENR_ADC345EN;
	RCC->CCIPR |= RCC_CCIPR_ADC345SEL_1;			// 00: no clock, 01: PLL P clk clock, *10: System clock

	DMA1_Channel2->CCR &= ~DMA_CCR_EN;
	DMA1_Channel2->CCR |= DMA_CCR_CIRC;				// Circular mode to keep refilling buffer
	DMA1_Channel2->CCR |= DMA_CCR_MINC;				// Memory in increment mode
	DMA1_Channel2->CCR |= DMA_CCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel2->CCR |= DMA_CCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel2->CCR |= DMA_CCR_PL_0;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	DMA1->IFCR = 0x3F << DMA_IFCR_CGIF2_Pos;		// clear all five interrupts for this stream

	DMAMUX1_Channel1->CCR |= 37; 					// DMA request MUX input 37 = ADC3 (See p.426)
	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF1; // Channel 2 Clear synchronization overrun event flag

	ADC3->CR &= ~ADC_CR_DEEPPWD;					// Deep power down: 0: ADC not in deep-power down	1: ADC in deep-power-down (default reset state)
	ADC3->CR |= ADC_CR_ADVREGEN;					// Enable ADC internal voltage regulator

	// Wait until voltage regulator settled
	volatile uint32_t wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}
	while ((ADC3->CR & ADC_CR_ADVREGEN) != ADC_CR_ADVREGEN) {}

	ADC345_COMMON->CCR |= ADC_CCR_CKMODE;			// adc_hclk/4 (Synchronous clock mode)
	ADC3->CFGR |= ADC_CFGR_CONT;					// 1: Continuous conversion mode for regular conversions
	ADC3->CFGR |= ADC_CFGR_OVRMOD;					// Overrun Mode 1: ADC_DR register is overwritten with the last conversion result when an overrun is detected.
	ADC3->CFGR |= ADC_CFGR_DMACFG;					// 0: DMA One Shot Mode selected, 1: DMA Circular Mode selected
	ADC3->CFGR |= ADC_CFGR_DMAEN;					// Enable ADC DMA

	// For scan mode: set number of channels to be converted
	ADC3->SQR1 |= (channels - 1);

	// Start calibration
	ADC3->CR &= ~ADC_CR_ADCALDIF;					// Calibration in single ended mode
	ADC3->CR |= ADC_CR_ADCAL;
	while ((ADC3->CR & ADC_CR_ADCAL) == ADC_CR_ADCAL) {};


	/*--------------------------------------------------------------------------------------------
	Configure ADC Channels to be converted:
	Sine2_Rate	 PE11	ADC345_IN15	3
	Sine1_Level	 PE9	ADC3_IN2	3
	Sine2_Level	 PE10	ADC345_IN14	3
	Sine3_Level	 PE7	ADC3_IN4,	3
	*/

	InitAdcPins(ADC3, {15, 2, 14, 4});

	// Enable ADC
	ADC3->CR |= ADC_CR_ADEN;
	while ((ADC3->ISR & ADC_ISR_ADRDY) == 0) {}

	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF1; // Channel 2 Clear synchronization overrun event flag
	DMA1->IFCR = 0x3F << DMA_IFCR_CGIF2_Pos;		// clear all five interrupts for this stream

	DMA1_Channel2->CNDTR |= channels;				// Number of data items to transfer (ie size of ADC buffer)
	DMA1_Channel2->CPAR = (uint32_t)(&(ADC3->DR));	// Configure the peripheral data register address 0x40022040
	DMA1_Channel2->CMAR = (uint32_t)(buffer);		// Configure the memory address (note that M1AR is used for double-buffer mode) 0x24000040

	DMA1_Channel2->CCR |= DMA_CCR_EN;				// Enable DMA and wait
	wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}

	ADC3->CR |= ADC_CR_ADSTART;						// Start ADC
}


void InitADC4(volatile uint16_t* buffer, uint16_t channels)
{
	DMA1_Channel3->CCR &= ~DMA_CCR_EN;
	DMA1_Channel3->CCR |= DMA_CCR_CIRC;				// Circular mode to keep refilling buffer
	DMA1_Channel3->CCR |= DMA_CCR_MINC;				// Memory in increment mode
	DMA1_Channel3->CCR |= DMA_CCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel3->CCR |= DMA_CCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel3->CCR |= DMA_CCR_PL_0;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	DMA1->IFCR = 0x3F << DMA_IFCR_CGIF3_Pos;		// clear all five interrupts for this stream

	DMAMUX1_Channel2->CCR |= 38; 					// DMA request MUX input 38 = ADC4 (See p.426)
	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF2; // Channel 2 Clear synchronization overrun event flag

	ADC4->CR &= ~ADC_CR_DEEPPWD;					// Deep power down: 0: ADC not in deep-power down	1: ADC in deep-power-down (default reset state)
	ADC4->CR |= ADC_CR_ADVREGEN;					// Enable ADC internal voltage regulator

	// Wait until voltage regulator settled
	volatile uint32_t wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}
	while ((ADC4->CR & ADC_CR_ADVREGEN) != ADC_CR_ADVREGEN) {}

	//ADC345_COMMON->CCR |= ADC_CCR_CKMODE;			// adc_hclk/4 (Synchronous clock mode)
	ADC4->CFGR |= ADC_CFGR_CONT;					// 1: Continuous conversion mode for regular conversions
	ADC4->CFGR |= ADC_CFGR_OVRMOD;					// Overrun Mode 1: ADC_DR register is overwritten with the last conversion result when an overrun is detected.
	ADC4->CFGR |= ADC_CFGR_DMACFG;					// 0: DMA One Shot Mode selected, 1: DMA Circular Mode selected
	ADC4->CFGR |= ADC_CFGR_DMAEN;					// Enable ADC DMA

	// For scan mode: set number of channels to be converted
	ADC4->SQR1 |= (channels - 1);

	// Start calibration
	ADC4->CR &= ~ADC_CR_ADCALDIF;					// Calibration in single ended mode
	ADC4->CR |= ADC_CR_ADCAL;
	while ((ADC4->CR & ADC_CR_ADCAL) == ADC_CR_ADCAL) {};


	/*--------------------------------------------------------------------------------------------
	Configure ADC Channels to be converted:
	Sine1_Rate	 PE8	ADC345_IN6	4
	Swell_Rate	 PD9	ADC4_IN13	4
	Swell_Level	 PE12	ADC345_IN16	4
	Ramp_Rate	 PE14	ADC4_IN1	4
	Ramp_Level	 PE15	ADC4_IN2	4
	*/

	InitAdcPins(ADC4, {6, 13, 16, 1, 2});

	// Enable ADC
	ADC4->CR |= ADC_CR_ADEN;
	while ((ADC4->ISR & ADC_ISR_ADRDY) == 0) {}

	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF2; // Channel 3 Clear synchronization overrun event flag
	DMA1->IFCR = 0x3F << DMA_IFCR_CGIF3_Pos;		// clear all five interrupts for this stream

	DMA1_Channel3->CNDTR |= channels;				// Number of data items to transfer (ie size of ADC buffer)
	DMA1_Channel3->CPAR = (uint32_t)(&(ADC4->DR));	// Configure the peripheral data register address 0x40022040
	DMA1_Channel3->CMAR = (uint32_t)(buffer);		// Configure the memory address (note that M1AR is used for double-buffer mode) 0x24000040

	DMA1_Channel3->CCR |= DMA_CCR_EN;				// Enable DMA and wait
	wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}

	ADC4->CR |= ADC_CR_ADSTART;						// Start ADC
}


void InitCordic()
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CORDICEN;
}




