# Como Fazer um Microkernel para TriCore TC2xx

## Documento Técnico Completo de Implementação

**Versão:** 2.0 (Consolidado)
**Arquitetura:** Infineon TriCore TC1.6.1+ (AURIX TC2xx)
**Toolchain:** GCC (tricore-elf-gcc)
**Plataforma de teste:** QEMU TriCore + Hardware TC2xx real

---

## Índice

1.  [Fundamentos e Motivação](#capítulo-1-fundamentos-e-motivação)
2.  [Arquitetura TriCore em Detalhe](#capítulo-2-arquitetura-tricore-em-detalhe)
3.  [Layout de Memória e Linker Script](#capítulo-3-layout-de-memória-e-linker-script)
4.  [Proteção de Memória (MPU TriCore)](#capítulo-4-proteção-de-memória)
5.  [Alocador de Memória Física](#capítulo-5-alocador-de-memória-física)
6.  [Memory Domains (Variáveis Globais Protegidas)](#capítulo-6-memory-domains)
7.  [API de MMAP](#capítulo-7-api-de-mmap)
8.  [Thread Control Block e Context Switch](#capítulo-8-thread-control-block-e-context-switch)
9.  [IPC (Inter-Process Communication)](#capítulo-9-ipc)
10. [Interrupções em Userspace](#capítulo-10-interrupções-em-userspace)
11. [Driver Completo em Userspace](#capítulo-11-driver-completo-em-userspace)
12. [Inicialização do Sistema](#capítulo-12-inicialização-do-sistema)
13. [Budget de Ranges MPU](#capítulo-13-budget-de-ranges-mpu)
14. [Payloads Grandes (Shared Memory e Grants)](#capítulo-14-payloads-grandes)
15. [Syscall Handler](#capítulo-15-syscall-handler)
16. [Primitivas de Sincronização em Userspace](#capítulo-16-primitivas-de-sincronização-em-userspace)
17. [Multi-Core (Componente Sobre o Microkernel)](#capítulo-17-multi-core)
18. [QEMU TriCore: Setup e Debug](#capítulo-18-qemu-tricore)
19. [Resumo e Referências](#capítulo-19-resumo-e-referências)

---

# Capítulo 1: Fundamentos e Motivação

## 1.1 Por Que um Microkernel em MCU?

A maioria dos RTOS embarcados (FreeRTOS, Zephyr em modo flat, ERIKA)
opera como kernel monolítico: drivers, stacks de protocolo e código de
aplicação rodam todos no mesmo nível de privilégio e compartilham o mesmo
espaço de endereçamento. Qualquer ponteiro corrompido derruba o sistema.

Um microkernel inverte essa lógica:

```
MONOLÍTICO (ERIKA típico no TriCore):
┌─────────────────────────────────────────────┐
│            TUDO EM SUPERVISOR               │
│                                             │
│  App + Drivers + Stacks + Kernel            │
│  Mesmo address space, mesmas permissões     │
│                                             │
│  Bug no driver CAN = ECU inteira morre      │
└─────────────────────────────────────────────┘

MICROKERNEL:
┌──────────┐ ┌──────────┐ ┌──────────┐
│  App A   │ │ASCLIN Drv│ │ QSPI Drv │   ← Userspace (User-0 / User-1)
│ (isolada)│ │(isolado) │ │(isolado) │     Cada um em seu address space
└────┬─────┘ └────┬─────┘ └────┬─────┘
     │            │            │
     └──────┬─────┴─────┬──────┘
            │   IPC     │
     ┌──────▼───────────▼──────┐
     │       KERNEL            │   ← Supervisor (IO=2, mínimo)
     │  Scheduler + IPC + MPU  │     Só o essencial
     │    ~2000 linhas de C    │
     └─────────────────────────┘

  Bug no driver ASCLIN → kernel mata o driver
                       → reinicia o driver
                       → App A nem percebeu
```

### 1.1.1 Por Que TriCore?

TriCore é a arquitetura dominante em ECUs automotivas (Infineon AURIX).
Se você trabalha com automotive, é quase certo que vai encontrar TC2xx ou
TC3xx. E se vai fazer um microkernel embarcado para segurança funcional
(ISO 26262), TriCore é o alvo natural.

```
ONDE TRICORE APARECE:
  - Engine Control Units (ECU)
  - Transmission controllers
  - Battery Management Systems (BMS)
  - ADAS (Advanced Driver Assistance)
  - Qualquer coisa ASIL-B a ASIL-D no carro

CHIPS COMUNS:
  TC275:  3 cores, 4MB Flash, 472KB RAM
  TC277:  3 cores, 4MB Flash, 472KB RAM + HSM
  TC297:  3 cores, 8MB Flash, 728KB RAM
  TC299:  6 cores (3 perf + 3 lockstep), 16MB Flash, 1.5MB RAM
```

### 1.1.2 O Que Ganhamos

| Propriedade | Monolítico | Microkernel |
|---|---|---|
| Isolamento de falhas | Nenhum | Por hardware (MPU) |
| Driver crashou | Sistema morreu | Kernel reinicia o driver |
| Superfície de ataque | Tudo privilegiado | Só o kernel (~2K LOC) |
| Overhead de comunicação | Zero (chamada direta) | ~0.3μs IPC round-trip (TriCore) |
| Complexidade do kernel | Alta | Baixa |
| Complexidade total | Média | Alta (IPC, protocolos) |

### 1.1.3 O Que Perdemos

- **Overhead de IPC**: cada chamada a driver é uma troca de contexto
  (~50 ciclos por direção no TriCore, ~150 no ARM)
- **Complexidade arquitetural**: protocolos de IPC, capabilities
- **Mais RAM pra stacks**: cada thread precisa de stack isolada
- **Sem endereçamento virtual**: endereços são físicos

### 1.1.4 Quando Faz Sentido

- Sistemas com requisitos de segurança (ISO 26262, IEC 62443)
- Dispositivos conectados (driver de rede isolado)
- Atualização parcial (trocar driver sem reiniciar)
- Multi-tenant (múltiplos fornecedores no mesmo ECU)

### 1.1.5 Componentes do Kernel

O kernel contém apenas:

| Componente | Responsabilidade | ~LOC |
|---|---|---|
| **Scheduler** | Escolher qual thread roda | 200-400 |
| **IPC** | Transferir mensagens entre threads | 300-500 |
| **MPU Manager** | Configurar proteção no context switch | 150-300 |
| **Physical Allocator** | Alocar regiões de SRAM alinhadas | 200-400 |
| **IRQ Dispatcher** | Receber IRQ e notificar driver user | 100-200 |
| **Capability Manager** | Controlar quem acessa o quê | 100-200 |
| **Syscall Handler** | Decodificar e despachar traps | 100-200 |
| **Total** | | **~1500-2000** |

Tudo mais — drivers, protocolos, lógica de aplicação — roda em userspace.

---

## 1.2 TriCore vs ARM Cortex-M: Mapa Mental

```
╔════════════════════════════╦══════════════════╦═══════════════════╗
║  Conceito                  ║  ARM Cortex-M    ║  TriCore TC1.6    ║
╠════════════════════════════╬══════════════════╬═══════════════════╣
║  Privilege levels          ║  2 (Priv/User)   ║  3 (SV/U1/U0)    ║
║  Exceção pra syscall       ║  SVC (imediato)  ║  SYSCALL (trap 6) ║
║  Proteção de memória       ║  MPU (8-16 reg)  ║  DPR+CPR separados║
║                            ║  power-of-2 size ║  any size, 8B aln ║
║  Registradores             ║  R0-R12,SP,LR,PC ║  D0-D15,A0-A15,PC║
║  Context save              ║  Manual (push/   ║  AUTOMÁTICO (CSA) ║
║                            ║   pop no PendSV) ║  ~5 instruções    ║
║  Interrupções              ║  NVIC            ║  ICU + SRC regs   ║
║  Stack pointer             ║  MSP + PSP       ║  A10(SP) + ISP    ║
║  Link register             ║  LR (R14)        ║  A11 (RA)         ║
║  Multi-core                ║  Raro em MCU     ║  2-6 cores padrão ║
║  MPU switch cost           ║  ~50 cycles      ║  ~3 cycles (PRS!) ║
╚════════════════════════════╩══════════════════╩═══════════════════╝
```

---

# Capítulo 2: Arquitetura TriCore em Detalhe

## 2.1 Registradores

```
DATA REGISTERS (32-bit cada):
  D0-D7:   Lower context (salvos em CALL)
  D8-D15:  Upper context (salvos em trap/interrupt)
  D15:     Implicit data register

  Extended (64-bit pairs):
  E0 = D1:D0, E2 = D3:D2, ... E14 = D15:D14

ADDRESS REGISTERS (32-bit cada):
  A0-A7:   Lower context
  A8-A15:  Upper context

  Especiais:
  A0,A1:  Global Address Registers (small data)
  A8,A9:  Global Address Registers (small data)
  A10:    Stack Pointer (SP)
  A11:    Return Address (RA)
  A14:    Frame Pointer (opcional)
  A15:    Implicit address register

SYSTEM REGISTERS (via MFCR/MTCR):
  PSW:   Program Status Word
         PSW.IO [11:10]:  I/O privilege level (0-2)
         PSW.IS [9]:      Interrupt Stack flag
         PSW.GW [8]:      Global Write
         PSW.CDE [7]:     Call Depth Enable
         PSW.CDC [6:0]:   Call Depth Counter
         PSW.PRS [15:14]: Protection Register Set (0-3)
         PSW.S [30]:      Safety bit
         PSW.USB [31:28]: User Status Bits

  PCXI:  Previous Context Information
         Link Word → ponteiro pro CSA anterior
         PIE: Previous Interrupt Enable
         PCPN: Previous CPU Priority Number
         UL: Upper/Lower flag

  PC:    Program Counter
  FCX:   Free Context List head
  LCX:   Limit Context (underflow warning)
  ICR:   Interrupt Control Register
         ICR.IE: Interrupt Enable
         ICR.CCPN: Current CPU Priority Number
  ISP:   Interrupt Stack Pointer
  BTV:   Base of Trap Vector Table
  BIV:   Base of Interrupt Vector Table
  SYSCON: System Configuration Register
```

## 2.2 A Joia da Coroa: CSA (Context Save Area)

A diferença mais impactante para um microkernel: TriCore salva e restaura
contexto automaticamente em hardware usando CSA.

```
ARM Cortex-M context switch:
  1. Salvar r4-r11 manualmente no stack
  2. Salvar SP no TCB
  3. Trocar SP
  4. Restaurar r4-r11 do novo stack
  ~30 instruções de push/pop

TriCore context switch:
  Hardware faz automaticamente:
  1. Toda chamada de função → SVLCX (Save Lower Context)
  2. Toda interrupção/trap → SVLCX + SVUCX (Save Upper + Lower)
  3. Contexto salvo numa LISTA ENCADEADA de CSAs
  4. PCXI register aponta pro topo da lista

  Para trocar de thread, basta:
  1. Salvar PCXI atual no TCB
  2. Carregar PCXI da nova thread
  3. RSLCX / RFE restaura tudo
  ~5 instruções!
```

```
ESTRUTURA DO CSA (Context Save Area):

Cada CSA = 64 bytes (16 words), alinhado a 64 bytes.

Upper Context (salvo automaticamente em trap/interrupt):
  ┌──────────────────────────────────────────────┐
  │  PCXI  │  PSW   │  A10(SP)│  A11(RA)│       │
  │  D8    │  D9    │  D10   │  D11   │       │
  │  A12   │  A13   │  A14   │  A15   │       │
  │  D12   │  D13   │  D14   │  D15   │       │
  └──────────────────────────────────────────────┘

Lower Context (salvo por SVLCX/BISR):
  ┌──────────────────────────────────────────────┐
  │  PCXI  │  A2    │  A3    │  D0    │       │
  │  D1    │  D2    │  D3    │  A4    │       │
  │  A5    │  A6    │  A7    │  D4    │       │
  │  D5    │  D6    │  D7    │  --    │       │
  └──────────────────────────────────────────────┘

CSAs formam LISTA ENCADEADA:
  PCXI → [CSA 0] → [CSA 1] → [CSA 2] → ... → NULL
  FCX = primeiro CSA livre
  LCX = limite (aviso de quase sem CSAs)
```

## 2.3 Modelo de Privilégio TriCore

```
PSW.IO (I/O Privilege Level):

  IO = 0: User-0 (mínimo privilégio)
           Sem acesso a periféricos
           Sem registros de sistema
           MPU protege memória

  IO = 1: User-1
           Acesso a periféricos "comuns"
           Sem registros de sistema

  IO = 2: Supervisor
           Acesso total
           Pode modificar MPU, CSFRs

PARA NOSSO MICROKERNEL:
  Kernel   → IO = 2 (Supervisor)
  Drivers  → IO = 1 (User-1, acesso a periféricos!)
  Apps     → IO = 0 (User-0, mínimo)

Melhor que ARM que só tem 2 níveis!
Temos nível intermediário perfeito pra drivers.
```

## 2.4 Traps e Syscalls

```
Trap Classes (TIN = Trap Identification Number):
  Class 0: MMU (não existe em TC2xx)
  Class 1: Internal Protection (MPU violation!)
  Class 2: Instruction Errors
  Class 3: Context Management (CSA)
  Class 4: System Bus Errors
  Class 5: Assertion Traps
  Class 6: System Call (SYSCALL instruction!)
  Class 7: NMI

SYSCALL:
  Instrução: SYSCALL #imm
  - Salva Upper Context automaticamente (→ CSA)
  - PSW.IO → Supervisor (automático!)
  - PC → trap handler classe 6
  - TIN = valor imediato
  - D15 = System Call Number (convenção)

  Retorno: RFE (Return From Exception)
  - Restaura Upper Context do CSA
  - Restaura PSW (volta pra User mode)
  - PC restaurado automaticamente
```

## 2.5 Mapa de Memória TC2xx

```
ENDEREÇOS IMPORTANTES TC275:
  PFlash:      0x80000000 (non-cached), 0xA0000000 (cached), 4MB
  DSPR Core0:  0x70000000 (global), 0xD0000000 (local), 240KB
  DSPR Core1:  0x60000000 (global), 0xD0000000 (local), 120KB
  DSPR Core2:  0x50000000 (global), 0xD0000000 (local), 120KB
  PSPR Core0:  0x70100000 (global), 0xC0000000 (local), 32KB
  LMU:         0x90000000, 32KB (shared entre todos os cores)
  Peripherals: 0xF0000000+

NOTA: Cada core tem sua própria DSPR e PSPR.
Local alias (0xD0000000) é visível só pelo core local.
Global alias (0x70000000) é visível por todos.
```

## 2.6 MPU do TriCore

```
DIFERENÇAS FUNDAMENTAIS VS ARM:

1. CODE e DATA protegidos SEPARADAMENTE
   CPR (Code Protection Range): execução
   DPR (Data Protection Range): read/write

2. Ranges são [lower, upper) — QUALQUER tamanho, alinhado a 8 bytes!
   Sem power-of-2. Sem desperdício.

3. 4 Protection Register Sets (PRS)
   Cada PRS = conjunto independente de enable bits
   Trocar PRS = trocar TODOS os enables de uma vez
   PSW.PRS seleciona o set ativo (0-3)

4. Mais ranges que ARM
   Até 18 Data Protection Ranges
   Até 10 Code Protection Ranges

5. MPU switch: trocar PSW.PRS = 1 instrução (~3 cycles)
   vs ARM: reescrever 6+ regiões (~50 cycles)
```

---

# Capítulo 3: Layout de Memória e Linker Script

## 3.1 Princípios para TriCore

```
VANTAGENS sobre ARM:
  1. MPU ranges: qualquer múltiplo de 8 bytes (sem power-of-2!)
  2. Data e Code protection separados
  3. Cada core tem RAM próprio (DSPR)
  4. 4 PRS pré-configuráveis

DECISÕES:
  - Kernel roda no Core 0
  - Kernel code em PFlash (cached: 0xA0000000)
  - Kernel data em DSPR Core 0 (0x70000000 global)
  - User threads: dados em DSPR
  - Stacks em DSPR (rápido, local ao core)
  - CSA pool: reservar região em DSPR
  - Periféricos: 0xF0000000+
  - Alinhamento: 64 bytes (CSA boundary)
```

## 3.2 Linker Script

```linker
/* tc275_microkernel.ld */
OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

MEMORY
{
    PFLASH   (rx)  : ORIGIN = 0xA0000000, LENGTH = 4M
    PFLASH_NC(rx)  : ORIGIN = 0x80000000, LENGTH = 4M
    DSPR0    (rwx) : ORIGIN = 0x70000000, LENGTH = 240K
    DSPR1    (rwx) : ORIGIN = 0x60000000, LENGTH = 120K
    DSPR2    (rwx) : ORIGIN = 0x50000000, LENGTH = 120K
    PSPR0    (rwx) : ORIGIN = 0x70100000, LENGTH = 32K
    LMU      (rwx) : ORIGIN = 0x90000000, LENGTH = 32K
    PERIPH   (rw)  : ORIGIN = 0xF0000000, LENGTH = 256M
}

MPU_ALIGN = 64;

SECTIONS
{
    /* ======== FLASH ======== */

    .bmhd : ALIGN(256) {
        KEEP(*(.bmhd))
    } > PFLASH_NC

    .trap_table : ALIGN(256) {
        _trap_table = .;
        KEEP(*(.trap_table))
    } > PFLASH

    .int_table : ALIGN(256) {
        _int_table = .;
        KEEP(*(.int_table))
    } > PFLASH

    .kernel_text : ALIGN(32) {
        _kernel_text_start = .;
        build/kernel/*.o(.text .text.* .rodata .rodata.*)
        _kernel_text_end = .;
    } > PFLASH

    .app_asclin_text : ALIGN(MPU_ALIGN) {
        _app_asclin_text_start = .;
        build/apps/asclin_driver/*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(MPU_ALIGN);
        _app_asclin_text_end = .;
    } > PFLASH

    .app_qspi_text : ALIGN(MPU_ALIGN) {
        _app_qspi_text_start = .;
        build/apps/qspi_driver/*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(MPU_ALIGN);
        _app_qspi_text_end = .;
    } > PFLASH

    .app_sensor_text : ALIGN(MPU_ALIGN) {
        _app_sensor_text_start = .;
        build/apps/sensor_app/*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(MPU_ALIGN);
        _app_sensor_text_end = .;
    } > PFLASH

    .domain_table : ALIGN(4) {
        _domain_table_start = .;
        KEEP(*(.domain_table))
        _domain_table_end = .;
    } > PFLASH


    /* ======== DSPR0 (Core 0) ======== */

    .kernel_data : ALIGN(MPU_ALIGN) {
        _kernel_data_start = .;
        build/kernel/*.o(.data .data.* .bss .bss.* COMMON)
        . = ALIGN(MPU_ALIGN);
        _kernel_data_end = .;
    } > DSPR0

    .kernel_stack (NOLOAD) : ALIGN(8) {
        _kernel_stack_bottom = .;
        . += 4K;
        _kernel_stack_top = .;
    } > DSPR0

    .isr_stack (NOLOAD) : ALIGN(8) {
        _isr_stack_bottom = .;
        . += 2K;
        _isr_stack_top = .;
    } > DSPR0

    .csa_pool (NOLOAD) : ALIGN(64) {
        _csa_pool_start = .;
        . += 16K;     /* 256 CSAs × 64 bytes */
        _csa_pool_end = .;
    } > DSPR0

    /* Memory Domains */
    .domain_asclin (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_asclin_start = .;
        *(.domain_asclin.data .domain_asclin.data.*)
        *(.domain_asclin.bss  .domain_asclin.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_asclin_end = .;
    } > DSPR0

    .domain_qspi (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_qspi_start = .;
        *(.domain_qspi.data .domain_qspi.data.*)
        *(.domain_qspi.bss  .domain_qspi.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_qspi_end = .;
    } > DSPR0

    .domain_sensor (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_sensor_start = .;
        *(.domain_sensor.data .domain_sensor.data.*)
        *(.domain_sensor.bss  .domain_sensor.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_sensor_end = .;
    } > DSPR0

    .domain_shared_ipc (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_shared_ipc_start = .;
        *(.domain_shared_ipc.data .domain_shared_ipc.data.*)
        *(.domain_shared_ipc.bss  .domain_shared_ipc.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_shared_ipc_end = .;
    } > LMU

    .user_pool (NOLOAD) : ALIGN(MPU_ALIGN) {
        _user_pool_start = .;
        . = ORIGIN(DSPR0) + LENGTH(DSPR0);
        _user_pool_end = .;
    } > DSPR0

    /* Small data areas (TriCore ABI) */
    .sdata : { *(.sdata .sdata.*) } > DSPR0
    .sbss  : { *(.sbss .sbss.*) } > DSPR0
    _small_data_  = ADDR(.sdata) + 0x8000;
    _small_data2_ = ADDR(.sdata) + 0x8000;
    _small_data3_ = ADDR(.sdata) + 0x8000;
    _small_data4_ = ADDR(.sdata) + 0x8000;
}
```

## 3.3 Mapa Visual

```
PFLASH (0xA0000000)
┌────────────────────────────────────────┐
│  BMHD (Boot Mode Header)              │
│  Trap Vector Table (BTV)               │
│  Interrupt Vector Table (BIV)          │
├────────────────────────────────────────┤
│  kernel .text + .rodata                │
├── ALIGN(64) ───────────────────────────┤
│  app_asclin .text + .rodata            │
├── ALIGN(64) ───────────────────────────┤
│  app_qspi .text + .rodata             │
├── ALIGN(64) ───────────────────────────┤
│  app_sensor .text + .rodata            │
└────────────────────────────────────────┘

DSPR Core 0 (0x70000000)
┌────────────────────────────────────────┐
│  kernel .data + .bss                   │ ← Supervisor only
├────────────────────────────────────────┤
│  kernel stack (4KB)                    │
│  ISR stack (2KB)                       │
├── ALIGN(64) ───────────────────────────┤
│  CSA Pool (16KB = 256 CSAs)            │
├── ALIGN(64) ───────────────────────────┤
│  domain_asclin (buffers, state)        │ ← MPU: driver only
├── ALIGN(64) ───────────────────────────┤
│  domain_qspi (buffers, state)          │ ← MPU: driver only
├── ALIGN(64) ───────────────────────────┤
│  domain_sensor (data)                  │ ← MPU: app only
├── ALIGN(64) ───────────────────────────┤
│  USER POOL (stacks, mmap)              │
└────────────────────────────────────────┘

LMU (0x90000000) - Shared all cores
┌────────────────────────────────────────┐
│  domain_shared_ipc                     │
│  (shared entre cores/threads)          │
└────────────────────────────────────────┘
```

---

# Capítulo 4: Proteção de Memória

## 4.1 Registros de Sistema TriCore (MFCR/MTCR)

```c
/* ===== tricore_csfr.h ===== */

#ifndef TRICORE_CSFR_H
#define TRICORE_CSFR_H

#include <stdint.h>

/* System registers */
#define CSFR_PSW        0xFE04
#define CSFR_PCXI       0xFE00
#define CSFR_PC         0xFE08
#define CSFR_SYSCON     0xFE14
#define CSFR_BIV        0xFE20
#define CSFR_BTV        0xFE24
#define CSFR_ISP        0xFE28
#define CSFR_ICR        0xFE2C
#define CSFR_FCX        0xFE38
#define CSFR_LCX        0xFE3C
#define CSFR_CORE_ID    0xFE1C

/* Data Protection Ranges: DPRn_L = 0xC000 + n*8 */
#define CSFR_DPR0_L     0xC000
#define CSFR_DPR0_U     0xC004

/* Code Protection Ranges: CPRn_L = 0xD000 + n*8 */
#define CSFR_CPR0_L     0xD000
#define CSFR_CPR0_U     0xD004

/* Protection Enable per PRS */
#define CSFR_DPRE_0     0xE010
#define CSFR_DPRE_1     0xE014
#define CSFR_DPRE_2     0xE018
#define CSFR_DPRE_3     0xE01C
#define CSFR_DPWE_0     0xE020
#define CSFR_DPWE_1     0xE024
#define CSFR_DPWE_2     0xE028
#define CSFR_DPWE_3     0xE02C
#define CSFR_CPRE_0     0xE000
#define CSFR_CPRE_1     0xE004
#define CSFR_CPRE_2     0xE008
#define CSFR_CPRE_3     0xE00C
#define CSFR_CPXE_0     0xE040
#define CSFR_CPXE_1     0xE044
#define CSFR_CPXE_2     0xE048
#define CSFR_CPXE_3     0xE04C

static inline uint32_t __mfcr(uint32_t csfr)
{
    uint32_t val;
    __asm__ volatile("mfcr %0, %1" : "=d"(val) : "i"(csfr));
    return val;
}

static inline void __mtcr(uint32_t csfr, uint32_t val)
{
    __asm__ volatile("mtcr %0, %1\nisync" : : "i"(csfr), "d"(val) : "memory");
}

static inline void __isync(void) { __asm__ volatile("isync" ::: "memory"); }
static inline void __dsync(void) { __asm__ volatile("dsync" ::: "memory"); }
static inline void __enable(void) { __asm__ volatile("enable" ::: "memory"); }
static inline void __disable(void) { __asm__ volatile("disable" ::: "memory"); }

static inline uint32_t __disable_and_save(void)
{
    uint32_t icr = __mfcr(CSFR_ICR);
    __disable();
    return icr;
}

static inline void __restore(uint32_t icr) { __mtcr(CSFR_ICR, icr); }
static inline uint32_t __get_core_id(void) { return __mfcr(CSFR_CORE_ID) & 0x7; }

#endif
```

## 4.2 Estruturas de Dados

```c
/* ===== mpu_tricore.h ===== */

#ifndef MPU_TRICORE_H
#define MPU_TRICORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PERM_READ       (1 << 0)
#define PERM_WRITE      (1 << 1)
#define PERM_EXEC       (1 << 2)
#define PERM_USER       (1 << 3)

#define NUM_DATA_RANGES     18
#define NUM_CODE_RANGES     10
#define NUM_PRS             4

#define PRS_KERNEL      0
#define PRS_DRIVER_A    1
#define PRS_DRIVER_B    2
#define PRS_APP         3

typedef enum {
    REGION_CODE, REGION_DATA, REGION_STACK,
    REGION_HEAP, REGION_PERIPHERAL, REGION_SHARED, REGION_GRANT,
} region_type_t;

typedef struct mem_region {
    uintptr_t       base;
    size_t          size;
    uint32_t        perms;
    region_type_t   type;
    bool            active;
    uint16_t        owner_pid;
    int8_t          dpr_index;
    int8_t          cpr_index;
} mem_region_t;

#define MAX_REGIONS_PER_THREAD  12

typedef struct address_space {
    mem_region_t    regions[MAX_REGIONS_PER_THREAD];
    uint8_t         region_count;
    uint16_t        pid;
    uint8_t         prs;
} address_space_t;

typedef struct prs_config {
    struct { uint32_t lower; uint32_t upper; } dpr[NUM_DATA_RANGES];
    uint32_t dpre;
    uint32_t dpwe;
    struct { uint32_t lower; uint32_t upper; } cpr[NUM_CODE_RANGES];
    uint32_t cpre;
    uint32_t cpxe;
} prs_config_t;

/* API */
int  as_add_region(address_space_t *as, uintptr_t base, size_t size,
                   uint32_t perms, region_type_t type);
int  as_remove_region(address_space_t *as, uintptr_t base);
bool as_contains(const address_space_t *as, uintptr_t addr, size_t size);

void mpu_init(void);
void mpu_configure_prs(uint8_t prs, const prs_config_t *config);
void mpu_switch_prs(uint8_t prs);
void mpu_switch_to(struct thread *thread);
void mpu_enable(void);
void mpu_disable(void);

#endif
```

## 4.3 Implementação

```c
/* ===== address_space.c ===== */

#include "mpu_tricore.h"
#include <string.h>

int as_add_region(address_space_t *as, uintptr_t base, size_t size,
                  uint32_t perms, region_type_t type)
{
    if (as->region_count >= MAX_REGIONS_PER_THREAD) return -1;

    uintptr_t end = base + size;
    for (int i = 0; i < as->region_count; i++) {
        if (!as->regions[i].active) continue;
        uintptr_t ex_s = as->regions[i].base;
        uintptr_t ex_e = ex_s + as->regions[i].size;
        if (base < ex_e && end > ex_s) {
            if (base == ex_s && size == as->regions[i].size) return 0;
            return -2;
        }
    }

    int idx = as->region_count;
    as->regions[idx] = (mem_region_t){
        .base = base, .size = size, .perms = perms,
        .type = type, .active = true, .owner_pid = as->pid,
        .dpr_index = -1, .cpr_index = -1,
    };
    as->region_count++;
    return idx;
}

int as_remove_region(address_space_t *as, uintptr_t base)
{
    for (int i = 0; i < as->region_count; i++) {
        if (as->regions[i].base == base && as->regions[i].active) {
            for (int j = i; j < as->region_count - 1; j++)
                as->regions[j] = as->regions[j + 1];
            as->region_count--;
            return 0;
        }
    }
    return -1;
}

bool as_contains(const address_space_t *as, uintptr_t addr, size_t size)
{
    uintptr_t end = addr + size;
    for (int i = 0; i < as->region_count; i++) {
        if (!as->regions[i].active) continue;
        uintptr_t rs = as->regions[i].base;
        uintptr_t re = rs + as->regions[i].size;
        if (addr >= rs && end <= re) return true;
    }
    return false;
}
```

```c
/* ===== mpu_tricore.c ===== */

#include "mpu_tricore.h"
#include "tricore_csfr.h"
#include "thread.h"

#define PSW_PRS_SHIFT   14
#define PSW_PRS_MASK    (0x3 << PSW_PRS_SHIFT)
#define PSW_IO_SHIFT    10
#define PSW_IO_MASK     (0x3 << PSW_IO_SHIFT)
#define SYSCON_PROTEN   (1 << 1)

static inline uint32_t dpre_csfr(uint8_t prs) { return CSFR_DPRE_0 + prs*4; }
static inline uint32_t dpwe_csfr(uint8_t prs) { return CSFR_DPWE_0 + prs*4; }
static inline uint32_t cpre_csfr(uint8_t prs) { return CSFR_CPRE_0 + prs*4; }
static inline uint32_t cpxe_csfr(uint8_t prs) { return CSFR_CPXE_0 + prs*4; }
static inline uint32_t dpr_l(uint8_t i) { return CSFR_DPR0_L + i*8; }
static inline uint32_t dpr_u(uint8_t i) { return CSFR_DPR0_U + i*8; }
static inline uint32_t cpr_l(uint8_t i) { return CSFR_CPR0_L + i*8; }
static inline uint32_t cpr_u(uint8_t i) { return CSFR_CPR0_U + i*8; }

void mpu_init(void)
{
    uint32_t syscon = __mfcr(CSFR_SYSCON);
    __mtcr(CSFR_SYSCON, syscon & ~SYSCON_PROTEN);

    for (int i = 0; i < NUM_DATA_RANGES; i++) {
        __mtcr(dpr_l(i), 0); __mtcr(dpr_u(i), 0);
    }
    for (int i = 0; i < NUM_CODE_RANGES; i++) {
        __mtcr(cpr_l(i), 0); __mtcr(cpr_u(i), 0);
    }
    for (int prs = 0; prs < NUM_PRS; prs++) {
        __mtcr(dpre_csfr(prs), 0); __mtcr(dpwe_csfr(prs), 0);
        __mtcr(cpre_csfr(prs), 0); __mtcr(cpxe_csfr(prs), 0);
    }

    /* PRS 0 (Kernel): acesso total */
    __mtcr(dpre_csfr(PRS_KERNEL), 0xFFFFFFFF);
    __mtcr(dpwe_csfr(PRS_KERNEL), 0xFFFFFFFF);
    __mtcr(cpre_csfr(PRS_KERNEL), 0xFFFFFFFF);
    __mtcr(cpxe_csfr(PRS_KERNEL), 0xFFFFFFFF);
}

void mpu_configure_prs(uint8_t prs, const prs_config_t *config)
{
    for (int i = 0; i < NUM_DATA_RANGES; i++) {
        __mtcr(dpr_l(i), config->dpr[i].lower);
        __mtcr(dpr_u(i), config->dpr[i].upper);
    }
    __mtcr(dpre_csfr(prs), config->dpre);
    __mtcr(dpwe_csfr(prs), config->dpwe);

    for (int i = 0; i < NUM_CODE_RANGES; i++) {
        __mtcr(cpr_l(i), config->cpr[i].lower);
        __mtcr(cpr_u(i), config->cpr[i].upper);
    }
    __mtcr(cpre_csfr(prs), config->cpre);
    __mtcr(cpxe_csfr(prs), config->cpxe);
}

void mpu_switch_to(struct thread *thread)
{
    address_space_t *as = &thread->addr_space;

    if (as->prs < NUM_PRS) {
        /* Fast path: PRS pré-configurado (~3 cycles) */
        uint32_t psw = __mfcr(CSFR_PSW);
        psw &= ~(PSW_PRS_MASK | PSW_IO_MASK);
        psw |= ((uint32_t)as->prs << PSW_PRS_SHIFT);
        psw |= ((uint32_t)(thread->privilege == PRIV_KERNEL ? 2 :
                            thread->privilege == PRIV_DRIVER ? 1 : 0)
                << PSW_IO_SHIFT);
        __mtcr(CSFR_PSW, psw);
    } else {
        /* Slow path: reconfigurar ranges no PRS 3 */
        prs_config_t cfg = {0};
        uint8_t d_idx = 0, c_idx = 0;

        for (int i = 0; i < as->region_count; i++) {
            mem_region_t *r = &as->regions[i];
            if (!r->active) continue;

            if (r->type == REGION_CODE) {
                if (c_idx >= NUM_CODE_RANGES) continue;
                cfg.cpr[c_idx].lower = r->base;
                cfg.cpr[c_idx].upper = r->base + r->size;
                if (r->perms & PERM_READ) cfg.cpre |= (1 << c_idx);
                if (r->perms & PERM_EXEC) cfg.cpxe |= (1 << c_idx);
                r->cpr_index = c_idx++;
            } else {
                if (d_idx >= NUM_DATA_RANGES) continue;
                cfg.dpr[d_idx].lower = r->base;
                cfg.dpr[d_idx].upper = r->base + r->size;
                if (r->perms & PERM_READ)  cfg.dpre |= (1 << d_idx);
                if (r->perms & PERM_WRITE) cfg.dpwe |= (1 << d_idx);
                r->dpr_index = d_idx++;
            }
        }

        mpu_configure_prs(3, &cfg);

        uint32_t psw = __mfcr(CSFR_PSW);
        psw &= ~(PSW_PRS_MASK | PSW_IO_MASK);
        psw |= (3 << PSW_PRS_SHIFT);
        psw |= ((uint32_t)(thread->privilege == PRIV_KERNEL ? 2 :
                            thread->privilege == PRIV_DRIVER ? 1 : 0)
                << PSW_IO_SHIFT);
        __mtcr(CSFR_PSW, psw);
    }
}

void mpu_enable(void)
{
    uint32_t syscon = __mfcr(CSFR_SYSCON);
    __mtcr(CSFR_SYSCON, syscon | SYSCON_PROTEN);
}

void mpu_disable(void)
{
    uint32_t syscon = __mfcr(CSFR_SYSCON);
    __mtcr(CSFR_SYSCON, syscon & ~SYSCON_PROTEN);
}
```

## 4.4 Fault Handler (Trap Class 1)

```c
/* ===== fault.c ===== */

#include "tricore_csfr.h"
#include "thread.h"
#include "scheduler.h"

/*
 * Trap Class 1: Internal Protection
 *   TIN 1: PRIV  - Privileged Instruction
 *   TIN 2: MPR   - Memory Protection Read
 *   TIN 3: MPW   - Memory Protection Write
 *   TIN 4: MPX   - Memory Protection Execute
 *   TIN 5: MPP   - Peripheral Protection
 *   TIN 6: MPN   - Null Address
 *   TIN 7: GRWP  - Global Register Write Protection
 */
void trap_class1_handler(void)
{
    uint32_t tin;
    __asm__ volatile("mov %0, %%d15" : "=d"(tin));

    thread_t *current = get_current_thread();

    if (current && current->privilege != PRIV_KERNEL) {
        thread_exit(current);
        scheduler_request_reschedule();
    } else {
        while (1) { __asm__ volatile("debug"); }
    }
}
```

---

# Capítulo 5: Alocador de Memória Física

Como TriCore MPU aceita qualquer tamanho múltiplo de 8, o buddy allocator
pode ter bloco mínimo de 64 bytes (alinhado ao CSA). Menos desperdício
que ARMv7-M (256 bytes mínimo).

```c
/* ===== phys_alloc.h ===== */

#ifndef PHYS_ALLOC_H
#define PHYS_ALLOC_H

#include <stdint.h>
#include <stddef.h>

void      phys_alloc_init(void);
void      phys_alloc_init_at(uintptr_t start, uintptr_t end);
uintptr_t phys_alloc(size_t requested_size);
void      phys_free(uintptr_t addr, size_t size);
size_t    phys_free_bytes(void);

#endif
```

```c
/* ===== phys_alloc.c ===== */

#include "phys_alloc.h"
#include <string.h>
#include <stdbool.h>

extern uint8_t _user_pool_start[];
extern uint8_t _user_pool_end[];

#define MIN_BLOCK_SHIFT  6                              /* 64 bytes */
#define MIN_BLOCK_SIZE   (1u << MIN_BLOCK_SHIFT)
#define MAX_BLOCK_SHIFT  17                             /* 128KB */
#define NUM_ORDERS       (MAX_BLOCK_SHIFT - MIN_BLOCK_SHIFT + 1)

typedef struct block_header {
    struct block_header *next;
    uint8_t             order;
    bool                free;
} block_header_t;

static block_header_t *free_lists[NUM_ORDERS];
static uintptr_t pool_base, pool_end;
static size_t    total_free;

static void init_pool(uintptr_t start, uintptr_t end)
{
    pool_base = (start + MIN_BLOCK_SIZE - 1) & ~(MIN_BLOCK_SIZE - 1);
    pool_end  = end;
    total_free = 0;

    for (int i = 0; i < NUM_ORDERS; i++) free_lists[i] = NULL;

    uintptr_t addr = pool_base;
    while (addr + MIN_BLOCK_SIZE <= pool_end) {
        uint8_t order = 0;
        for (int o = NUM_ORDERS - 1; o >= 0; o--) {
            size_t sz = MIN_BLOCK_SIZE << o;
            if ((addr & (sz - 1)) == 0 && addr + sz <= pool_end) {
                order = o; break;
            }
        }
        size_t bs = MIN_BLOCK_SIZE << order;
        block_header_t *blk = (block_header_t *)addr;
        blk->order = order; blk->free = true;
        blk->next = free_lists[order]; free_lists[order] = blk;
        total_free += bs; addr += bs;
    }
}

void phys_alloc_init(void) { init_pool((uintptr_t)_user_pool_start, (uintptr_t)_user_pool_end); }
void phys_alloc_init_at(uintptr_t start, uintptr_t end) { init_pool(start, end); }

uintptr_t phys_alloc(size_t req)
{
    size_t sz = MIN_BLOCK_SIZE; uint8_t order = 0;
    while (sz < req && order < NUM_ORDERS-1) { sz <<= 1; order++; }
    if (sz < req) return 0;

    uint8_t fo = order;
    while (fo < NUM_ORDERS && !free_lists[fo]) fo++;
    if (fo >= NUM_ORDERS) return 0;

    block_header_t *blk = free_lists[fo]; free_lists[fo] = blk->next;
    while (fo > order) {
        fo--;
        size_t half = MIN_BLOCK_SIZE << fo;
        block_header_t *buddy = (block_header_t *)((uintptr_t)blk + half);
        buddy->order = fo; buddy->free = true;
        buddy->next = free_lists[fo]; free_lists[fo] = buddy;
    }
    blk->order = order; blk->free = false;
    total_free -= (MIN_BLOCK_SIZE << order);
    return (uintptr_t)blk;
}

void phys_free(uintptr_t addr, size_t size)
{
    uint8_t order = 0; size_t s = MIN_BLOCK_SIZE;
    while (s < size) { s <<= 1; order++; }
    block_header_t *blk = (block_header_t *)addr;

    while (order < NUM_ORDERS - 1) {
        size_t bs = MIN_BLOCK_SIZE << order;
        uintptr_t ba = addr ^ bs;
        if (ba < pool_base || ba >= pool_end) break;
        block_header_t *buddy = (block_header_t *)ba;
        if (!buddy->free || buddy->order != order) break;
        block_header_t **pp = &free_lists[order];
        while (*pp && *pp != buddy) pp = &(*pp)->next;
        if (*pp) *pp = buddy->next;
        if (ba < addr) { addr = ba; blk = buddy; }
        order++;
    }
    blk->order = order; blk->free = true;
    blk->next = free_lists[order]; free_lists[order] = blk;
    total_free += (MIN_BLOCK_SIZE << order);
}

size_t phys_free_bytes(void) { return total_free; }
```

---

# Capítulo 6: Memory Domains

```c
/* ===== mem_domain.h ===== */

#ifndef MEM_DOMAIN_H
#define MEM_DOMAIN_H

#include <stdint.h>
#include <stddef.h>
#include "mpu_tricore.h"

#define DOMAIN_DATA(name) __attribute__((section(".domain_" #name ".data")))
#define DOMAIN_BSS(name)  __attribute__((section(".domain_" #name ".bss")))

typedef struct mem_domain {
    const char *name;
    uintptr_t   data_start;
    uintptr_t   data_end;
    uint32_t    perms;
} mem_domain_t;

#define DEFINE_MEM_DOMAIN(dname, permissions)                       \
    extern uint8_t _domain_##dname##_start[];                      \
    extern uint8_t _domain_##dname##_end[];                        \
    const mem_domain_t __domain_desc_##dname                       \
        __attribute__((section(".domain_table"), used)) = {        \
        .name       = #dname,                                      \
        .data_start = (uintptr_t)_domain_##dname##_start,         \
        .data_end   = (uintptr_t)_domain_##dname##_end,           \
        .perms      = (permissions),                               \
    }

#define PRIVATE      DOMAIN_BSS(MODULE_NAME)
#define PRIVATE_INIT DOMAIN_DATA(MODULE_NAME)

struct thread;
int thread_add_domain(struct thread *t, const mem_domain_t *domain);
int thread_share_domain(struct thread *t, const mem_domain_t *domain,
                        uint32_t perms_override);

#endif
```

```c
/* ===== mem_domain.c ===== */

#include "mem_domain.h"
#include "thread.h"

/* TriCore: tamanho não precisa ser po2! Alinhamento de 8 basta. */
static size_t align_up(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

int thread_add_domain(struct thread *t, const mem_domain_t *domain)
{
    uintptr_t base = domain->data_start;
    size_t raw = domain->data_end - domain->data_start;
    if (raw == 0) return -1;
    size_t sz = align_up(raw, 8);  /* TriCore: múltiplo de 8! */

    if (t->addr_space.region_count >= MAX_REGIONS_PER_THREAD) return -3;
    return as_add_region(&t->addr_space, base, sz, domain->perms, REGION_DATA);
}

int thread_share_domain(struct thread *t, const mem_domain_t *domain,
                        uint32_t perms_override)
{
    uint32_t effective = (domain->perms & perms_override) | PERM_USER;
    size_t sz = align_up(domain->data_end - domain->data_start, 8);
    return as_add_region(&t->addr_space, domain->data_start, sz,
                         effective, REGION_SHARED);
}
```

---

# Capítulo 7: API de MMAP

## 7.1 Syscall Wrappers (TriCore)

```c
/* ===== user_syscall.h ===== */

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define MMAP_ANONYMOUS  (1 << 0)
#define MMAP_PERIPHERAL (1 << 1)
#define MMAP_SHARED     (1 << 2)

/* Syscall numbers */
#define SYS_MMAP            1
#define SYS_MUNMAP          2
#define SYS_EP_CREATE       30
#define SYS_SEND            31
#define SYS_RECV            32
#define SYS_CALL            33
#define SYS_REPLY           34
#define SYS_REPLY_RECV      35
#define SYS_NOTIFY_CREATE   40
#define SYS_NOTIFY_SIGNAL   41
#define SYS_NOTIFY_WAIT     42
#define SYS_NOTIFY_POLL     43
#define SYS_RECV_WITH_NOTIFY 50
#define SYS_IRQ_BIND        60
#define SYS_IRQ_ENABLE      61
#define SYS_IRQ_ACK         63
#define SYS_YIELD           10
#define SYS_EXIT            11

/*
 * TriCore syscall convention:
 *   D4-D7: arguments
 *   D2:    return value
 *   SYSCALL #n → trap class 6, TIN = n
 */

#define SYSCALL_0(nr, ret) do { \
    register uint32_t _d2 __asm__("d2"); \
    __asm__ volatile("syscall %1" : "=d"(_d2) : "i"(nr) : "memory"); \
    ret = _d2; \
} while(0)

#define SYSCALL_1(nr, a0, ret) do { \
    register uint32_t _d4 __asm__("d4") = (uint32_t)(a0); \
    register uint32_t _d2 __asm__("d2"); \
    __asm__ volatile("syscall %1" : "=d"(_d2) : "i"(nr), "d"(_d4) : "memory"); \
    ret = _d2; \
} while(0)

#define SYSCALL_2(nr, a0, a1, ret) do { \
    register uint32_t _d4 __asm__("d4") = (uint32_t)(a0); \
    register uint32_t _d5 __asm__("d5") = (uint32_t)(a1); \
    register uint32_t _d2 __asm__("d2"); \
    __asm__ volatile("syscall %1" : "=d"(_d2) : "i"(nr), "d"(_d4), "d"(_d5) : "memory"); \
    ret = _d2; \
} while(0)

#define SYSCALL_3(nr, a0, a1, a2, ret) do { \
    register uint32_t _d4 __asm__("d4") = (uint32_t)(a0); \
    register uint32_t _d5 __asm__("d5") = (uint32_t)(a1); \
    register uint32_t _d6 __asm__("d6") = (uint32_t)(a2); \
    register uint32_t _d2 __asm__("d2"); \
    __asm__ volatile("syscall %1" : "=d"(_d2) : "i"(nr), "d"(_d4), "d"(_d5), "d"(_d6) : "memory"); \
    ret = _d2; \
} while(0)

#define SYSCALL_4(nr, a0, a1, a2, a3, ret) do { \
    register uint32_t _d4 __asm__("d4") = (uint32_t)(a0); \
    register uint32_t _d5 __asm__("d5") = (uint32_t)(a1); \
    register uint32_t _d6 __asm__("d6") = (uint32_t)(a2); \
    register uint32_t _d7 __asm__("d7") = (uint32_t)(a3); \
    register uint32_t _d2 __asm__("d2"); \
    __asm__ volatile("syscall %1" : "=d"(_d2) : "i"(nr), "d"(_d4), "d"(_d5), "d"(_d6), "d"(_d7) : "memory"); \
    ret = _d2; \
} while(0)

static inline void *sys_mmap(void *hint, size_t sz, uint32_t perms, uint32_t flags) {
    uint32_t r; SYSCALL_4(SYS_MMAP, hint, sz, perms, flags, r); return (void *)r;
}
static inline int sys_munmap(void *addr, size_t sz) {
    int r; SYSCALL_2(SYS_MUNMAP, addr, sz, r); return r;
}
static inline int sys_ep_create(void) {
    int r; SYSCALL_0(SYS_EP_CREATE, r); return r;
}
static inline int sys_notify_create(void) {
    int r; SYSCALL_0(SYS_NOTIFY_CREATE, r); return r;
}
static inline void sys_notify_signal(int id, uint32_t bits) {
    int r; SYSCALL_2(SYS_NOTIFY_SIGNAL, id, bits, r); (void)r;
}
static inline uint32_t sys_notify_wait(int id, uint32_t mask) {
    uint32_t r; SYSCALL_2(SYS_NOTIFY_WAIT, id, mask, r); return r;
}
static inline uint32_t sys_notify_poll(int id, uint32_t mask) {
    uint32_t r; SYSCALL_2(SYS_NOTIFY_POLL, id, mask, r); return r;
}
static inline int sys_irq_bind(uint16_t irq, int ntfy, uint32_t bit) {
    int r; SYSCALL_3(SYS_IRQ_BIND, irq, ntfy, bit, r); return r;
}
static inline int sys_irq_enable(uint16_t irq) {
    int r; SYSCALL_1(SYS_IRQ_ENABLE, irq, r); return r;
}
static inline int sys_irq_ack(uint16_t irq) {
    int r; SYSCALL_1(SYS_IRQ_ACK, irq, r); return r;
}
static inline void sys_yield(void) {
    int r; SYSCALL_0(SYS_YIELD, r); (void)r;
}
static inline void sys_exit(int code) {
    int r; SYSCALL_1(SYS_EXIT, code, r); (void)r;
}

/* IPC de alto nível — caller prepara ipc_msg_t no TCB antes da syscall */
static inline int sys_call(int ep, void *msg) {
    int r; SYSCALL_2(SYS_CALL, ep, msg, r); return r;
}
static inline int sys_reply(int tid, void *msg) {
    int r; SYSCALL_2(SYS_REPLY, tid, msg, r); return r;
}
static inline int sys_recv_with_notify(int ep, int ntfy, uint32_t mask, void *result) {
    int r; SYSCALL_4(SYS_RECV_WITH_NOTIFY, ep, ntfy, mask, result, r); return r;
}
static inline int sys_register_service(const char *name, int ep) {
    /* Implementado como IPC pro nameserver/MC supervisor */
    (void)name; (void)ep; return 0;
}
static inline int sys_lookup_service(const char *name) {
    (void)name; return -1; /* Stub */
}

#endif
```

## 7.2 Kernel Side (mmap handler)

```c
/* ===== syscall_mmap.c ===== */

#include "thread.h"
#include "phys_alloc.h"
#include "mpu_tricore.h"
#include <string.h>

typedef struct {
    const char *name; uintptr_t base; size_t size; uint32_t perms;
} peripheral_desc_t;

static const peripheral_desc_t periph_table[] = {
    { "ASCLIN0", 0xF0000600, 256, PERM_READ|PERM_WRITE|PERM_USER },
    { "ASCLIN1", 0xF0000700, 256, PERM_READ|PERM_WRITE|PERM_USER },
    { "QSPI0",   0xF0001C00, 512, PERM_READ|PERM_WRITE|PERM_USER },
    { "PORT00",  0xF003A000, 256, PERM_READ|PERM_WRITE|PERM_USER },
    { "STM0",    0xF0000000, 256, PERM_READ|PERM_WRITE|PERM_USER },
    { NULL, 0, 0, 0 }
};

#define MAX_CAPS 8
#define MAX_PROCS 8
static struct {
    uint16_t pid;
    struct { uintptr_t base; uint32_t perms; } caps[MAX_CAPS];
    uint8_t count;
} proc_caps[MAX_PROCS];

static bool check_cap(uint16_t pid, uintptr_t base, uint32_t perms)
{
    for (int p = 0; p < MAX_PROCS; p++) {
        if (proc_caps[p].pid != pid) continue;
        for (int c = 0; c < proc_caps[p].count; c++)
            if (proc_caps[p].caps[c].base == base)
                return (perms & proc_caps[p].caps[c].perms) == perms;
    }
    return false;
}

void grant_periph_capability(uint16_t pid, uintptr_t base, uint32_t perms)
{
    for (int p = 0; p < MAX_PROCS; p++) {
        if (proc_caps[p].pid == pid || proc_caps[p].pid == 0) {
            proc_caps[p].pid = pid;
            int c = proc_caps[p].count;
            if (c < MAX_CAPS) {
                proc_caps[p].caps[c].base = base;
                proc_caps[p].caps[c].perms = perms;
                proc_caps[p].count++;
            }
            return;
        }
    }
}

void *handle_mmap(thread_t *caller, void *hint, size_t size,
                  uint32_t perms, uint32_t flags)
{
    address_space_t *as = &caller->addr_space;
    perms |= PERM_USER;

    if (flags & MMAP_ANONYMOUS) {
        /* TriCore: alinhar a 64 (CSA), não precisa po2 */
        size_t alloc_size = (size + 63) & ~63u;
        if (as->region_count >= MAX_REGIONS_PER_THREAD) return NULL;
        uintptr_t phys = phys_alloc(alloc_size);
        if (!phys) return NULL;
        memset((void *)phys, 0, alloc_size);
        if (perms & PERM_WRITE) perms &= ~PERM_EXEC;  /* W^X */
        int idx = as_add_region(as, phys, alloc_size, perms, REGION_HEAP);
        if (idx < 0) { phys_free(phys, alloc_size); return NULL; }
        return (void *)phys;
    }

    if (flags & MMAP_PERIPHERAL) {
        uintptr_t pb = (uintptr_t)hint;
        const peripheral_desc_t *pd = NULL;
        for (int i = 0; periph_table[i].name; i++)
            if (periph_table[i].base == pb) { pd = &periph_table[i]; break; }
        if (!pd) return NULL;
        if (!check_cap(caller->pid, pb, perms)) return NULL;
        perms &= ~PERM_EXEC;
        size_t ms = (pd->size + 7) & ~7u;
        if (as->region_count >= MAX_REGIONS_PER_THREAD) return NULL;
        if (as_add_region(as, pb, ms, perms, REGION_PERIPHERAL) < 0) return NULL;
        return (void *)pb;
    }

    return NULL;
}

int handle_munmap(thread_t *caller, void *addr, size_t size)
{
    address_space_t *as = &caller->addr_space;
    uintptr_t t = (uintptr_t)addr;
    for (int i = 0; i < as->region_count; i++) {
        if (as->regions[i].base != t || !as->regions[i].active) continue;
        if (as->regions[i].type == REGION_CODE || as->regions[i].type == REGION_STACK) return -1;
        if (as->regions[i].type == REGION_HEAP) phys_free(t, as->regions[i].size);
        as_remove_region(as, t);
        return 0;
    }
    return -1;
}
```

---

# Capítulo 8: Thread Control Block e Context Switch

## 8.1 TCB

```c
/* ===== thread.h ===== */

#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <stdbool.h>
#include "mpu_tricore.h"

typedef enum { THREAD_READY, THREAD_RUNNING, THREAD_BLOCKED, THREAD_DEAD } thread_state_t;
typedef enum { PRIV_KERNEL, PRIV_DRIVER, PRIV_USER } privilege_t;
typedef enum {
    BLOCKED_ON_NONE, BLOCKED_ON_SEND, BLOCKED_ON_RECV,
    BLOCKED_ON_CALL, BLOCKED_ON_REPLY, BLOCKED_ON_NOTIFY,
    BLOCKED_ON_RECV_OR_NOTIFY,
} blocked_reason_t;
typedef enum { WAKEUP_IPC, WAKEUP_NOTIFY, WAKEUP_TIMEOUT } wakeup_reason_t;

/* TriCore: CSA faz o trabalho pesado, salvamos mínimo */
typedef struct {
    uint32_t pcxi;
    uint32_t psw;
    uint32_t a10_sp;
    uint32_t a11_ra;
    uint32_t pc;
} thread_context_t;

#define IPC_MSG_REGS 6
typedef struct { uint32_t label; uint32_t words[IPC_MSG_REGS]; } ipc_msg_t;

typedef struct thread {
    thread_context_t    ctx;
    uint16_t tid; uint16_t pid;
    char name[16];
    thread_state_t state; uint8_t priority;
    struct thread *next;
    privilege_t privilege;
    address_space_t addr_space;
    uintptr_t stack_base; size_t stack_size;

    /* IPC */
    ipc_msg_t ipc_msg;
    uint16_t ipc_badge, ipc_from_tid;
    struct thread *ipc_next;
    blocked_reason_t blocked_on;
    uint16_t blocked_ep, blocked_notify, reply_from_tid;
    wakeup_reason_t wakeup_reason;
    uint32_t notify_wait_mask, notify_received;
    uint32_t timeout_ticks;
} thread_t;

thread_t *thread_create_user(const char *name, void (*entry)(void *),
                             void *arg, uint8_t prio, uint16_t pid,
                             privilege_t priv,
                             uintptr_t code_start, size_t code_size,
                             size_t stack_size);
void thread_exit(thread_t *t);
void thread_kill_process(uint16_t pid);
thread_t *get_current_thread(void);
thread_t *thread_get_by_tid(uint16_t tid);
void scheduler_init(void);
void scheduler_enqueue(thread_t *t);
thread_t *scheduler_dequeue(void);
void scheduler_yield(void);
void scheduler_request_preempt(void);
void scheduler_request_reschedule(void);
void scheduler_start(void);

#endif
```

## 8.2 CSA Setup

```c
/* ===== csa.h ===== */

#ifndef CSA_H
#define CSA_H

#include <stdint.h>

typedef struct {
    uint32_t pcxi, psw, a10, a11;
    uint32_t d8, d9, d10, d11;
    uint32_t a12, a13, a14, a15;
    uint32_t d12, d13, d14, d15;
} csa_upper_t;

typedef struct {
    uint32_t pcxi, a2, a3, d0;
    uint32_t d1, d2, d3, a4;
    uint32_t a5, a6, a7, d4;
    uint32_t d5, d6, d7, _res;
} csa_lower_t;

static inline uint32_t addr_to_link(uintptr_t a) {
    return ((a >> 6) & 0xFFFF) | (((a >> 12) & 0xF0000));
}
static inline uintptr_t link_to_addr(uint32_t l) {
    return ((l & 0xF0000) << 12) | ((l & 0xFFFF) << 6);
}

void csa_pool_init(void);

#endif
```

```c
/* ===== csa.c ===== */

#include "csa.h"
#include "tricore_csfr.h"

extern uint8_t _csa_pool_start[], _csa_pool_end[];

void csa_pool_init(void)
{
    uintptr_t start = ((uintptr_t)_csa_pool_start + 63) & ~63u;
    uintptr_t end   = (uintptr_t)_csa_pool_end;

    uint32_t prev = 0;
    uintptr_t addr = start;
    uintptr_t last = start;
    int count = 0;

    while (addr + 64 <= end) { last = addr; addr += 64; count++; }

    addr = last;
    for (int i = count - 1; i >= 0; i--) {
        *(uint32_t *)addr = prev;
        prev = addr_to_link(addr);
        addr -= 64;
    }

    __mtcr(CSFR_FCX, addr_to_link(start));
    __mtcr(CSFR_LCX, addr_to_link(last));
}
```

## 8.3 Context Switch

```c
/* ===== context_switch.c ===== */

#include "thread.h"
#include "tricore_csfr.h"
#include "mpu_tricore.h"

static thread_t *current = NULL;

/*
 * TriCore context switch: ~10 instruções.
 * Hardware já salvou Upper Context via CSA.
 */
void context_switch(void)
{
    if (current) {
        current->ctx.pcxi = __mfcr(CSFR_PCXI);
        if (current->state == THREAD_RUNNING) current->state = THREAD_READY;
        if (current->state == THREAD_READY) scheduler_enqueue(current);
    }

    current = scheduler_dequeue();
    if (!current) {
        while (!current) { __enable(); __asm__ volatile("wait"); __disable(); current = scheduler_dequeue(); }
    }
    current->state = THREAD_RUNNING;

    mpu_switch_to(current);
    __mtcr(CSFR_PCXI, current->ctx.pcxi);
    /* RFE no assembly do trap handler restaura tudo */
}

thread_t *get_current_thread(void) { return current; }
```

## 8.4 Trap Vector Table

```asm
/* ===== trap_table.S ===== */

    .section .trap_table, "ax"
    .align 8
    .globl _trap_table
_trap_table:

/* Class 0: MMU */ .align 5
    svlcx
    movh.a %a14, hi:trap_unhandled
    lea %a14, [%a14]lo:trap_unhandled
    ji %a14

/* Class 1: Protection */ .align 5
    svlcx
    movh.a %a14, hi:trap_class1_handler
    lea %a14, [%a14]lo:trap_class1_handler
    ji %a14

/* Class 2: Instruction */ .align 5
    svlcx
    movh.a %a14, hi:trap_unhandled
    lea %a14, [%a14]lo:trap_unhandled
    ji %a14

/* Class 3: Context */ .align 5
    svlcx
    movh.a %a14, hi:trap_unhandled
    lea %a14, [%a14]lo:trap_unhandled
    ji %a14

/* Class 4: Bus */ .align 5
    svlcx
    movh.a %a14, hi:trap_unhandled
    lea %a14, [%a14]lo:trap_unhandled
    ji %a14

/* Class 5: Assertion */ .align 5
    svlcx
    movh.a %a14, hi:trap_unhandled
    lea %a14, [%a14]lo:trap_unhandled
    ji %a14

/* Class 6: SYSCALL */ .align 5
    svlcx
    movh.a %a14, hi:syscall_handler
    lea %a14, [%a14]lo:syscall_handler
    ji %a14

/* Class 7: NMI */ .align 5
    svlcx
    movh.a %a14, hi:trap_unhandled
    lea %a14, [%a14]lo:trap_unhandled
    ji %a14

    .text
    .globl trap_unhandled
trap_unhandled:
    debug
    j trap_unhandled
```

# Capítulo 9: Thread e Process Lifecycle

## 9.1 Design: Thread Server em Userspace (Path D)

```
A criação e destruição de threads NÃO é feita diretamente por qualquer
thread user. Em vez disso, existe um THREAD SERVER — uma thread
privilegiada em userspace que é a única com acesso às syscalls primitivas
de criação/destruição.

POR QUE:
  - Kernel fica mínimo (uma syscall primitiva: _THREAD_SPAWN)
  - Política de criação = userspace (quotas, limites, prioridade max)
  - MC Supervisor já existe → ele É o thread server
  - Apps não precisam de capabilities especiais
  - Basta falar com o thread server via IPC

FLUXO:
  App quer criar uma thread:
    1. App faz IPC CALL pro Thread Server (MC Supervisor)
    2. Thread Server valida (quota, prioridade, etc.)
    3. Thread Server faz syscall THREAD_SPAWN (ele tem permissão)
    4. Kernel cria a thread
    5. Thread Server retorna TID pro App via IPC REPLY

  Driver crashou e precisa reiniciar:
    1. Kernel detecta fault → notifica Thread Server
    2. Thread Server faz PROC_DESTROY (limpa address space)
    3. Thread Server faz PROC_CREATE + THREAD_SPAWN (recria tudo)
    4. Novo driver começa a rodar

┌──────────────────────────────────────────────────────────────┐
│                    VISÃO DO LIFECYCLE                          │
│                                                                │
│  ┌───────┐    IPC "create"    ┌──────────────────┐            │
│  │ App A │ ──────────────────►│  THREAD SERVER   │            │
│  │(user) │ ◄──────────────────│  (MC Supervisor) │            │
│  └───────┘    IPC reply(tid)  │  privilege: IO=1 │            │
│                                │                  │            │
│                                │  Tem acesso às   │            │
│                                │  syscalls:       │            │
│                                │  • THREAD_SPAWN  │            │
│                                │  • THREAD_KILL   │            │
│                                │  • PROC_CREATE   │            │
│                                │  • PROC_DESTROY  │            │
│                                └────────┬─────────┘            │
│                                         │ SYSCALL              │
│  ═══════════════════════════════════════╪══════════════════    │
│                                         ▼                      │
│                                ┌─────────────────┐             │
│                                │     KERNEL      │             │
│                                │  thread_spawn() │             │
│                                │  proc_create()  │             │
│                                └─────────────────┘             │
└──────────────────────────────────────────────────────────────┘

VANTAGENS DO PATH D:
  ✓ Kernel mínimo (não precisa de capability system pra threads)
  ✓ Política em userspace (quotas, rate limiting, audit log)
  ✓ Thread Server pode negar criação (memória baixa, etc.)
  ✓ Centralizado: fácil de rastrear quem criou o quê
  ✓ Restart de drivers: Thread Server já sabe como recriar
  ✓ Zero mudança nas syscalls que apps normais usam
  
TRADE-OFF:
  ✗ Criar thread = 1 IPC round-trip extra (~0.3μs no TriCore)
  ✗ Thread Server é single point of failure (mas roda no Core 0)
  ✗ Não é "self-service" — app depende do server estar vivo
```

## 9.2 Syscalls Primitivas (Kernel Side)

Estas syscalls são **restritas**: apenas threads com IO=1+ (PRIV_DRIVER)
ou IO=2 (PRIV_KERNEL) podem invocar. Apps em IO=0 recebem -EPERM.

```c
/* ===== Syscall numbers adicionais ===== */

#define SYS_THREAD_SPAWN    70
#define SYS_THREAD_KILL     71
#define SYS_THREAD_SUSPEND  72
#define SYS_THREAD_RESUME   73
#define SYS_THREAD_SET_PRIO 74
#define SYS_THREAD_INFO     75
#define SYS_THREAD_GET_TID  76

#define SYS_PROC_CREATE     80
#define SYS_PROC_DESTROY    81
#define SYS_PROC_ADD_REGION 82
#define SYS_PROC_GRANT_CAP  83
#define SYS_PROC_GRANT_IRQ  84
```

## 9.3 Processo: O Container de Threads

```c
/* ===== process.h ===== */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "mpu_tricore.h"

#define MAX_PROCESSES       16
#define MAX_THREADS_PER_PROC 8

/*
 * "Processo" no nosso microkernel:
 *   - Address space compartilhado entre suas threads
 *   - Conjunto de capabilities (periféricos, IRQs)
 *   - Grupo que pode ser destruído junto
 *
 * Thread é a unidade de execução.
 * Processo é a unidade de isolamento e resource ownership.
 */
typedef struct process {
    uint16_t            pid;
    char                name[16];
    bool                active;

    /* Address space (compartilhado entre threads do processo) */
    address_space_t     addr_space;

    /* Code region base (todas as threads executam daqui) */
    uintptr_t           code_start;
    size_t              code_size;

    /* Privilege level pra threads deste processo */
    privilege_t         privilege;

    /* Capabilities */
    struct {
        uintptr_t base;
        uint32_t  perms;
    } periph_caps[8];
    uint8_t periph_cap_count;

    uint16_t irq_caps[8];
    uint8_t  irq_cap_count;

    /* Threads pertencentes */
    uint16_t            thread_tids[MAX_THREADS_PER_PROC];
    uint8_t             thread_count;

    /* Quotas */
    uint8_t             max_threads;
    size_t              max_memory;      /* Bytes totais alocáveis */
    size_t              used_memory;     /* Bytes atualmente alocados */
} process_t;

process_t *proc_create(const char *name, privilege_t priv,
                       uintptr_t code_start, size_t code_size);
int        proc_destroy(uint16_t pid);
process_t *proc_get_by_pid(uint16_t pid);
int        proc_add_region(uint16_t pid, uintptr_t base, size_t size,
                           uint32_t perms, region_type_t type);
int        proc_grant_periph(uint16_t pid, uintptr_t base, uint32_t perms);
int        proc_grant_irq(uint16_t pid, uint16_t irq_idx);

#endif
```

```c
/* ===== process.c ===== */

#include "process.h"
#include "thread.h"
#include "phys_alloc.h"
#include <string.h>

static process_t proc_table[MAX_PROCESSES];
static uint16_t next_pid = 1;

process_t *proc_create(const char *name, privilege_t priv,
                       uintptr_t code_start, size_t code_size)
{
    process_t *p = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!proc_table[i].active) { p = &proc_table[i]; break; }
    }
    if (!p) return NULL;

    memset(p, 0, sizeof(process_t));
    p->pid = next_pid++;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->active = true;
    p->privilege = priv;
    p->code_start = code_start;
    p->code_size = code_size;
    p->addr_space.pid = p->pid;

    /* Defaults de quota */
    p->max_threads = MAX_THREADS_PER_PROC;
    p->max_memory = 64 * 1024;  /* 64KB default */

    /* Adicionar code region ao address space */
    size_t code_aligned = (code_size + 7) & ~7u;
    as_add_region(&p->addr_space, code_start, code_aligned,
                  PERM_READ | PERM_EXEC | PERM_USER, REGION_CODE);

    return p;
}

int proc_destroy(uint16_t pid)
{
    process_t *p = proc_get_by_pid(pid);
    if (!p) return -1;

    /* Matar todas as threads do processo */
    for (int i = 0; i < p->thread_count; i++) {
        thread_t *t = thread_get_by_tid(p->thread_tids[i]);
        if (t && t->state != THREAD_DEAD) {
            /* Liberar stack */
            if (t->stack_base)
                phys_free(t->stack_base, t->stack_size);
            thread_exit(t);
        }
    }

    /* Liberar regiões HEAP do address space */
    for (int i = 0; i < p->addr_space.region_count; i++) {
        if (p->addr_space.regions[i].type == REGION_HEAP &&
            p->addr_space.regions[i].active) {
            phys_free(p->addr_space.regions[i].base,
                      p->addr_space.regions[i].size);
        }
    }

    /* Marcar processo como inativo */
    p->active = false;
    return 0;
}

process_t *proc_get_by_pid(uint16_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].active && proc_table[i].pid == pid)
            return &proc_table[i];
    return NULL;
}

int proc_add_region(uint16_t pid, uintptr_t base, size_t size,
                    uint32_t perms, region_type_t type)
{
    process_t *p = proc_get_by_pid(pid);
    if (!p) return -1;
    return as_add_region(&p->addr_space, base, size, perms, type);
}

int proc_grant_periph(uint16_t pid, uintptr_t base, uint32_t perms)
{
    process_t *p = proc_get_by_pid(pid);
    if (!p || p->periph_cap_count >= 8) return -1;
    p->periph_caps[p->periph_cap_count].base = base;
    p->periph_caps[p->periph_cap_count].perms = perms;
    p->periph_cap_count++;
    return 0;
}

int proc_grant_irq(uint16_t pid, uint16_t irq_idx)
{
    process_t *p = proc_get_by_pid(pid);
    if (!p || p->irq_cap_count >= 8) return -1;
    p->irq_caps[p->irq_cap_count] = irq_idx;
    p->irq_cap_count++;
    return 0;
}
```

## 9.4 Thread Spawn (Kernel Primitive)

```c
/* ===== thread_lifecycle.c ===== */

#include "thread.h"
#include "process.h"
#include "phys_alloc.h"
#include "tricore_csfr.h"
#include <string.h>

/*
 * Syscall primitiva: criar thread dentro de um processo.
 *
 * RESTRIÇÃO: só callable por threads com IO >= 1
 * (Thread Server / MC Supervisor).
 *
 * A thread criada herda o address space do processo.
 */
int handle_thread_spawn(thread_t *caller,
                        uint16_t target_pid,
                        uintptr_t entry_point,
                        uint32_t arg,
                        size_t stack_size,
                        uint8_t priority)
{
    /* Verificar privilégio do caller */
    if (caller->privilege < PRIV_DRIVER)
        return -1;  /* EPERM: só IO >= 1 pode spawnar */

    /* Encontrar processo alvo */
    process_t *proc = proc_get_by_pid(target_pid);
    if (!proc) return -2;  /* EINVAL: processo não existe */

    /* Verificar quota de threads */
    if (proc->thread_count >= proc->max_threads)
        return -3;  /* EAGAIN: limite de threads */

    /* Verificar quota de memória (stack) */
    size_t stack_aligned = (stack_size + 63) & ~63u;
    if (proc->used_memory + stack_aligned > proc->max_memory)
        return -4;  /* ENOMEM: limite de memória */

    /* Verificar que entry_point está no code region do processo */
    if (!as_contains(&proc->addr_space, entry_point, 4))
        return -5;  /* EFAULT: entry fora do address space */

    /* Alocar stack */
    uintptr_t stack_phys = phys_alloc(stack_aligned);
    if (!stack_phys) return -6;  /* ENOMEM */

    memset((void *)stack_phys, 0xCC, stack_aligned);

    /* Criar thread */
    thread_t *t = thread_create_user(
        proc->name,             /* Herda nome do processo */
        (void (*)(void *))entry_point,
        (void *)arg,
        priority,
        proc->pid,
        proc->privilege,
        proc->code_start,
        proc->code_size,
        stack_aligned
    );

    if (!t) {
        phys_free(stack_phys, stack_aligned);
        return -6;
    }

    /* Copiar address space do processo (exceto stack, que é nova) */
    t->addr_space = proc->addr_space;

    /* Adicionar stack region específica desta thread */
    as_add_region(&t->addr_space, stack_phys, stack_aligned,
                  PERM_READ | PERM_WRITE | PERM_USER, REGION_STACK);

    t->stack_base = stack_phys;
    t->stack_size = stack_aligned;

    /* Registrar thread no processo */
    proc->thread_tids[proc->thread_count++] = t->tid;
    proc->used_memory += stack_aligned;

    return t->tid;
}

/*
 * Matar uma thread específica.
 * Restrição: IO >= 1, ou a thread está matando a si mesma (EXIT).
 */
int handle_thread_kill(thread_t *caller, uint16_t target_tid)
{
    if (caller->privilege < PRIV_DRIVER && caller->tid != target_tid)
        return -1;

    thread_t *target = thread_get_by_tid(target_tid);
    if (!target) return -2;

    /* Limpar de filas IPC se estiver bloqueado */
    /* (Simplificado: thread_exit já cuida disso) */

    /* Liberar stack */
    if (target->stack_base) {
        process_t *proc = proc_get_by_pid(target->pid);
        if (proc) proc->used_memory -= target->stack_size;
        phys_free(target->stack_base, target->stack_size);
        target->stack_base = 0;
    }

    thread_exit(target);
    return 0;
}

/*
 * Suspender thread (remover do scheduler sem matar).
 */
int handle_thread_suspend(thread_t *caller, uint16_t target_tid)
{
    if (caller->privilege < PRIV_DRIVER) return -1;
    thread_t *t = thread_get_by_tid(target_tid);
    if (!t) return -2;
    if (t->state == THREAD_RUNNING || t->state == THREAD_READY)
        t->state = THREAD_BLOCKED;
    t->blocked_on = BLOCKED_ON_NONE;  /* Suspended, não waiting */
    return 0;
}

/*
 * Retomar thread suspensa.
 */
int handle_thread_resume(thread_t *caller, uint16_t target_tid)
{
    if (caller->privilege < PRIV_DRIVER) return -1;
    thread_t *t = thread_get_by_tid(target_tid);
    if (!t) return -2;
    if (t->state == THREAD_BLOCKED && t->blocked_on == BLOCKED_ON_NONE) {
        t->state = THREAD_READY;
        scheduler_enqueue(t);
    }
    return 0;
}

/*
 * Alterar prioridade.
 */
int handle_thread_set_prio(thread_t *caller, uint16_t target_tid,
                           uint8_t new_prio)
{
    if (caller->privilege < PRIV_DRIVER) return -1;
    thread_t *t = thread_get_by_tid(target_tid);
    if (!t) return -2;
    t->priority = new_prio;
    /* Se está ready, reposicionar na ready queue */
    return 0;
}

/*
 * Obter TID da thread chamadora.
 * Única syscall de thread que QUALQUER thread pode chamar.
 */
uint16_t handle_thread_get_tid(thread_t *caller)
{
    return caller->tid;
}

/*
 * Obter info de uma thread.
 */
int handle_thread_info(thread_t *caller, uint16_t target_tid,
                       uint32_t *out_state, uint32_t *out_prio,
                       uint32_t *out_pid)
{
    if (caller->privilege < PRIV_DRIVER) return -1;
    thread_t *t = thread_get_by_tid(target_tid);
    if (!t) return -2;
    *out_state = t->state;
    *out_prio = t->priority;
    *out_pid = t->pid;
    return 0;
}
```

## 9.5 Thread Server Protocol (IPC)

```c
/* ===== thread_server_protocol.h ===== */

/*
 * Protocolo IPC que apps usam pra falar com o Thread Server.
 * O Thread Server (MC Supervisor) é quem realmente invoca
 * as syscalls primitivas.
 */

#define TSRV_MSG_SPAWN          100
#define TSRV_MSG_KILL           101
#define TSRV_MSG_SUSPEND        102
#define TSRV_MSG_RESUME         103
#define TSRV_MSG_SET_PRIO       104
#define TSRV_MSG_GET_INFO       105
#define TSRV_MSG_PROC_CREATE    110
#define TSRV_MSG_PROC_DESTROY   111

/*
 * TSRV_MSG_SPAWN:
 *   request:
 *     words[0] = entry_point
 *     words[1] = arg
 *     words[2] = stack_size
 *     words[3] = priority
 *     words[4] = target_pid (0 = mesmo processo do caller)
 *   reply:
 *     label = 0 (sucesso) ou erro negativo
 *     words[0] = tid criado
 *
 * TSRV_MSG_KILL:
 *   request:
 *     words[0] = target_tid
 *   reply:
 *     label = 0 ou erro
 *
 * TSRV_MSG_PROC_CREATE:
 *   request:
 *     words[0..1] = name (8 bytes)
 *     words[2] = privilege (0=user, 1=driver)
 *     words[3] = code_start
 *     words[4] = code_size
 *     words[5] = max_memory (quota)
 *   reply:
 *     label = 0 ou erro
 *     words[0] = pid criado
 *
 * TSRV_MSG_PROC_DESTROY:
 *   request:
 *     words[0] = target_pid
 *   reply:
 *     label = 0 ou erro
 */
```

## 9.6 Client Library (User API)

```c
/* ===== libthread.h ===== */

#ifndef LIBTHREAD_H
#define LIBTHREAD_H

#include "user_syscall.h"
#include "thread_server_protocol.h"

static int tsrv_ep = -1;

static inline int tsrv_init(void)
{
    if (tsrv_ep < 0)
        tsrv_ep = sys_lookup_service("thread_server");
    return tsrv_ep;
}

/*
 * Criar thread no mesmo processo.
 * App faz IPC pro Thread Server, que faz a syscall real.
 */
static inline int thread_spawn(void (*entry)(void *), void *arg,
                                size_t stack_size, uint8_t priority)
{
    if (tsrv_init() < 0) return -1;

    ipc_msg_t msg = { .label = TSRV_MSG_SPAWN };
    msg.words[0] = (uint32_t)entry;
    msg.words[1] = (uint32_t)arg;
    msg.words[2] = stack_size;
    msg.words[3] = priority;
    msg.words[4] = 0;  /* 0 = mesmo processo */

    sys_call(tsrv_ep, &msg);

    if (msg.label != 0) return msg.label;  /* Erro */
    return (int)msg.words[0];  /* TID */
}

/*
 * Matar uma thread.
 */
static inline int thread_kill(uint16_t tid)
{
    if (tsrv_init() < 0) return -1;

    ipc_msg_t msg = { .label = TSRV_MSG_KILL };
    msg.words[0] = tid;
    sys_call(tsrv_ep, &msg);
    return msg.label;
}

/*
 * Criar processo novo (só faz sentido pra MC Supervisor ou init).
 */
static inline int proc_spawn(const char *name, uint8_t privilege,
                              uintptr_t code_start, size_t code_size,
                              size_t max_memory)
{
    if (tsrv_init() < 0) return -1;

    ipc_msg_t msg = { .label = TSRV_MSG_PROC_CREATE };
    __builtin_memcpy(&msg.words[0], name, 8);
    msg.words[2] = privilege;
    msg.words[3] = code_start;
    msg.words[4] = code_size;
    msg.words[5] = max_memory;

    sys_call(tsrv_ep, &msg);

    if (msg.label != 0) return msg.label;
    return (int)msg.words[0];  /* PID */
}

/*
 * Destruir processo inteiro.
 */
static inline int proc_kill(uint16_t pid)
{
    if (tsrv_init() < 0) return -1;

    ipc_msg_t msg = { .label = TSRV_MSG_PROC_DESTROY };
    msg.words[0] = pid;
    sys_call(tsrv_ep, &msg);
    return msg.label;
}

#endif
```

## 9.7 Thread Server (Dentro do MC Supervisor)

```c
/* ===== Adicionar ao mc_supervisor.c, no switch do server loop ===== */

/* Handlers de Thread Server integrados ao MC Supervisor */

case TSRV_MSG_SPAWN: {
    uint16_t target_pid = msg.words[4];
    if (target_pid == 0) target_pid = caller_pid;  /* Mesmo processo */

    /* Verificação de política: caller pode criar neste processo? */
    /* Regra simples: só pode criar no próprio processo,
       ou ser o MC Supervisor criando em qualquer lugar */
    if (target_pid != caller_pid && caller_pid != MC_SUPERVISOR_PID) {
        reply.label = -1;  /* EPERM */
    } else {
        int tid = handle_thread_spawn(
            get_current_thread(),  /* MC Sup tem IO=1 */
            target_pid,
            msg.words[0],  /* entry */
            msg.words[1],  /* arg */
            msg.words[2],  /* stack_size */
            msg.words[3]   /* priority */
        );

        if (tid < 0) {
            reply.label = tid;  /* Propagar erro */
        } else {
            reply.label = 0;
            reply.words[0] = tid;
        }
    }
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_KILL: {
    uint16_t target_tid = msg.words[0];
    thread_t *target = thread_get_by_tid(target_tid);

    /* Política: caller pode matar thread no próprio processo */
    if (!target) {
        reply.label = -2;
    } else if (target->pid != caller_pid && caller_pid != MC_SUPERVISOR_PID) {
        reply.label = -1;  /* EPERM */
    } else {
        reply.label = handle_thread_kill(get_current_thread(), target_tid);
    }
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_PROC_CREATE: {
    /* Só o MC Supervisor pode criar processos */
    /* Na prática, quem manda este IPC é o próprio MC Sup
       ou um "init" process de confiança */
    char name[16] = {0};
    __builtin_memcpy(name, &msg.words[0], 8);
    privilege_t priv = msg.words[2] == 1 ? PRIV_DRIVER : PRIV_USER;

    process_t *p = proc_create(name, priv, msg.words[3], msg.words[4]);
    if (!p) {
        reply.label = -6;
    } else {
        p->max_memory = msg.words[5];
        reply.label = 0;
        reply.words[0] = p->pid;
    }
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_PROC_DESTROY: {
    uint16_t target_pid = msg.words[0];
    reply.label = proc_destroy(target_pid);
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_SUSPEND: {
    reply.label = handle_thread_suspend(get_current_thread(), msg.words[0]);
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_RESUME: {
    reply.label = handle_thread_resume(get_current_thread(), msg.words[0]);
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_SET_PRIO: {
    reply.label = handle_thread_set_prio(get_current_thread(),
                                          msg.words[0], msg.words[1]);
    last_client = msg.words[5]; have_reply = true;
    break;
}

case TSRV_MSG_GET_INFO: {
    uint32_t state, prio, pid;
    reply.label = handle_thread_info(get_current_thread(),
                                      msg.words[0], &state, &prio, &pid);
    reply.words[0] = state;
    reply.words[1] = prio;
    reply.words[2] = pid;
    last_client = msg.words[5]; have_reply = true;
    break;
}
```

## 9.8 Cenário: Restart de Driver

```c
/* ===== Dentro do MC Supervisor: handler de "driver morreu" ===== */

/*
 * Quando o kernel detecta um fault (trap class 1) numa thread de driver,
 * ele mata a thread E sinaliza o MC Supervisor via notification.
 *
 * O MC Supervisor então faz o restart completo.
 */

#define NOTIFY_DRIVER_FAULT  (1u << 7)

static void restart_driver(const char *name, void (*entry)(void *),
                           uintptr_t code_start, size_t code_size,
                           const mem_domain_t *domain,
                           uintptr_t periph_base, uint16_t irq_idx)
{
    /* 1. Destruir processo antigo (se ainda existe) */
    /* Encontrar PID pelo nome no service directory */
    for (int i = 0; i < svc_count; i++) {
        if (svc_dir[i].active) {
            bool match = true;
            for (int j = 0; name[j]; j++)
                if (svc_dir[i].name[j] != name[j]) { match = false; break; }
            if (match) {
                proc_destroy(/* pid do processo antigo */);
                svc_dir[i].active = false;
                break;
            }
        }
    }

    /* 2. Criar novo processo */
    process_t *p = proc_create(name, PRIV_DRIVER, code_start, code_size);
    if (!p) return;

    /* 3. Adicionar domain */
    size_t dsz = domain->data_end - domain->data_start;
    as_add_region(&p->addr_space, domain->data_start,
                  (dsz + 7) & ~7u, domain->perms, REGION_DATA);

    /* 4. Dar capabilities */
    proc_grant_periph(p->pid, periph_base, PERM_READ | PERM_WRITE);
    proc_grant_irq(p->pid, irq_idx);

    /* 5. Spawnar thread principal */
    int tid = handle_thread_spawn(get_current_thread(),
                                   p->pid, (uintptr_t)entry, 0,
                                   2048, 10);
    if (tid < 0) {
        proc_destroy(p->pid);
        return;
    }

    /* 6. Registrar no service directory (será atualizado quando
       o driver chamar sys_register_service) */
}

/* No server loop, quando notification de fault chega: */
if (result.notify_bits & NOTIFY_DRIVER_FAULT) {
    /* Determinar qual driver morreu
     * (kernel pode ter colocado info na mailbox) */
    restart_driver("asclin", asclin_driver_main,
                   (uintptr_t)_app_asclin_text_start,
                   _app_asclin_text_end - _app_asclin_text_start,
                   &__domain_desc_asclin,
                   0xF0000600, ASCLIN0_RX_IRQ);
}
```

## 9.9 Diagrama: Lifecycle Completo

```
BOOT:
  kernel_main()
    ├── proc_create("asclin", PRIV_DRIVER, code, ...)
    ├── proc_grant_periph(pid, 0xF0000600, RW)
    ├── proc_grant_irq(pid, ASCLIN0_RX_IRQ)
    ├── thread_create_user("asclin", entry, ..., pid, ...)
    │
    ├── proc_create("mc_sup", PRIV_DRIVER, code, ...)
    ├── thread_create_user("mc_sup", mc_supervisor_main, ...)
    │
    └── scheduler_start()

RUNTIME (App cria sub-thread):
  App A                    MC Supervisor            Kernel
  ──────                   ═══════════════          ══════
  thread_spawn(worker,...)
    │ IPC CALL ──────────► recv
                           validate quota
                           │ SYSCALL(THREAD_SPAWN)──► criar thread
                           │                         alocar stack
                           │                         setup CSA/PCXI
                           │                ◄──────── tid
                           reply(tid)
    │◄─── reply ──────────┘
  tid = novo thread


FAULT + RESTART:
  ASCLIN Driver            Kernel                  MC Supervisor
  ═════════════            ══════                  ═══════════════
  *null_ptr = 0
    │
    ▼
  [MPU TRAP CLASS 1]
                     ────► trap_class1_handler()
                           thread_exit(asclin)
                           notify_signal(MC_NOTIFY, FAULT_BIT)
                                                    │
                                                    ▼
                                              recv_with_notify()
                                              result: FAULT!
                                                    │
                                              proc_destroy(old_pid)
                                              proc_create("asclin",...)
                                              proc_grant_periph(...)
                                              proc_grant_irq(...)
                                              handle_thread_spawn(...)
                                                    │
                                              ASCLIN reiniciado!
                                              (apps nem perceberam)
```

## 9.10 Custo

```
Thread spawn via Thread Server:
  App → IPC CALL → MC Sup → SYSCALL(THREAD_SPAWN) → reply
  = 1 IPC round-trip + 1 syscall primitiva
  ≈ 0.3μs (IPC) + 0.2μs (spawn) = ~0.5μs total

Thread spawn direto (se fosse syscall de app):
  App → SYSCALL(THREAD_SPAWN)
  ≈ 0.2μs

Overhead do Path D: ~0.3μs extra por criação.
Criação de thread é RARA (boot, restart). Overhead irrelevante.
Benefício: kernel mínimo, política flexível em userspace.
```
---

# Capítulo 9: IPC (Inter-Process Communication)

## 9.1 Dois Mecanismos

```
┌────────────────────────────────┬────────────────────────────────┐
│    SYNCHRONOUS MESSAGE         │    ASYNC NOTIFICATION          │
│    (Endpoint)                  │    (Notification object)       │
├────────────────────────────────┼────────────────────────────────┤
│  send(ep, msg) → bloqueia     │  signal(ntfy, bits) → NUNCA    │
│  recv(ep) → bloqueia          │     bloqueia                   │
│  call(ep, msg) → send+recv    │  wait(ntfy, mask) → bloqueia   │
│  reply(tid, msg) → direto     │     até bits setados           │
│                                │                                │
│  Uso: request/response         │  Uso: IRQ → driver             │
│  Payload: 6 words (24 bytes)   │  Payload: 32-bit bitmap        │
│  BLOQUEANTE                    │  NON-BLOCKING pro sender       │
└────────────────────────────────┴────────────────────────────────┘
```

## 9.2 Estruturas

```c
/* ===== ipc.h ===== */

#ifndef IPC_H
#define IPC_H

#include "thread.h"

#define MAX_ENDPOINTS       32
#define MAX_NOTIFICATIONS   16

typedef struct endpoint {
    uint16_t id; thread_t *send_queue, *recv_queue;
    uint16_t owner_pid; bool active;
} endpoint_t;

typedef struct notification {
    uint16_t id; volatile uint32_t bits;
    thread_t *waiting_thread; uint16_t owner_pid; bool active;
} notification_t;

typedef struct {
    ipc_msg_t msg; uint16_t badge;
    uint32_t notify_bits; bool is_notification;
} ipc_recv_result_t;

int      ep_create(uint16_t owner_pid);
int      ipc_send(thread_t *sender, uint16_t ep_id, const ipc_msg_t *msg);
int      ipc_recv(thread_t *recv, uint16_t ep_id, ipc_msg_t *out, uint16_t *badge);
int      ipc_call(thread_t *caller, uint16_t ep_id, ipc_msg_t *inout);
int      ipc_reply(thread_t *replier, uint16_t tid, const ipc_msg_t *msg);
int      ipc_reply_recv(thread_t *srv, uint16_t reply_tid, const ipc_msg_t *rep,
                        uint16_t ep, ipc_msg_t *out, uint16_t *badge);
int      notify_create(uint16_t owner_pid);
void     notify_signal(uint16_t id, uint32_t bits);
uint32_t notify_wait(thread_t *t, uint16_t id, uint32_t mask);
uint32_t notify_poll(uint16_t id, uint32_t mask);
int      ipc_recv_with_notify(thread_t *recv, uint16_t ep, uint16_t ntfy,
                              uint32_t mask, ipc_recv_result_t *result);

#endif
```

## 9.3 Implementação Completa

```c
/* ===== ipc.c ===== */

#include "ipc.h"
#include "scheduler.h"
#include "tricore_csfr.h"

static endpoint_t     endpoints[MAX_ENDPOINTS];
static notification_t notifications[MAX_NOTIFICATIONS];

int ep_create(uint16_t pid)
{
    for (int i = 0; i < MAX_ENDPOINTS; i++)
        if (!endpoints[i].active) {
            endpoints[i] = (endpoint_t){ .id=i, .owner_pid=pid, .active=true };
            return i;
        }
    return -1;
}

int ipc_send(thread_t *s, uint16_t ep_id, const ipc_msg_t *msg)
{
    if (ep_id >= MAX_ENDPOINTS || !endpoints[ep_id].active) return -1;
    endpoint_t *ep = &endpoints[ep_id];
    s->ipc_msg = *msg; s->ipc_badge = s->pid;

    if (ep->recv_queue) {
        thread_t *r = ep->recv_queue; ep->recv_queue = r->ipc_next;
        r->ipc_msg = s->ipc_msg; r->ipc_badge = s->ipc_badge;
        r->ipc_from_tid = s->tid; r->wakeup_reason = WAKEUP_IPC;
        r->state = THREAD_READY; r->blocked_on = BLOCKED_ON_NONE;
        scheduler_enqueue(r);
        return 0;
    }

    s->state = THREAD_BLOCKED; s->blocked_on = BLOCKED_ON_SEND;
    s->blocked_ep = ep_id; s->ipc_next = NULL;
    thread_t **t = &ep->send_queue; while (*t) t = &(*t)->ipc_next; *t = s;
    scheduler_yield();
    return 0;
}

int ipc_recv(thread_t *r, uint16_t ep_id, ipc_msg_t *out, uint16_t *badge)
{
    if (ep_id >= MAX_ENDPOINTS || !endpoints[ep_id].active) return -1;
    endpoint_t *ep = &endpoints[ep_id];

    if (ep->send_queue) {
        thread_t *s = ep->send_queue; ep->send_queue = s->ipc_next;
        *out = s->ipc_msg; if (badge) *badge = s->ipc_badge;
        r->ipc_from_tid = s->tid;
        if (s->blocked_on == BLOCKED_ON_CALL) s->blocked_on = BLOCKED_ON_REPLY;
        else { s->state = THREAD_READY; s->blocked_on = BLOCKED_ON_NONE; scheduler_enqueue(s); }
        return 0;
    }

    r->state = THREAD_BLOCKED; r->blocked_on = BLOCKED_ON_RECV;
    r->blocked_ep = ep_id; r->ipc_next = NULL;
    thread_t **t = &ep->recv_queue; while (*t) t = &(*t)->ipc_next; *t = r;
    scheduler_yield();
    *out = r->ipc_msg; if (badge) *badge = r->ipc_badge;
    return 0;
}

int ipc_call(thread_t *c, uint16_t ep_id, ipc_msg_t *inout)
{
    endpoint_t *ep = &endpoints[ep_id]; if (!ep->active) return -1;
    c->ipc_msg = *inout; c->ipc_badge = c->pid;

    if (ep->recv_queue) {
        thread_t *srv = ep->recv_queue; ep->recv_queue = srv->ipc_next;
        srv->ipc_msg = c->ipc_msg; srv->ipc_badge = c->ipc_badge;
        srv->ipc_from_tid = c->tid; srv->wakeup_reason = WAKEUP_IPC;
        srv->state = THREAD_READY; srv->blocked_on = BLOCKED_ON_NONE;
        scheduler_enqueue(srv);
    } else {
        c->ipc_next = NULL;
        thread_t **t = &ep->send_queue; while (*t) t = &(*t)->ipc_next; *t = c;
    }

    c->state = THREAD_BLOCKED; c->blocked_on = BLOCKED_ON_CALL; c->blocked_ep = ep_id;
    scheduler_yield();
    *inout = c->ipc_msg;
    return 0;
}

int ipc_reply(thread_t *rp, uint16_t tid, const ipc_msg_t *msg)
{
    thread_t *c = thread_get_by_tid(tid); if (!c) return -1;
    if (c->blocked_on != BLOCKED_ON_REPLY && c->blocked_on != BLOCKED_ON_CALL) return -2;
    c->ipc_msg = *msg; c->ipc_badge = rp->pid;
    c->blocked_on = BLOCKED_ON_NONE; c->state = THREAD_READY;
    scheduler_enqueue(c);
    return 0;
}

int ipc_reply_recv(thread_t *srv, uint16_t reply_tid, const ipc_msg_t *rep,
                   uint16_t ep, ipc_msg_t *out, uint16_t *badge)
{
    if (reply_tid) ipc_reply(srv, reply_tid, rep);
    return ipc_recv(srv, ep, out, badge);
}

/* ===== Notification ===== */

int notify_create(uint16_t pid)
{
    for (int i = 0; i < MAX_NOTIFICATIONS; i++)
        if (!notifications[i].active) {
            notifications[i] = (notification_t){ .id=i, .owner_pid=pid, .active=true };
            return i;
        }
    return -1;
}

void notify_signal(uint16_t id, uint32_t bits)
{
    if (id >= MAX_NOTIFICATIONS) return;
    notification_t *n = &notifications[id]; if (!n->active) return;

    uint32_t saved = __disable_and_save();
    n->bits |= bits;

    if (n->waiting_thread && n->waiting_thread->state == THREAD_BLOCKED &&
        (n->bits & n->waiting_thread->notify_wait_mask)) {
        thread_t *w = n->waiting_thread;
        uint32_t delivered = n->bits & w->notify_wait_mask;
        w->notify_received = delivered; n->bits &= ~delivered;
        n->waiting_thread = NULL; w->blocked_on = BLOCKED_ON_NONE;
        w->wakeup_reason = WAKEUP_NOTIFY; w->state = THREAD_READY;
        scheduler_enqueue(w);
        if (w->priority > get_current_thread()->priority)
            scheduler_request_preempt();
    }
    __restore(saved);
}

uint32_t notify_wait(thread_t *t, uint16_t id, uint32_t mask)
{
    if (id >= MAX_NOTIFICATIONS) return 0;
    notification_t *n = &notifications[id];

    uint32_t saved = __disable_and_save();
    uint32_t pending = n->bits & mask;
    if (pending) { n->bits &= ~pending; __restore(saved); return pending; }

    t->state = THREAD_BLOCKED; t->blocked_on = BLOCKED_ON_NOTIFY;
    t->notify_wait_mask = mask; t->notify_received = 0;
    n->waiting_thread = t;
    __restore(saved);
    scheduler_yield();
    return t->notify_received;
}

uint32_t notify_poll(uint16_t id, uint32_t mask)
{
    notification_t *n = &notifications[id];
    uint32_t saved = __disable_and_save();
    uint32_t p = n->bits & mask; n->bits &= ~p;
    __restore(saved);
    return p;
}

/* ===== Multiplexing ===== */

int ipc_recv_with_notify(thread_t *r, uint16_t ep_id, uint16_t nid,
                         uint32_t nmask, ipc_recv_result_t *res)
{
    endpoint_t *ep = &endpoints[ep_id];
    notification_t *n = &notifications[nid];

    uint32_t saved = __disable_and_save();

    if (ep->send_queue) {
        thread_t *s = ep->send_queue; ep->send_queue = s->ipc_next;
        res->msg = s->ipc_msg; res->badge = s->ipc_badge;
        res->is_notification = false;
        if (s->blocked_on == BLOCKED_ON_CALL) s->blocked_on = BLOCKED_ON_REPLY;
        else { s->state = THREAD_READY; s->blocked_on = BLOCKED_ON_NONE; scheduler_enqueue(s); }
        __restore(saved); return 0;
    }

    uint32_t pending = n->bits & nmask;
    if (pending) {
        n->bits &= ~pending;
        res->notify_bits = pending; res->is_notification = true;
        __restore(saved); return 0;
    }

    r->state = THREAD_BLOCKED; r->blocked_on = BLOCKED_ON_RECV_OR_NOTIFY;
    r->blocked_ep = ep_id; r->blocked_notify = nid; r->notify_wait_mask = nmask;
    r->ipc_next = NULL;
    thread_t **t = &ep->recv_queue; while (*t) t = &(*t)->ipc_next; *t = r;
    n->waiting_thread = r;
    __restore(saved);
    scheduler_yield();

    if (r->wakeup_reason == WAKEUP_IPC) {
        n->waiting_thread = NULL;
        res->msg = r->ipc_msg; res->badge = r->ipc_badge; res->is_notification = false;
    } else {
        thread_t **pp = &ep->recv_queue;
        while (*pp && *pp != r) pp = &(*pp)->ipc_next;
        if (*pp) *pp = r->ipc_next;
        res->notify_bits = r->notify_received; res->is_notification = true;
    }
    return 0;
}
```

## 9.4 Custo do IPC no TriCore

```
╔═══════════════════════════════════════════════════════════════════╗
║  One-way:                                    ~50 cycles          ║
║    (CSA automático + PRS switch = muito menos que ARM)           ║
║  Round-trip (CALL + REPLY):                  ~100 cycles         ║
║  Round-trip com REPLY_RECV:                  ~80 cycles          ║
║                                                                   ║
║  @ 200 MHz (TC275):  ~0.4 μs round-trip                         ║
║  @ 300 MHz (TC297):  ~0.27 μs round-trip                        ║
║                                                                   ║
║  vs ARM Cortex-M @ 168MHz: ~1.5 μs                              ║
║  TriCore é ~4x mais rápido no IPC!                               ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

# Capítulo 10: Interrupções em Userspace

## 10.1 Sistema de Interrupções TriCore

```
Cada periférico tem SRC (Service Request Control) registers:
  SRC.SRPN  = Priority (0-255)
  SRC.TOS   = Target core (0/1/2)
  SRC.SRE   = Enable
  SRC.SRR   = Request flag (HW sets)
  SRC.CLRR  = Clear request
  SRC.SETR  = SW trigger

Interrupt Vector: BIV + (SRPN * 32 bytes)

Ao entrar na ISR:
  - HW salva Upper Context no CSA (automático!)
  - ICR.CCPN = prioridade desta IRQ
  - PSW.IO = Supervisor
  - PSW.IS = 1 (Interrupt Stack)
```

## 10.2 SRC Helpers

```c
/* ===== tricore_src.h ===== */

#ifndef TRICORE_SRC_H
#define TRICORE_SRC_H

#include <stdint.h>

typedef volatile struct { uint32_t reg; } src_t;

#define SRC_SRPN_SHIFT  0
#define SRC_SRPN_MASK   (0xFF)
#define SRC_SRE         (1 << 10)
#define SRC_TOS_SHIFT   11
#define SRC_SRR         (1 << 24)
#define SRC_CLRR        (1 << 25)
#define SRC_SETR        (1 << 26)

#define SRC_BASE        0xF0038000
#define SRC_ASCLIN0_TX  (*(src_t *)(SRC_BASE + 0x300))
#define SRC_ASCLIN0_RX  (*(src_t *)(SRC_BASE + 0x304))
#define SRC_ASCLIN0_ERR (*(src_t *)(SRC_BASE + 0x308))
#define SRC_QSPI0_TX   (*(src_t *)(SRC_BASE + 0x380))
#define SRC_QSPI0_RX   (*(src_t *)(SRC_BASE + 0x384))
#define SRC_STM0_0      (*(src_t *)(SRC_BASE + 0x490))
#define SRC_GPSR00      (*(src_t *)(SRC_BASE + 0x990))
#define SRC_GPSR10      (*(src_t *)(SRC_BASE + 0x994))
#define SRC_GPSR20      (*(src_t *)(SRC_BASE + 0x998))

static inline void src_enable(volatile src_t *s, uint8_t prio, uint8_t tos) {
    s->reg = (prio << SRC_SRPN_SHIFT) | SRC_SRE | (tos << SRC_TOS_SHIFT) | SRC_CLRR;
}
static inline void src_disable(volatile src_t *s) { s->reg &= ~SRC_SRE; }
static inline void src_clear(volatile src_t *s) { s->reg |= SRC_CLRR; }

#endif
```

## 10.3 IRQ Dispatch

```c
/* ===== irq_dispatch.c ===== */

#include "ipc.h"
#include "tricore_src.h"

typedef struct {
    volatile src_t *src_reg;
    uint16_t notify_id; uint32_t notify_bit;
    uint16_t owner_pid; bool active; uint32_t fire_count;
} irq_binding_t;

#define MAX_IRQ_BINDINGS 64
static irq_binding_t irq_bindings[MAX_IRQ_BINDINGS];

int irq_bind(uint16_t idx, uint16_t nid, uint32_t bit, uint16_t pid) {
    if (idx >= MAX_IRQ_BINDINGS) return -1;
    irq_bindings[idx] = (irq_binding_t){ .notify_id=nid, .notify_bit=bit, .owner_pid=pid, .active=true };
    return 0;
}
int irq_enable(uint16_t idx, uint16_t pid) {
    if (idx >= MAX_IRQ_BINDINGS) return -1;
    irq_binding_t *b = &irq_bindings[idx];
    if (!b->active || b->owner_pid != pid) return -2;
    if (b->src_reg) b->src_reg->reg |= SRC_SRE;
    return 0;
}
int irq_ack(uint16_t idx, uint16_t pid) { return irq_enable(idx, pid); }

void kernel_irq_dispatch(uint8_t srpn)
{
    if (srpn >= MAX_IRQ_BINDINGS) return;
    irq_binding_t *b = &irq_bindings[srpn];
    if (!b->active) return;
    b->fire_count++;
    if (b->src_reg) { b->src_reg->reg &= ~SRC_SRE; b->src_reg->reg |= SRC_CLRR; }
    notify_signal(b->notify_id, b->notify_bit);
}
```

## 10.4 Interrupt Vector Table

```asm
/* ===== int_table.S ===== */
    .section .int_table, "ax"
    .align 8
    .globl _int_table
_int_table:

.macro INT_ENTRY prio
    .align 5
    svlcx
    mov %d4, \prio
    movh.a %a14, hi:isr_common
    lea %a14, [%a14]lo:isr_common
    ji %a14
    nop; nop; nop
.endm

    INT_ENTRY 0
    INT_ENTRY 1
    INT_ENTRY 2
    INT_ENTRY 3
    INT_ENTRY 4
    INT_ENTRY 5
    INT_ENTRY 6
    INT_ENTRY 7
    INT_ENTRY 8
    INT_ENTRY 9
    INT_ENTRY 10
    /* Adicionar até prioridade máxima usada */

    .text
    .globl isr_common
isr_common:
    call kernel_irq_dispatch
    rslcx
    rfe
```

---

# Capítulo 11: Driver Completo em Userspace

## 11.1 ASCLIN Driver (UART TriCore)

```c
/* ===== drivers/asclin_driver.c ===== */

#define MODULE_NAME asclin
#include "mem_domain.h"
#include "user_syscall.h"

PRIVATE static uint8_t  rx_buf[512];
PRIVATE static uint16_t rx_head, rx_tail;
PRIVATE static uint32_t stats_rx, stats_tx, stats_err;
DEFINE_MEM_DOMAIN(asclin, PERM_READ | PERM_WRITE | PERM_USER);

typedef volatile struct {
    uint32_t CLC, IOCR, ID, _r0;
    uint32_t TXFIFOCON, RXFIFOCON, BITCON, FRAMECON;
    uint32_t DATCON, BRG, BRD, _r1[2];
    uint32_t FLAGS, FLAGSSET, FLAGSCLEAR, FLAGSENABLE;
    uint32_t TXDATA, RXDATA, CSR;
} asclin_regs_t;

#define ASCLIN0_BASE    0xF0000600
#define ASCLIN0_RX_IRQ  10
#define NOTIFY_RX       (1u << 0)
#define MSG_WRITE       1
#define MSG_READ        2
#define MSG_STATUS      3

void asclin_driver_main(void *arg)
{
    (void)arg;
    asclin_regs_t *asc = (asclin_regs_t *)sys_mmap(
        (void *)ASCLIN0_BASE, sizeof(asclin_regs_t),
        PERM_READ | PERM_WRITE, MMAP_PERIPHERAL);
    if (!asc) sys_exit(1);

    int nid = sys_notify_create();
    sys_irq_bind(ASCLIN0_RX_IRQ, nid, NOTIFY_RX);
    int ep = sys_ep_create();
    sys_register_service("asclin0", ep);

    asc->CLC = 0; asc->CSR = 0;
    asc->FRAMECON = (1 << 16);
    asc->DATCON = 7;
    asc->BITCON = (15) | (9 << 16);
    asc->BRG = 48;
    asc->TXFIFOCON = (1 << 6);
    asc->RXFIFOCON = (1 << 6) | (1 << 1);
    asc->FLAGSENABLE = (1 << 3);
    rx_head = rx_tail = 0;
    stats_rx = stats_tx = stats_err = 0;
    sys_irq_enable(ASCLIN0_RX_IRQ);

    ipc_msg_t msg, reply;
    uint16_t badge, last = 0;
    bool hr = false;

    while (1) {
        ipc_recv_result_t res;
        if (hr) { sys_reply(last, &reply); hr = false; }
        sys_recv_with_notify(ep, nid, NOTIFY_RX, &res);

        if (res.is_notification) {
            while (asc->RXFIFOCON & (0x1F << 16)) {
                rx_buf[rx_head & 511] = asc->RXDATA; rx_head++; stats_rx++;
            }
            asc->FLAGSCLEAR = (1 << 3);
            sys_irq_ack(ASCLIN0_RX_IRQ);
        } else {
            msg = res.msg;
            switch (msg.label) {
            case MSG_WRITE: {
                uint32_t len = msg.words[0]; if (len > 20) len = 20;
                uint8_t *d = (uint8_t *)&msg.words[1];
                for (uint32_t i = 0; i < len; i++) {
                    while (!(asc->FLAGS & (1<<15))); asc->TXDATA = d[i]; stats_tx++;
                }
                reply.label = 0; reply.words[0] = len;
                last = msg.words[5]; hr = true; break;
            }
            case MSG_READ: {
                uint32_t req = msg.words[0]; if (req > 20) req = 20;
                uint32_t av = (rx_head - rx_tail) & 511;
                uint32_t n = req < av ? req : av;
                reply.label = 0; reply.words[0] = n;
                uint8_t *o = (uint8_t *)&reply.words[1];
                for (uint32_t i = 0; i < n; i++) o[i] = rx_buf[rx_tail++ & 511];
                last = msg.words[5]; hr = true; break;
            }
            case MSG_STATUS:
                reply.label = 0;
                reply.words[0] = stats_rx; reply.words[1] = stats_tx;
                reply.words[2] = stats_err; reply.words[3] = (rx_head-rx_tail)&511;
                last = msg.words[5]; hr = true; break;
            default:
                reply.label = -1; last = msg.words[5]; hr = true;
            }
        }
    }
}
```

## 11.2 Client App

```c
/* ===== apps/client_app.c ===== */
#include "user_syscall.h"

void client_app_main(void *arg)
{
    (void)arg;
    int ep = sys_lookup_service("asclin0");
    if (ep < 0) sys_exit(1);

    ipc_msg_t msg = { .label = 1 };
    const char *txt = "Hello TC275!\r\n";
    msg.words[0] = 14;
    __builtin_memcpy(&msg.words[1], txt, 14);
    sys_call(ep, &msg);

    sys_exit(0);
}
```

---

# Capítulo 12: Inicialização do Sistema

## 12.1 Startup Assembly

```asm
/* ===== startup.S ===== */
    .section .bmhd, "a"
    .align 8
bmhd0:
    .word 0x00000000
    .word _start
    .word 0xB359B359
    .word 0x43211234

    .text
    .globl _start
_start:
    /* Configurar SP */
    movh.a %a10, hi:_kernel_stack_top
    lea %a10, [%a10]lo:_kernel_stack_top

    /* ISP */
    movh %d0, hi:_isr_stack_top
    addi %d0, %d0, lo:_isr_stack_top
    mtcr 0xFE28, %d0
    isync

    /* BTV */
    movh %d0, hi:_trap_table
    addi %d0, %d0, lo:_trap_table
    mtcr 0xFE24, %d0
    isync

    /* BIV */
    movh %d0, hi:_int_table
    addi %d0, %d0, lo:_int_table
    mtcr 0xFE20, %d0
    isync

    /* Small data */
    movh.a %a0, hi:_small_data_
    lea %a0, [%a0]lo:_small_data_
    movh.a %a1, hi:_small_data2_
    lea %a1, [%a1]lo:_small_data2_
    movh.a %a8, hi:_small_data3_
    lea %a8, [%a8]lo:_small_data3_
    movh.a %a9, hi:_small_data4_
    lea %a9, [%a9]lo:_small_data4_

    call kernel_main
    debug
    j .
```

## 12.2 Kernel Init

```c
/* ===== main.c ===== */

#include "tricore_csfr.h"
#include "mpu_tricore.h"
#include "phys_alloc.h"
#include "csa.h"
#include "thread.h"
#include "scheduler.h"

extern const mem_domain_t __domain_desc_asclin;
extern const mem_domain_t __domain_desc_sensor;
extern const mem_domain_t __domain_desc_shared_ipc;
extern void asclin_driver_main(void *);
extern void sensor_app_main(void *);
extern uint8_t _app_asclin_text_start[], _app_asclin_text_end[];
extern uint8_t _app_sensor_text_start[], _app_sensor_text_end[];

void kernel_main(void)
{
    csa_pool_init();
    mpu_init();
    phys_alloc_init();
    scheduler_init();

    /* ASCLIN Driver (IO=1: acesso a periféricos) */
    {
        thread_t *t = thread_create_user("asclin", asclin_driver_main, NULL,
            10, 1, PRIV_DRIVER,
            (uintptr_t)_app_asclin_text_start,
            _app_asclin_text_end - _app_asclin_text_start, 2048);
        thread_add_domain(t, &__domain_desc_asclin);
        grant_periph_capability(1, 0xF0000600, PERM_READ|PERM_WRITE);
    }

    /* Sensor App (IO=0: mínimo) */
    {
        thread_t *t = thread_create_user("sensor", sensor_app_main, NULL,
            5, 3, PRIV_USER,
            (uintptr_t)_app_sensor_text_start,
            _app_sensor_text_end - _app_sensor_text_start, 4096);
        thread_add_domain(t, &__domain_desc_sensor);
        thread_share_domain(t, &__domain_desc_shared_ipc, PERM_READ|PERM_WRITE|PERM_USER);
    }

    mpu_enable();
    __enable();
    scheduler_start();
}
```

---

# Capítulo 13: Budget de Ranges MPU

```
╔═══════════════════════════════════════════════════════════════════╗
║           TRICORE MPU BUDGET POR THREAD                          ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║  Data Protection Ranges (DPR): 18 disponíveis                    ║
║  Code Protection Ranges (CPR): 10 disponíveis                    ║
║                                                                   ║
║  Com PRS pré-alocado (fast path):                                ║
║    Trocar PSW.PRS = trocar TUDO de uma vez (~3 cycles)           ║
║    4 PRS → 4 protection domains "quentes" simultâneos            ║
║                                                                   ║
║  ASCLIN driver:                                                   ║
║    DPR 0: kernel data (priv)                                     ║
║    DPR 1: domain_asclin (RW)                                     ║
║    DPR 2: thread stack (RW)                                      ║
║    DPR 3: ASCLIN0 periph (RW)                                   ║
║    CPR 0: kernel code (priv)                                     ║
║    CPR 1: app_asclin code (RX)                                   ║
║    Sobram: 14 DPR + 8 CPR → MUITO espaço!                       ║
║                                                                   ║
║  vs ARM com 8 slots total: TriCore é MUITO mais confortável     ║
║                                                                   ║
║  DICA: Qualquer tamanho múltiplo de 8. Sem desperdício po2.     ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

# Capítulo 14: Payloads Grandes

## 14.1 Shared Memory Buffer

```c
/* Ambas as threads têm o domain shared_ipc mapeado */
DOMAIN_BSS(shared_ipc) static uint8_t shared_buf[4096];
DEFINE_MEM_DOMAIN(shared_ipc, PERM_READ | PERM_WRITE | PERM_USER);

void send_large(int ep, const uint8_t *data, size_t len) {
    __builtin_memcpy(shared_buf, data, len);
    ipc_msg_t msg = { .label = 100 };
    msg.words[0] = (uint32_t)shared_buf;
    msg.words[1] = len;
    sys_call(ep, &msg);
}
```

## 14.2 Grant (Transferência Temporária)

```c
/*
 * Kernel move região do address space do granter
 * pro target. Zero-copy. Reverte no reply.
 */
int handle_grant(thread_t *granter, uint16_t target_tid,
                 uintptr_t addr, size_t size, uint32_t perms)
{
    thread_t *target = thread_get_by_tid(target_tid);
    address_space_t *src = &granter->addr_space;
    address_space_t *dst = &target->addr_space;

    int si = -1;
    for (int i = 0; i < src->region_count; i++)
        if (src->regions[i].base == addr && src->regions[i].active) { si = i; break; }
    if (si < 0) return -1;
    if (dst->region_count >= MAX_REGIONS_PER_THREAD) return -2;

    mem_region_t granted = src->regions[si];
    granted.perms = perms | PERM_USER;
    granted.type = REGION_GRANT;

    as_remove_region(src, addr);
    as_add_region(dst, granted.base, granted.size, granted.perms, REGION_GRANT);

    if (granter == get_current_thread()) mpu_switch_to(granter);
    return 0;
}
```

---

# Capítulo 15: Syscall Handler

```c
/* ===== syscall.c ===== */

#include "thread.h"
#include "ipc.h"
#include "irq_dispatch.h"
#include "tricore_csfr.h"

void syscall_handler(void)
{
    uint32_t nr, a0, a1, a2, a3;
    __asm__ volatile("mov %0, %%d15" : "=d"(nr));
    __asm__ volatile("mov %0, %%d4" : "=d"(a0));
    __asm__ volatile("mov %0, %%d5" : "=d"(a1));
    __asm__ volatile("mov %0, %%d6" : "=d"(a2));
    __asm__ volatile("mov %0, %%d7" : "=d"(a3));

    thread_t *c = get_current_thread();
    uint32_t result = 0;

    switch (nr) {
    /* === Thread Lifecycle (IO >= 1 apenas) === */
    case SYS_THREAD_SPAWN:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = handle_thread_spawn(c, a0, a1, a2, a3,
                                        /* priority no stack ou reg extra */
                                        a3 >> 16);  /* hack: prio no high16 de a3 */
        break;
    case SYS_THREAD_KILL:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = handle_thread_kill(c, a0);
        break;
    case SYS_THREAD_SUSPEND:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = handle_thread_suspend(c, a0);
        break;
    case SYS_THREAD_RESUME:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = handle_thread_resume(c, a0);
        break;
    case SYS_THREAD_SET_PRIO:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = handle_thread_set_prio(c, a0, a1);
        break;
    case SYS_THREAD_INFO: {
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        uint32_t st, pr, pid;
        result = handle_thread_info(c, a0, &st, &pr, &pid);
        /* Retornar via TCB (caller lê depois) */
        c->ipc_msg.words[0] = st;
        c->ipc_msg.words[1] = pr;
        c->ipc_msg.words[2] = pid;
        break;
    }
    case SYS_THREAD_GET_TID:
        result = handle_thread_get_tid(c);
        break;

    /* === Process Lifecycle (IO >= 1) === */
    case SYS_PROC_CREATE:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        /* a0=name_lo, a1=name_hi|priv, a2=code_start, a3=code_size */
        {
            char nm[16] = {0};
            *(uint32_t *)nm = a0;
            privilege_t priv = (a1 >> 24) == 1 ? PRIV_DRIVER : PRIV_USER;
            process_t *p = proc_create(nm, priv, a2, a3);
            result = p ? p->pid : (uint32_t)-6;
        }
        break;
    case SYS_PROC_DESTROY:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = proc_destroy(a0);
        break;
    case SYS_PROC_ADD_REGION:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = proc_add_region(a0, a1, a2, a3, REGION_DATA);
        break;
    case SYS_PROC_GRANT_CAP:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = proc_grant_periph(a0, a1, a2);
        break;
    case SYS_PROC_GRANT_IRQ:
        if (c->privilege < PRIV_DRIVER) { result = (uint32_t)-1; break; }
        result = proc_grant_irq(a0, a1);
        break;

    case SYS_MMAP: result = (uint32_t)handle_mmap(c,(void*)a0,a1,a2,a3); break;
    case SYS_MUNMAP: result = handle_munmap(c,(void*)a0,a1); break;
    case SYS_EP_CREATE: result = ep_create(c->pid); break;
    case SYS_CALL: { ipc_msg_t m = c->ipc_msg; ipc_call(c,a0,&m); c->ipc_msg = m; break; }
    case SYS_REPLY: { ipc_msg_t m = c->ipc_msg; result = ipc_reply(c,a0,&m); break; }
    case SYS_REPLY_RECV: {
        ipc_msg_t rep = c->ipc_msg, recv; uint16_t badge;
        ipc_reply_recv(c, a0, &rep, a1, &recv, &badge);
        c->ipc_msg = recv; c->ipc_badge = badge; break;
    }
    case SYS_NOTIFY_CREATE: result = notify_create(c->pid); break;
    case SYS_NOTIFY_SIGNAL: notify_signal(a0,a1); break;
    case SYS_NOTIFY_WAIT: result = notify_wait(c,a0,a1); break;
    case SYS_NOTIFY_POLL: result = notify_poll(a0,a1); break;
    case SYS_RECV_WITH_NOTIFY: {
        ipc_recv_result_t r;
        ipc_recv_with_notify(c,a0,a1,a2,&r);
        c->ipc_msg = r.msg; c->ipc_badge = r.badge;
        c->notify_received = r.notify_bits;
        result = r.is_notification ? 1 : 0; break;
    }
    case SYS_IRQ_BIND: result = irq_bind(a0,a1,a2,c->pid); break;
    case SYS_IRQ_ENABLE: result = irq_enable(a0,c->pid); break;
    case SYS_IRQ_ACK: result = irq_ack(a0,c->pid); break;
    case SYS_YIELD: context_switch(); break;
    case SYS_EXIT: thread_exit(c); context_switch(); break;
    default: result = (uint32_t)-1;
    }

    __asm__ volatile("mov %%d2, %0" : : "d"(result));
    __asm__ volatile("rslcx\nrfe");
}
```

---

# Capítulo 16: Primitivas de Sincronização em Userspace

## 16.1 Filosofia

```
Kernel fornece MECANISMOS (IPC + notification).
Userspace implementa POLÍTICAS (mutex, queue, timer...).

┌───────────────────┬───────────┬─────────────────────────────────┐
│    Primitiva      │   Onde?   │   Como?                         │
├───────────────────┼───────────┼─────────────────────────────────┤
│ Mutex             │ Userspace │ Notification bit (token)        │
│ Semaphore         │ Userspace │ Notification bits               │
│ Queue / Channel   │ Userspace │ Shared mem + notification       │
│ Soft Timer        │ User svc  │ Timer server (thread dedicada)  │
│ Event Group       │ Userspace │ Notification (já É isso!)       │
│ Mailbox           │ Userspace │ = IPC endpoint (já existe!)     │
│ Barrier           │ Userspace │ Endpoint + contador             │
│ RWLock            │ Userspace │ Endpoint server com protocolo   │
│ Pipe              │ Userspace │ = Queue unidirecional           │
└───────────────────┴───────────┴─────────────────────────────────┘
```

## 16.2 Mutex

```c
/* ===== libmutex.h ===== */
typedef struct { int notify_id; uint32_t bit; } umutex_t;

static inline int umutex_init(umutex_t *m, int nid, uint32_t bit) {
    m->notify_id = nid; m->bit = bit;
    sys_notify_signal(nid, bit);
    return 0;
}
static inline void umutex_lock(umutex_t *m) { sys_notify_wait(m->notify_id, m->bit); }
static inline void umutex_unlock(umutex_t *m) { sys_notify_signal(m->notify_id, m->bit); }
```

## 16.3 Semáforo

```c
/* ===== libsemaphore.h ===== */
typedef struct { int notify_id; uint32_t all_bits; } usem_t;

static inline int usem_init(usem_t *s, int nid, uint8_t base, uint8_t count) {
    s->notify_id = nid; s->all_bits = 0;
    for (uint8_t i = 0; i < count; i++) s->all_bits |= (1u << (base + i));
    sys_notify_signal(nid, s->all_bits);
    return 0;
}
static inline void usem_wait(usem_t *s) {
    uint32_t got = sys_notify_wait(s->notify_id, s->all_bits);
    uint32_t one = got & (-got); uint32_t rest = got & ~one;
    if (rest) sys_notify_signal(s->notify_id, rest);
}
static inline void usem_post(usem_t *s) {
    uint32_t one = s->all_bits & (-s->all_bits);
    sys_notify_signal(s->notify_id, one);
}
```

## 16.4 Queue

```c
/* ===== libqueue.h ===== */

#define Q_HAS_ITEM  (1u << 0)
#define Q_HAS_SPACE (1u << 1)

typedef struct {
    volatile uint32_t head, tail;
    uint32_t item_size, capacity;
    uint8_t data[];
} queue_shared_t;

typedef struct { queue_shared_t *shm; int notify_id; } uqueue_t;

static inline int uqueue_init(uqueue_t *q, void *mem, size_t sz, uint32_t isz, int nid) {
    q->shm = (queue_shared_t *)mem; q->notify_id = nid;
    q->shm->head = q->shm->tail = 0; q->shm->item_size = isz;
    q->shm->capacity = (sz - sizeof(queue_shared_t)) / isz;
    sys_notify_signal(nid, Q_HAS_SPACE);
    return 0;
}
static inline int uqueue_send(uqueue_t *q, const void *item, bool block) {
    while (((q->shm->head + 1) % q->shm->capacity) == q->shm->tail) {
        if (!block) return -1;
        sys_notify_wait(q->notify_id, Q_HAS_SPACE);
    }
    uint32_t i = q->shm->head % q->shm->capacity;
    __builtin_memcpy(&q->shm->data[i * q->shm->item_size], item, q->shm->item_size);
    __asm__ volatile("dsync" ::: "memory");
    q->shm->head++;
    sys_notify_signal(q->notify_id, Q_HAS_ITEM);
    return 0;
}
static inline int uqueue_recv(uqueue_t *q, void *item, bool block) {
    while (q->shm->head == q->shm->tail) {
        if (!block) return -1;
        sys_notify_wait(q->notify_id, Q_HAS_ITEM);
    }
    uint32_t i = q->shm->tail % q->shm->capacity;
    __builtin_memcpy(item, &q->shm->data[i * q->shm->item_size], q->shm->item_size);
    __asm__ volatile("dsync" ::: "memory");
    q->shm->tail++;
    sys_notify_signal(q->notify_id, Q_HAS_SPACE);
    return 0;
}
```

## 16.5 Event Group

```c
/* Notification JÁ É event group */
static inline uint32_t event_wait_any(int nid, uint32_t bits) {
    return sys_notify_wait(nid, bits);
}
static inline uint32_t event_wait_all(int nid, uint32_t bits) {
    uint32_t c = 0;
    while ((c & bits) != bits) c |= sys_notify_wait(nid, bits & ~c);
    return c;
}
static inline void event_set(int nid, uint32_t bits) {
    sys_notify_signal(nid, bits);
}
```

## 16.6 Timer Server

```c
/* ===== timer_server.c ===== */

#define MODULE_NAME timer_svc
#include "mem_domain.h"
#include "user_syscall.h"

#define TIMER_ONESHOT  1
#define TIMER_PERIODIC 2
#define TIMER_CANCEL   3

typedef struct {
    uint32_t expire, period;
    uint16_t owner_notify; uint32_t owner_bit;
    bool active; uint16_t id;
} timer_entry_t;

#define MAX_TIMERS 32
PRIVATE static timer_entry_t timers[MAX_TIMERS];
PRIVATE static uint16_t next_id;
DEFINE_MEM_DOMAIN(timer_svc, PERM_READ | PERM_WRITE | PERM_USER);

static void process_expired(uint32_t now) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active || timers[i].expire > now) continue;
        sys_notify_signal(timers[i].owner_notify, timers[i].owner_bit);
        if (timers[i].period > 0) timers[i].expire = now + timers[i].period;
        else timers[i].active = false;
    }
}

void timer_server_main(void *arg) {
    (void)arg;
    int ep = sys_ep_create();
    sys_register_service("timer", ep);
    int tick_ntfy = sys_notify_create();
    /* Bind STM tick interrupt */
    next_id = 1;
    for (int i = 0; i < MAX_TIMERS; i++) timers[i].active = false;

    ipc_msg_t msg, reply; uint16_t badge, last = 0; bool hr = false;

    while (1) {
        ipc_recv_result_t res;
        if (hr) { sys_reply(last, &reply); hr = false; }
        sys_recv_with_notify(ep, tick_ntfy, 1, &res);

        if (res.is_notification) {
            process_expired(0 /* sys_get_ticks() */);
        } else {
            msg = res.msg;
            int slot = -1;
            switch (msg.label) {
            case TIMER_ONESHOT: case TIMER_PERIODIC:
                for (int i = 0; i < MAX_TIMERS; i++)
                    if (!timers[i].active) { slot = i; break; }
                if (slot < 0) { reply.label = -1; }
                else {
                    timers[slot] = (timer_entry_t){
                        .expire = /* now + */ msg.words[0],
                        .period = msg.label == TIMER_PERIODIC ? msg.words[0] : 0,
                        .owner_notify = msg.words[1], .owner_bit = msg.words[2],
                        .active = true, .id = next_id++,
                    };
                    reply.label = 0; reply.words[0] = timers[slot].id;
                }
                last = msg.words[5]; hr = true; break;
            case TIMER_CANCEL: {
                uint16_t tid = msg.words[0]; reply.label = -1;
                for (int i = 0; i < MAX_TIMERS; i++)
                    if (timers[i].active && timers[i].id == tid) {
                        timers[i].active = false; reply.label = 0; break;
                    }
                last = msg.words[5]; hr = true; break;
            }}
        }
    }
}
```

---

# Capítulo 17: Multi-Core (Componente Sobre o Microkernel)

## 17.1 Filosofia

```
O microkernel roda INDEPENDENTE em cada core.
Multi-core é uma CAMADA ACIMA.

┌──────────────────────────────────────┐
│    MC SUPERVISOR (userspace, Core 0) │
│  • Service directory                 │
│  • Inter-core IPC router             │
│  • Core health monitor               │
│  • Core lifecycle                    │
└──────────┬───────────────────────────┘
           │
     ┌─────┼─────┐
     ▼     ▼     ▼
  KERNEL  KERNEL  KERNEL
  Core 0  Core 1  Core 2
  (indep) (indep) (indep)
  DSPR0   DSPR1   DSPR2

Cada core: microkernel independente.
Coordenação: via LMU mailbox + SW interrupt.
Zero spinlocks no kernel.
```

## 17.2 Boot Sequence

```
Core 0: _start → kernel_main()
  ├── Init próprio (CSA, MPU, scheduler)
  ├── Preparar DSPR1 (CSA pool, boot sync)
  ├── Preparar DSPR2
  ├── Soltar Core 1 (DBGSR.HALT clear)
  ├── Esperar Core 1 RUNNING
  ├── Soltar Core 2
  ├── Esperar Core 2 RUNNING
  ├── Criar threads locais
  └── scheduler_start()

Core 1: core1_start
  ├── Ler config do boot_sync (LMU)
  ├── Setup SP, ISP, BTV, BIV
  ├── Init kernel local (CSA, MPU, scheduler)
  ├── Criar threads locais
  ├── Sinalizar RUNNING
  └── scheduler_start()

Core 2: core2_start (idêntico ao Core 1)
```

## 17.3 Boot Sync

```c
/* ===== boot_sync.h ===== */
#ifndef BOOT_SYNC_H
#define BOOT_SYNC_H

#include <stdint.h>

#define MAX_CORES 3
#define CORE_STATE_HALT    0
#define CORE_STATE_READY   1
#define CORE_STATE_GO      2
#define CORE_STATE_RUNNING 3
#define CORE_STATE_FAILED  0xFF

typedef struct __attribute__((aligned(64))) {
    volatile uint32_t magic;
    struct __attribute__((aligned(64))) {
        volatile uint32_t state;
        uintptr_t entry, stack_top, isr_stack;
        uintptr_t csa_start, csa_end;
        uintptr_t pool_start, pool_end;
        uintptr_t trap_table, int_table;
    } core[MAX_CORES];
} boot_sync_t;

extern boot_sync_t *boot_sync;

#endif
```

## 17.4 Inter-Core IPC

```c
/* ===== inter_core_ipc.h ===== */

#ifndef INTER_CORE_IPC_H
#define INTER_CORE_IPC_H

#include <stdint.h>

#define MBOX_SLOTS 16
#define MBOX_PAYLOAD 6

typedef struct __attribute__((aligned(64))) {
    volatile uint32_t state;
    uint32_t src_core, dst_core, src_tid, dst_ep;
    uint32_t label, payload[MBOX_PAYLOAD];
} icore_slot_t;

typedef struct __attribute__((aligned(64))) {
    icore_slot_t slots[MBOX_SLOTS];
    volatile uint32_t head, tail;
} icore_mbox_t;

static inline void icore_trigger(uint8_t dst) {
    volatile uint32_t *src_tab[] = {
        (volatile uint32_t *)0xF0038990,  /* GPSR00 */
        (volatile uint32_t *)0xF0038994,  /* GPSR10 */
        (volatile uint32_t *)0xF0038998,  /* GPSR20 */
    };
    if (dst < 3) *src_tab[dst] |= (1 << 26);
}

int icore_send(uint8_t dst, uint16_t ep, uint32_t label, const uint32_t *payload);
int icore_recv(uint8_t my_core, icore_slot_t *out);

#endif
```

## 17.5 MC Supervisor

```c
/* ===== mc_supervisor.c ===== */

#define MODULE_NAME mc_sup
#include "mem_domain.h"
#include "user_syscall.h"
#include "inter_core_ipc.h"

#define MC_REGISTER  1
#define MC_LOOKUP    2
#define MC_ROUTE     3
#define MC_HEARTBEAT 4
#define MC_STATUS    5

PRIVATE static struct { char name[16]; uint8_t core; uint16_t ep; bool active; } svc_dir[32];
PRIVATE static uint8_t svc_count;
PRIVATE static struct { uint32_t last_hb; bool alive; uint32_t restarts; } health[MAX_CORES];

DEFINE_MEM_DOMAIN(mc_sup, PERM_READ | PERM_WRITE | PERM_USER);

void mc_supervisor_main(void *arg)
{
    (void)arg;
    int ep = sys_ep_create();
    int nid = sys_notify_create();
    /* Bind GPSR0 SW interrupt */
    svc_count = 0;
    for (int i = 0; i < MAX_CORES; i++) health[i].alive = true;

    ipc_msg_t msg, reply; uint16_t badge, last = 0; bool hr = false;

    while (1) {
        ipc_recv_result_t res;
        if (hr) { sys_reply(last, &reply); hr = false; }
        sys_recv_with_notify(ep, nid, 0x3, &res);

        if (res.is_notification) {
            icore_slot_t slot;
            while (icore_recv(0, &slot) == 0) {
                ipc_msg_t fwd = { .label = slot.label };
                for (int i = 0; i < MBOX_PAYLOAD && i < IPC_MSG_REGS; i++)
                    fwd.words[i] = slot.payload[i];
                sys_call(slot.dst_ep, &fwd);
                icore_send(slot.src_core, 0, fwd.label, fwd.words);
            }
        } else {
            msg = res.msg;
            switch (msg.label) {
            case MC_REGISTER: {
                char name[16] = {0};
                __builtin_memcpy(name, &msg.words[0], 8);
                if (svc_count < 32) {
                    __builtin_strncpy(svc_dir[svc_count].name, name, 15);
                    svc_dir[svc_count].core = msg.words[2];
                    svc_dir[svc_count].ep = msg.words[3];
                    svc_dir[svc_count].active = true;
                    svc_count++;
                    reply.label = 0;
                } else reply.label = -1;
                last = msg.words[5]; hr = true; break;
            }
            case MC_LOOKUP: {
                char name[16] = {0};
                __builtin_memcpy(name, &msg.words[0], 8);
                reply.label = -1;
                for (int i = 0; i < svc_count; i++) {
                    bool match = true;
                    for (int j = 0; j < 16 && name[j]; j++)
                        if (svc_dir[i].name[j] != name[j]) { match = false; break; }
                    if (match) {
                        reply.label = 0;
                        reply.words[0] = svc_dir[i].core;
                        reply.words[1] = svc_dir[i].ep;
                        break;
                    }
                }
                last = msg.words[5]; hr = true; break;
            }
            case MC_HEARTBEAT:
                if (msg.words[0] < MAX_CORES) {
                    health[msg.words[0]].alive = true;
                    /* health[msg.words[0]].last_hb = now; */
                }
                reply.label = 0; last = msg.words[5]; hr = true; break;
            case MC_STATUS:
                reply.label = 0;
                for (int i = 0; i < MAX_CORES && i < IPC_MSG_REGS; i++)
                    reply.words[i] = health[i].alive ? CORE_STATE_RUNNING : CORE_STATE_FAILED;
                last = msg.words[5]; hr = true; break;
            }
        }
    }
}
```

## 17.6 Client Library Transparente

```c
/* ===== libintercore.h ===== */

#ifndef LIBINTERCORE_H
#define LIBINTERCORE_H

#include "user_syscall.h"

typedef struct { uint8_t core; uint16_t ep; bool local; } service_handle_t;

static int mc_ep = -1;

static inline int icore_lookup(const char *name, service_handle_t *h) {
    if (mc_ep < 0) mc_ep = sys_lookup_service("mc_supervisor");
    ipc_msg_t msg = { .label = 2 }; /* MC_LOOKUP */
    __builtin_memcpy(&msg.words[0], name, 8);
    sys_call(mc_ep, &msg);
    if (msg.label != 0) return -1;
    h->core = msg.words[0]; h->ep = msg.words[1];
    h->local = (h->core == __get_core_id());
    return 0;
}

static inline int icore_call(service_handle_t *h, ipc_msg_t *msg) {
    if (h->local) return sys_call(h->ep, msg);
    /* Route via MC supervisor */
    ipc_msg_t r = { .label = 3 };
    r.words[0] = h->core; r.words[1] = h->ep;
    r.words[2] = msg->label; r.words[3] = msg->words[0];
    r.words[4] = msg->words[1]; r.words[5] = msg->words[2];
    int ret = sys_call(mc_ep, &r);
    *msg = r;
    return ret;
}

#endif
```

---

# Capítulo 18: QEMU TriCore — Setup e Debug

## 18.1 Makefile

```makefile
CROSS    = tricore-elf-
CC       = $(CROSS)gcc
AS       = $(CROSS)gcc
LD       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
SIZE     = $(CROSS)size

ARCHFLAGS = -mcpu=tc27xx -mtc161
CFLAGS   = $(ARCHFLAGS) -O2 -g -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -fno-common -ffreestanding -nostdlib
ASFLAGS  = $(ARCHFLAGS) -g
LDFLAGS  = $(ARCHFLAGS) -T tc275_microkernel.ld \
           -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=build/kernel.map

KERNEL_SRC = $(wildcard src/kernel/*.c)
KERNEL_ASM = $(wildcard src/kernel/*.S)
APP_SRC    = $(wildcard src/apps/*/*.c)
SVC_SRC    = $(wildcard src/services/*/*.c)

OBJS = $(KERNEL_SRC:%.c=build/%.o) $(KERNEL_ASM:%.S=build/%.o) \
       $(APP_SRC:%.c=build/%.o) $(SVC_SRC:%.c=build/%.o)

all: build/kernel.elf

build/%.o: %.c ; @mkdir -p $(dir $@) && $(CC) $(CFLAGS) -c $< -o $@
build/%.o: %.S ; @mkdir -p $(dir $@) && $(AS) $(ASFLAGS) -c $< -o $@

build/kernel.elf: $(OBJS) ; $(LD) $(LDFLAGS) $^ -o $@ -lgcc && $(SIZE) $@

clean: ; rm -rf build/
```

## 18.2 QEMU

```bash
# Rodar
qemu-system-tricore -M tricore_testboard -kernel build/kernel.elf \
    -nographic -serial mon:stdio -d guest_errors,unimp

# Debug
qemu-system-tricore -M tricore_testboard -kernel build/kernel.elf \
    -nographic -S -s

# Em outro terminal
tricore-elf-gdb build/kernel.elf \
    -ex "target remote :1234" -ex "break kernel_main" -ex "c"
```

## 18.3 Limitações do QEMU TriCore

```
✓ CPU TC1.6 instruction set
✓ CSA / context management
✓ Traps (SYSCALL, protection faults)
✓ Basic memory map
✓ STM timer
✓ ASCLIN (basic UART)
✓ GDB debugging

✗ Multi-core (só core 0)
✗ MPU / memory protection (parcial)
✗ Todos os periféricos (DMA, QSPI)
✗ Flash programming
✗ Safety features (lockstep)

→ Desenvolver lógica no QEMU, testar MPU no hardware real
```

---

# Capítulo 19: Resumo e Referências

## 19.1 Lista de Syscalls

| # | Nome | Acesso | Descrição |
|---|---|---|---|
| **Memória** | | | |
| 1 | MMAP | Any | Alocar memória / mapear periférico |
| 2 | MUNMAP | Any | Liberar região |
| **IPC** | | | |
| 30 | EP_CREATE | Any | Criar endpoint |
| 31 | SEND | Any | Enviar mensagem (bloqueante) |
| 32 | RECV | Any | Receber mensagem (bloqueante) |
| 33 | CALL | Any | Send + Recv atômico (RPC) |
| 34 | REPLY | Any | Responder CALL |
| 35 | REPLY_RECV | Any | Reply + Recv atômico |
| **Notification** | | | |
| 40 | NOTIFY_CREATE | Any | Criar notification |
| 41 | NOTIFY_SIGNAL | Any | Sinalizar bits (non-blocking) |
| 42 | NOTIFY_WAIT | Any | Esperar bits (bloqueante) |
| 43 | NOTIFY_POLL | Any | Checar bits (non-blocking) |
| 50 | RECV_WITH_NOTIFY | Any | Recv multiplexado |
| **IRQ** | | | |
| 60 | IRQ_BIND | IO≥1 | Associar IRQ → notification |
| 61 | IRQ_ENABLE | IO≥1 | Habilitar IRQ |
| 63 | IRQ_ACK | IO≥1 | Re-habilitar IRQ pós-tratamento |
| **Thread Lifecycle** | | | |
| 70 | THREAD_SPAWN | IO≥1 | Criar thread num processo |
| 71 | THREAD_KILL | IO≥1* | Matar thread (ou EXIT p/ si) |
| 72 | THREAD_SUSPEND | IO≥1 | Pausar thread |
| 73 | THREAD_RESUME | IO≥1 | Retomar thread pausada |
| 74 | THREAD_SET_PRIO | IO≥1 | Alterar prioridade |
| 75 | THREAD_INFO | IO≥1 | Obter state/prio/pid |
| 76 | THREAD_GET_TID | Any | Obter próprio TID |
| **Process Lifecycle** | | | |
| 80 | PROC_CREATE | IO≥1 | Criar processo (address space) |
| 81 | PROC_DESTROY | IO≥1 | Destruir processo + threads |
| 82 | PROC_ADD_REGION | IO≥1 | Adicionar região ao addr space |
| 83 | PROC_GRANT_CAP | IO≥1 | Dar capability de periférico |
| 84 | PROC_GRANT_IRQ | IO≥1 | Dar capability de IRQ |
| **Control** | | | |
| 10 | YIELD | Any | Ceder CPU |
| 11 | EXIT | Any | Terminar thread chamadora |

## 19.2 TriCore vs ARM pra Microkernel

```
╔═══════════════════════════════════════════════════════════════╗
║  Context switch:  ARM ~40 inst  →  TriCore ~5 inst (CSA!)    ║
║  MPU switch:      ARM ~50 cyc   →  TriCore ~3 cyc (PRS!)    ║
║  MPU flexibility: ARM po2+align →  TriCore any×8             ║
║  Privilege:       ARM 2 levels  →  TriCore 3 levels          ║
║  Multi-core:      ARM rare      →  TriCore 2-6 standard      ║
║  IPC round-trip:  ARM ~1.5μs    →  TriCore ~0.3μs            ║
╚═══════════════════════════════════════════════════════════════╝
```

## 19.3 Decisões de Design

```
1.  CSA pra context switch (HW automático)
2.  PSW.PRS pra trocar protection domain
3.  3 privilege levels: SV / User-1 / User-0
4.  Drivers em IO=1 (periféricos sem SV)
5.  Apps em IO=0 (mínimo privilégio)
6.  Kernel data em DSPR0 (rápido, local)
7.  Shared data em LMU (visível por todos)
8.  Alinhamento 64 bytes (CSA boundary)
9.  SYSCALL → trap class 6
10. reply_recv essencial (50% menos transições)
11. recv_with_notify (multiplex IRQ+IPC)
12. Semaphore/mutex/queue/timer = userspace
13. Multi-core = MC Supervisor (userspace component)
14. Kernel-per-core (não SMP, zero spinlocks)
15. W^X enforced (heap nunca exec)
16. Capabilities pra periféricos + IRQs
```

## 19.4 Referências

| Recurso | Descrição |
|---|---|
| TriCore Architecture Manual v1.6 | ISA + system registers |
| TC27x User Manual | Periféricos e memory map |
| AURIX TC2xx Safety Manual | ISO 26262, lockstep |
| QEMU TriCore | qemu-system-tricore |
| tricore-elf-gcc | GCC port upstream |
| ERIKA Enterprise | RTOS open-source TriCore (ref) |
| seL4 | Modelo de IPC, capabilities |
| L4 family | IPC via registros, call/reply_recv |
| Zephyr RTOS | Memory domains, MPU management |
| Tock OS | Isolation + MPU, grant model |
| QNX | Microkernel produção, IRQ userspace |
| MINIX 3 | Driver restart, isolation |
| Fuchsia/Zircon | Handles, channels, API moderna |
| Infineon iLLD | Low-Level Drivers (ref de registros) |

## 19.5 Roadmap

```
Fase 1: Boot + CSA
  □ startup.S (BMHD, SP, BTV, BIV)
  □ CSA pool init
  □ Trap vector table
  □ SYSCALL básico
  → Teste QEMU: trap funciona

Fase 2: Scheduler + Context Switch
  □ TCB com PCXI
  □ Context switch via CSA
  □ STM interrupt pra preemption
  → Teste: duas threads alternando

Fase 3: MPU
  □ DPR/CPR configuração
  □ PRS setup por thread
  □ PSW.PRS switch
  □ Trap class 1 handler
  → Teste no hardware real

Fase 4: IPC
  □ Endpoints (send/recv/call/reply)
  □ Notifications (signal/wait/poll)
  □ reply_recv + recv_with_notify
  → Benchmark: round-trip IPC

Fase 5: IRQ + Driver
  □ Interrupt vector table
  □ SRC configuration
  □ IRQ binding
  □ ASCLIN driver userspace
  → Teste: UART echo

Fase 6: Multi-Core
  □ Boot sync (LMU)
  □ Secondary core startup
  □ MC Supervisor
  □ Inter-core IPC via LMU mailbox
  → Teste: cross-core communication

Fase 7: Userspace Libraries
  □ Mutex, Semaphore, Queue
  □ Timer server
  □ libintercore
  → Aplicação completa multi-core
```

---

*Este documento é autocontido. Cobre desde fundamentos até implementação
completa de um microkernel para TriCore TC2xx com proteção de memória,
IPC síncrono e assíncrono, drivers em userspace, interrupções delegadas,
primitivas de sincronização em userspace, e suporte multi-core como
componente de supervisor. Pronto para ser consumido por um agent de
implementação.*

*Tamanho estimado do kernel: ~1500-2000 linhas de C + ~150 linhas de
assembly. Bibliotecas userspace: ~500-800 linhas. MC Supervisor: ~300
linhas.*
```
