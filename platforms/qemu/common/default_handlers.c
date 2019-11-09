extern void Default_Handler(void);

void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void TestIrq_Handler(void) __attribute__((weak, alias("Default_Handler")));
