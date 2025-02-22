void SysTick_Handler(void)
{
	SysTickVal++;
}


// Output timer
void TIM5_IRQHandler(void)
{
	TIM5->SR &= ~TIM_SR_UIF;					// clear UIF flag
	modulation.CalcLFO();
}

void NMI_Handler(void) {}

void HardFault_Handler(void) {
	while (1) {}
}

void MemManage_Handler(void) {
	while (1) {}
}

void BusFault_Handler(void) {
	while (1) {}
}

void UsageFault_Handler(void) {
	while (1) {}
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

