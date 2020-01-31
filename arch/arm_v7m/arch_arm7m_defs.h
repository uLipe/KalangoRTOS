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
  volatile  uint32_t CPUID;                   /*!< Offset: 0x000 (R/ )  CPUID Base Register                                   */
  volatile uint32_t ICSR;                    /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register                  */
  volatile uint32_t VTOR;                    /*!< Offset: 0x008 (R/W)  Vector Table Offset Register                          */
  volatile uint32_t AIRCR;                   /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register      */
  volatile uint32_t SCR;                     /*!< Offset: 0x010 (R/W)  System Control Register                               */
  volatile uint32_t CCR;                     /*!< Offset: 0x014 (R/W)  Configuration Control Register                        */
  volatile uint8_t  SHP[12];                 /*!< Offset: 0x018 (R/W)  System Handlers Priority Registers (4-7, 8-11, 12-15) */
  volatile uint32_t SHCSR;                   /*!< Offset: 0x024 (R/W)  System Handler Control and State Register             */
  volatile uint32_t CFSR;                    /*!< Offset: 0x028 (R/W)  Configurable Fault Status Register                    */
  volatile uint32_t HFSR;                    /*!< Offset: 0x02C (R/W)  HardFault Status Register                             */
  volatile uint32_t DFSR;                    /*!< Offset: 0x030 (R/W)  Debug Fault Status Register                           */
  volatile uint32_t MMFAR;                   /*!< Offset: 0x034 (R/W)  MemManage Fault Address Register                      */
  volatile uint32_t BFAR;                    /*!< Offset: 0x038 (R/W)  BusFault Address Register                             */
  volatile uint32_t AFSR;                    /*!< Offset: 0x03C (R/W)  Auxiliary Fault Status Register                       */
  volatile  uint32_t PFR[2];                  /*!< Offset: 0x040 (R/ )  Processor Feature Register                            */
  volatile  uint32_t DFR;                     /*!< Offset: 0x048 (R/ )  Debug Feature Register                                */
  volatile  uint32_t ADR;                     /*!< Offset: 0x04C (R/ )  Auxiliary Feature Register                            */
  volatile  uint32_t MMFR[4];                 /*!< Offset: 0x050 (R/ )  Memory Model Feature Register                         */
  volatile  uint32_t ISAR[5];                 /*!< Offset: 0x060 (R/ )  Instruction Set Attributes Register                   */
  volatile  uint32_t RESERVED0[5];
  volatile  uint32_t CPACR;                   /*!< Offset: 0x088 (R/W)  Coprocessor Access Control Register                   */
} SCB_Type;

/**
  \brief  Structure type to access the Floating Point Unit (FPU).
 */
typedef struct
{
    uint32_t RESERVED0[1U];
  volatile uint32_t FPCCR;                  /*!< Offset: 0x004 (R/W)  Floating-Point Context Control Register */
  volatile uint32_t FPCAR;                  /*!< Offset: 0x008 (R/W)  Floating-Point Context Address Register */
  volatile uint32_t FPDSCR;                 /*!< Offset: 0x00C (R/W)  Floating-Point Default Status Control Register */
  volatile uint32_t MVFR0;                  /*!< Offset: 0x010 (R/ )  Media and FP Feature Register 0 */
  volatile uint32_t MVFR1;                  /*!< Offset: 0x014 (R/ )  Media and FP Feature Register 1 */
} FPU_Type;

#define SysTick_BASE        (0xE000E000UL +  0x0010UL)
#define SCB_BASE            (0xE000E000UL +  0x0D00UL)
#define FPU_BASE            (0xE000E000UL +  0x0F30UL)  

#define SCB                 ((SCB_Type       *)     SCB_BASE      )   /*!< SCB configuration struct           */
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )   /*!< SysTick configuration struct       */

#ifdef CONFIG_HAS_FLOAT
#define FPU                 ((FPU_Type       *)     FPU_BASE      )   /*!< Floating Point Unit */
#endif

#define SHP_SVCALL_PRIO     7
#define SHP_PENDSV_PRIO     10
#define SHP_SYSTICK_PRIO    11

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