#pragma once


/** \brief  Structure type to access the System Timer (SysTick).
 */
typedef struct
{
  volatile uint32_t CTRL;                    /*!< Offset: 0x000 (R/W)  SysTick Control and Status Register */
  volatile uint32_t LOAD;                    /*!< Offset: 0x004 (R/W)  SysTick Reload Value Register       */
  volatile uint32_t VAL;                     /*!< Offset: 0x008 (R/W)  SysTick Current Value Register      */
  volatile  uint32_t CALIB;                   /*!< Offset: 0x00C (R/ )  SysTick Calibration Register        */
} SysTick_Type;

/** \brief  Structure type to access the System Control Block (SCB).
 */
 typedef struct
 {
	volatile  uint32_t CPUID;                  /*!< Offset: 0x000 (R/ )  CPUID Base Register */
	volatile uint32_t ICSR;                   /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register */
	uint32_t RESERVED0;
	volatile uint32_t AIRCR;                  /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register */
	volatile uint32_t SCR;                    /*!< Offset: 0x010 (R/W)  System Control Register */
	volatile uint32_t CCR;                    /*!< Offset: 0x014 (R/W)  Configuration Control Register */
	uint32_t RESERVED1;
	volatile uint32_t SHP[2U];                /*!< Offset: 0x01C (R/W)  System Handlers Priority Registers. [0] is RESERVED */
	volatile uint32_t SHCSR;                  /*!< Offset: 0x024 (R/W)  System Handler Control and State Register */
 } SCB_Type;

#define SysTick_BASE        (0xE000E000UL +  0x0010UL)
#define SCB_BASE            (0xE000E000UL +  0x0D00UL)

#define SCB                 ((SCB_Type       *)     SCB_BASE      )   /*!< SCB configuration struct           */
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )   /*!< SysTick configuration struct       */

#define SHP_PENDSV_PRIO           0
#define SHP_SYSTICK_SVCAKK_PRIO   1

//These functions below were extracted from cmsis to help on avoiding dependency of full
//blown cmsis stack.

static inline __attribute__((__always_inline__)) uint32_t __get_PRIMASK(void)
{
  uint32_t result;
  __asm volatile ("MRS %0, primask" : "=r" (result) );
  return(result);
}

static inline __attribute__((__always_inline__)) void __set_PRIMASK(uint32_t priMask)
{
  __asm volatile ("MSR primask, %0 " : "=r" (priMask) );
}

static inline __attribute__((__always_inline__)) void __disable_irq(void)
{
  __asm volatile ("cpsid i" : : : "memory");
}

static inline __attribute__((__always_inline__))  uint32_t __get_IPSR(void)
{
  uint32_t result;
  __asm volatile ("MRS %0, ipsr" : "=r" (result) );
  return(result);
}