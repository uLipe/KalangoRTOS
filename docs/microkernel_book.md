# Como Fazer um Microkernel para MCU

## Documento Técnico Completo de Implementação

**Versão:** 1.0
**Escopo:** ARM Cortex-M (MPU) e RISC-V (PMP)
**Público:** Desenvolvedores embarcados migrando de RTOS monolítico para microkernel

---

## Índice

1. [Fundamentos e Motivação](#capítulo-1-fundamentos-e-motivação)
2. [Layout de Memória e Linker Script](#capítulo-2-layout-de-memória-e-linker-script)
3. [Proteção de Memória](#capítulo-3-proteção-de-memória)
4. [Alocador de Memória Física](#capítulo-4-alocador-de-memória-física)
5. [Memory Domains (Variáveis Globais Protegidas)](#capítulo-5-memory-domains)
6. [API de MMAP](#capítulo-6-api-de-mmap)
7. [Thread Control Block e Context Switch](#capítulo-7-thread-control-block-e-context-switch)
8. [IPC (Inter-Process Communication)](#capítulo-8-ipc)
9. [Interrupções em Userspace](#capítulo-9-interrupções-em-userspace)
10. [Driver Completo em Userspace](#capítulo-10-driver-completo-em-userspace)
11. [Inicialização do Sistema](#capítulo-11-inicialização-do-sistema)
12. [Budget de Slots MPU](#capítulo-12-budget-de-slots-mpu)
13. [Payloads Grandes (Shared Memory e Grants)](#capítulo-13-payloads-grandes)
14. [Syscall Handler](#capítulo-14-syscall-handler)
15. [Primitivas de Sincronização em Userspace](#capítulo-15-primitivas-de-sincronização-em-userspace)
16. [Resumo e Referências](#capítulo-16-resumo-e-referências)

---

# Capítulo 1: Fundamentos e Motivação

## 1.1 Por Que um Microkernel em MCU?

A maioria dos RTOS embarcados (FreeRTOS, Zephyr em modo flat, ThreadX) opera
como um kernel monolítico: drivers, stacks de protocolo e código de aplicação
rodam todos no mesmo nível de privilégio e compartilham o mesmo espaço de
endereçamento. Qualquer ponteiro corrompido pode derrubar o sistema inteiro.

Um microkernel inverte essa lógica:

```
MONOLÍTICO (FreeRTOS típico):
┌─────────────────────────────────────────────┐
│            TUDO EM PRIVILEGIADO              │
│                                             │
│  App + Drivers + Stacks + Kernel            │
│  Mesmo address space, mesmas permissões     │
│                                             │
│  Bug no driver WiFi = sistema inteiro morre │
└─────────────────────────────────────────────┘

MICROKERNEL:
┌──────────┐ ┌──────────┐ ┌──────────┐
│  App A   │ │ UART Drv │ │ SPI Drv  │   ← Userspace (unprivileged)
│ (isolada)│ │(isolado) │ │(isolado) │     Cada um em seu address space
└────┬─────┘ └────┬─────┘ └────┬─────┘
     │            │            │
     └──────┬─────┴─────┬──────┘
            │   IPC     │
     ┌──────▼───────────▼──────┐
     │       KERNEL            │   ← Privilegiado (mínimo)
     │  Scheduler + IPC + MPU  │     Só o essencial
     │    ~2000 linhas de C    │
     └─────────────────────────┘

  Bug no driver UART → kernel mata o driver
                     → reinicia o driver
                     → App A nem percebeu
```

### 1.1.1 O Que Ganhamos

| Propriedade | Monolítico | Microkernel |
|---|---|---|
| Isolamento de falhas | Nenhum | Por hardware (MPU) |
| Driver crashou | Sistema morreu | Kernel reinicia o driver |
| Superfície de ataque | Tudo privilegiado | Só o kernel (~2K LOC) |
| Overhead de comunicação | Zero (chamada direta) | ~1-2μs por IPC round-trip |
| Complexidade do kernel | Alta | Baixa |
| Complexidade total | Média | Alta (IPC, protocolos) |

### 1.1.2 O Que Perdemos

Não existe almoço grátis:

- **Overhead de IPC**: cada chamada a driver é uma troca de contexto
  (~150 ciclos por direção)
- **Slots MPU limitados**: 8 a 16 regiões, difícil com muitos mapeamentos
- **Sem endereçamento virtual**: endereços são físicos, sem isolamento
  "real" de address space
- **Complexidade arquitetural**: precisa definir protocolos de IPC,
  gerenciar capabilities
- **Memória desperdiçada**: alinhamento power-of-2 da MPU (ARMv7-M)

### 1.1.3 Quando Faz Sentido

Cenários ideais para microkernel em MCU:

- Sistemas com requisitos de segurança (ISO 26262, IEC 62443, DO-178C)
- Dispositivos conectados (driver de rede isolado do app)
- Sistemas que precisam de atualização parcial (trocar driver sem reiniciar)
- Plataformas multi-tenant (múltiplos "apps" de fornecedores diferentes)
- Qualquer sistema onde "o driver pode ter bugs e não pode matar o resto"

---

## 1.2 Hardware Disponível: MPU e PMP

Em MCUs não temos MMU (Memory Management Unit) como em processadores de
aplicação. O que temos são mecanismos mais simples:

### 1.2.1 ARM Cortex-M: MPU (Memory Protection Unit)

```
MPU ARMv7-M (Cortex-M3/M4/M7):
  - 8 regiões configuráveis (algumas implementações: 16)
  - Tamanho: power-of-2, mínimo 32 bytes
  - Base: alinhada ao tamanho (base & (size-1) == 0)
  - 8 sub-regions por região (desabilitar fatias de 1/8)
  - Atributos: RO/RW/XN, Privileged/User, Cacheable/Bufferable
  - PRIVDEFENA: privileged usa default memory map (sem restrição)

MPU ARMv8-M (Cortex-M23/M33/M55/M85):
  - Até 16 regiões
  - Tamanho: múltiplo de 32 bytes (NÃO precisa ser power-of-2!)
  - Base: alinhada a 32 bytes
  - Muito mais flexível que ARMv7-M
  - SAU (Security Attribution Unit) adicional pra TrustZone
```

### 1.2.2 RISC-V: PMP (Physical Memory Protection)

```
PMP (Physical Memory Protection):
  - Até 16 entradas (configurável pela implementação)
  - Modos de matching:
    - TOR (Top Of Range): região definida por [addr[i-1], addr[i])
    - NAPOT (Naturally Aligned Power-Of-Two): base+size em power-of-2
    - NA4: Naturally Aligned 4 bytes
  - Permissões: R/W/X por entrada
  - Machine mode: sem restrição (equivale ao privileged do ARM)
  - Sem PMP configurado: Machine mode pode tudo, outros modos nada
```

### 1.2.3 Implicação Fundamental: Sem Tradução de Endereços

```
COM MMU (Linux, processadores de aplicação):
  Processo A vê endereço 0x1000 → MMU traduz pra físico 0x80001000
  Processo B vê endereço 0x1000 → MMU traduz pra físico 0x80050000
  Mesmo endereço virtual, diferentes endereços físicos.
  Isolamento total.

COM MPU/PMP (nosso caso):
  Thread A acessa endereço 0x20001000 → É o endereço REAL 0x20001000
  Thread B acessa endereço 0x20001000 → É o MESMO endereço REAL
  Sem tradução. MPU só permite ou bloqueia o acesso.

  Consequência: mmap() não cria endereço virtual.
  Retorna o endereço FÍSICO da região alocada.
  Periféricos são mapeados no endereço original.
```

---

## 1.3 Arquitetura Geral do Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                   VISÃO GERAL DO SISTEMA                     │
│                                                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ App A    │  │ App B    │  │ UART Drv │  │ SPI Drv  │   │
│  │(user)    │  │(user)    │  │(user)    │  │(user)    │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │              │              │              │         │
│       ▼              ▼              ▼              ▼         │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                    SYSCALL LAYER                      │   │
│  │               (SVC no ARM, ECALL no RISC-V)          │   │
│  └──────────────────────────────────────────────────────┘   │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                KERNEL (privilegiado)                   │   │
│  │                                                        │   │
│  │  ┌───────────┐ ┌────────┐ ┌────────┐ ┌────────────┐  │   │
│  │  │ Scheduler │ │  IPC   │ │MPU Mgr │ │ Phys Alloc │  │   │
│  │  └───────────┘ └────────┘ └────────┘ └────────────┘  │   │
│  │  ┌───────────┐ ┌────────┐                             │   │
│  │  │IRQ Dispch │ │Cap Mgr │                             │   │
│  │  └───────────┘ └────────┘                             │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  MEMÓRIA FÍSICA:                                              │
│  ┌─────┬───────┬───────┬───────┬────────┬────────┬───────┐  │
│  │KERN │domain │domain │domain │domain  │ Stack  │ Free  │  │
│  │data │ uart  │ spi   │sensor │shared  │ pool   │ pool  │  │
│  └─────┴───────┴───────┴───────┴────────┴────────┴───────┘  │
│                                                               │
│  PERIFÉRICOS (MMIO):                                          │
│  ┌────────┬────────┬────────┬────────────────────────────┐   │
│  │ GPIOA  │ USART2 │  SPI1  │  ...                       │   │
│  │(mmap'd)│(mmap'd)│(mmap'd)│                            │   │
│  └────────┴────────┴────────┴────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 1.3.1 Componentes do Kernel

O kernel contém apenas:

| Componente | Responsabilidade | ~LOC |
|---|---|---|
| **Scheduler** | Escolher qual thread roda | 200-400 |
| **IPC** | Transferir mensagens entre threads | 300-500 |
| **MPU Manager** | Configurar proteção no context switch | 150-300 |
| **Physical Allocator** | Alocar regiões de SRAM alinhadas | 200-400 |
| **IRQ Dispatcher** | Receber IRQ e notificar driver user | 100-200 |
| **Capability Manager** | Controlar quem acessa o quê | 100-200 |
| **Syscall Handler** | Decodificar e despachar SVCs | 100-200 |
| **Total** | | **~1500-2000** |

Tudo mais — drivers, protocolos, lógica de aplicação — roda em userspace.

---

# Capítulo 2: Layout de Memória e Linker Script

## 2.1 Princípios de Layout

O layout de memória precisa respeitar as restrições da MPU:

```
REGRA ARMv7-M:
  - Base alinhada ao tamanho
  - Tamanho é power-of-2
  - Mínimo 32 bytes (256 bytes é mais prático)

REGRA ARMv8-M:
  - Base alinhada a 32 bytes
  - Tamanho múltiplo de 32 bytes
  - MUITO mais flexível

REGRA RISC-V PMP (NAPOT):
  - Base alinhada ao tamanho
  - Tamanho power-of-2
  - Similar ao ARMv7-M
```

Todas as seções do linker que serão protegidas pela MPU devem estar
alinhadas. Isso desperdiça memória, mas é o custo da proteção por hardware.

## 2.2 Linker Script Completo

```linker
/* microkernel.ld */

MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    SRAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
    PERIPH (rw)  : ORIGIN = 0x40000000, LENGTH = 512M
}

/*
 * Constante de alinhamento.
 * ARMv7-M: 256 (menor tamanho prático pra MPU region)
 * ARMv8-M: 32
 * RISC-V PMP NAPOT: 256
 */
MPU_ALIGN = 256;

SECTIONS
{
    /* ============================================================ */
    /*                         FLASH                                 */
    /* ============================================================ */

    /* --- Kernel code --- */
    .kernel_text : ALIGN(32) {
        _kernel_text_start = .;
        build/kernel/*.o(.text .text.*)
        build/kernel/*.o(.rodata .rodata.*)
        _kernel_text_end = .;
    } > FLASH

    /* --- User app code (cada app alinhada separadamente) --- */

    .app_uart_text : ALIGN(MPU_ALIGN) {
        _app_uart_text_start = .;
        build/apps/uart_driver/*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(MPU_ALIGN);
        _app_uart_text_end = .;
    } > FLASH

    .app_spi_text : ALIGN(MPU_ALIGN) {
        _app_spi_text_start = .;
        build/apps/spi_driver/*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(MPU_ALIGN);
        _app_spi_text_end = .;
    } > FLASH

    .app_sensor_text : ALIGN(MPU_ALIGN) {
        _app_sensor_text_start = .;
        build/apps/sensor_app/*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(MPU_ALIGN);
        _app_sensor_text_end = .;
    } > FLASH

    /* Tabela de domain descriptors (auto-discovery) */
    .domain_table : ALIGN(4) {
        _domain_table_start = .;
        KEEP(*(.domain_table))
        _domain_table_end = .;
    } > FLASH

    /* ============================================================ */
    /*                          SRAM                                 */
    /* ============================================================ */

    /* --- Kernel data (privileged only) --- */
    .kernel_data : ALIGN(32) {
        _kernel_data_start = .;
        build/kernel/*.o(.data .data.*)
        build/kernel/*.o(.bss .bss.* COMMON)
        _kernel_data_end = .;
    } > SRAM

    /* --- Kernel stack --- */
    .kernel_stack (NOLOAD) : ALIGN(8) {
        _kernel_stack_bottom = .;
        . += 4K;
        _kernel_stack_top = .;
    } > SRAM

    /* ============================================================ */
    /*                    MEMORY DOMAINS                             */
    /* ============================================================ */

    .domain_uart (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_uart_start = .;
        *(.domain_uart.data .domain_uart.data.*)
        *(.domain_uart.bss  .domain_uart.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_uart_end = .;
    } > SRAM

    .domain_spi (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_spi_start = .;
        *(.domain_spi.data .domain_spi.data.*)
        *(.domain_spi.bss  .domain_spi.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_spi_end = .;
    } > SRAM

    .domain_sensor (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_sensor_start = .;
        *(.domain_sensor.data .domain_sensor.data.*)
        *(.domain_sensor.bss  .domain_sensor.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_sensor_end = .;
    } > SRAM

    .domain_shared_ipc (NOLOAD) : ALIGN(MPU_ALIGN) {
        _domain_shared_ipc_start = .;
        *(.domain_shared_ipc.data .domain_shared_ipc.data.*)
        *(.domain_shared_ipc.bss  .domain_shared_ipc.bss.*)
        . = ALIGN(MPU_ALIGN);
        _domain_shared_ipc_end = .;
    } > SRAM

    /* ============================================================ */
    /*                   USER MEMORY POOL                            */
    /* ============================================================ */

    .user_pool (NOLOAD) : ALIGN(MPU_ALIGN) {
        _user_pool_start = .;
        . = ORIGIN(SRAM) + LENGTH(SRAM);
        _user_pool_end = .;
    } > SRAM
}
```

## 2.3 Mapa Visual da Memória

```
FLASH (0x08000000)
┌────────────────────────────────────────┐ 0x08000000
│  Vector Table                          │
│  kernel .text + .rodata                │
│  (privileged code)                     │
├── ALIGN(256) ──────────────────────────┤
│  app_uart .text + .rodata              │
│  (user executable, read-only)          │
├── ALIGN(256) ──────────────────────────┤
│  app_spi .text + .rodata               │
├── ALIGN(256) ──────────────────────────┤
│  app_sensor .text + .rodata            │
├────────────────────────────────────────┤
│  .domain_table (descriptors, RO)       │
├────────────────────────────────────────┤
│  (livre)                               │
└────────────────────────────────────────┘ 0x08080000

SRAM (0x20000000)
┌────────────────────────────────────────┐ 0x20000000
│  kernel .data + .bss                   │ ← Priv only
│  (TCBs, endpoints, scheduler state)    │
├────────────────────────────────────────┤
│  kernel stack (4KB)                    │ ← MSP
├── ALIGN(256) ──────────────────────────┤
│  domain_uart (rx_buf, tx_buf, state)   │ ← MPU: Thread UART only
├── ALIGN(256) ──────────────────────────┤
│  domain_spi (buffers, state)           │ ← MPU: Thread SPI only
├── ALIGN(256) ──────────────────────────┤
│  domain_sensor (readings, calibration) │ ← MPU: Thread Sensor only
├── ALIGN(256) ──────────────────────────┤
│  domain_shared_ipc (published data)    │ ← MPU: Múltiplas threads
├── ALIGN(256) ──────────────────────────┤
│  USER POOL                             │
│  ┌──────────────────────────────────┐  │
│  │  Thread A stack (2KB, po2)       │  │ ← phys_alloc
│  ├──────────────────────────────────┤  │
│  │  Thread B stack (4KB, po2)       │  │
│  ├──────────────────────────────────┤  │
│  │  mmap'd heap buffer (1KB)        │  │ ← sys_mmap
│  ├──────────────────────────────────┤  │
│  │  (livre)                         │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘ 0x2003FFFF

PERIFÉRICOS (0x40000000+)
┌────────────────────────────────────────┐
│  0x40004400  USART2 regs               │ ← mmap'd pelo driver UART
│  0x40013000  SPI1 regs                 │ ← mmap'd pelo driver SPI
│  0x40020000  GPIOA regs                │ ← mmap'd conforme necessário
└────────────────────────────────────────┘
```

## 2.4 Script de Geração Automática de Seções

```python
#!/usr/bin/env python3
"""
generate_domain_ld.py

Escaneia o código-fonte buscando usos de DOMAIN_DATA/DOMAIN_BSS
e gera o trecho correspondente do linker script.

Uso:
  grep -roh 'DOMAIN_[A-Z]*([a-z_]*)' src/ | \
    sort -u | python3 generate_domain_ld.py >> domains.ld
"""

import sys
import re

domains = set()
for line in sys.stdin:
    m = re.search(r'DOMAIN_(?:DATA|BSS|RODATA)\((\w+)\)', line.strip())
    if m:
        domains.add(m.group(1))

ALIGN = 256  # ARMv7-M. Usar 32 pra ARMv8-M.

for name in sorted(domains):
    print(f"""
    .domain_{name} (NOLOAD) : ALIGN({ALIGN}) {{
        _domain_{name}_start = .;
        *(.domain_{name}.data .domain_{name}.data.*)
        *(.domain_{name}.bss  .domain_{name}.bss.*)
        . = ALIGN({ALIGN});
        _domain_{name}_end = .;
    }} > SRAM""")
```

---

# Capítulo 3: Proteção de Memória

## 3.1 Modelo de Address Space

Cada thread possui um address space: um conjunto de regiões de memória que
ela pode acessar. Todas as outras regiões são inacessíveis. Qualquer acesso
fora do permitido gera um MemManage Fault (ARM) ou access fault (RISC-V),
e o kernel mata a thread.

### 3.1.1 Estruturas de Dados

```c
/* ===== address_space.h ===== */

#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Permissões (abstraídas do hardware específico) */
#define PERM_READ       (1 << 0)
#define PERM_WRITE      (1 << 1)
#define PERM_EXEC       (1 << 2)
#define PERM_USER       (1 << 3)

/* Tipos de região */
typedef enum {
    REGION_CODE,
    REGION_DATA,
    REGION_STACK,
    REGION_HEAP,
    REGION_PERIPHERAL,
    REGION_SHARED,
    REGION_GRANT,
} region_type_t;

/* Descritor de uma região */
typedef struct mem_region {
    uintptr_t       base;
    size_t          size;
    uint32_t        perms;
    region_type_t   type;
    bool            active;
    uint16_t        owner_pid;
} mem_region_t;

/*
 * Layout típico dos slots MPU (8 regiões):
 *
 *   Slot 0: Kernel .text   (priv RX)      ← fixo
 *   Slot 1: Kernel .data   (priv RW)      ← fixo
 *   Slot 2: App .text      (user RX)      ← variável por thread
 *   Slot 3: App data/domain (user RW)     ← variável por thread
 *   Slot 4: App stack      (user RW, XN)  ← variável por thread
 *   Slot 5-7: Extra (mmap/domain)         ← variável
 */
#define MPU_KERNEL_SLOTS        2
#define MAX_REGIONS_PER_THREAD  6

typedef struct address_space {
    mem_region_t    regions[MAX_REGIONS_PER_THREAD];
    uint8_t         region_count;
    uint16_t        pid;
} address_space_t;

int  as_add_region(address_space_t *as, uintptr_t base, size_t size,
                   uint32_t perms, region_type_t type);
int  as_remove_region(address_space_t *as, uintptr_t base);
bool as_contains(const address_space_t *as, uintptr_t addr, size_t size);

#endif
```

```c
/* ===== address_space.c ===== */

#include "address_space.h"
#include <string.h>

int as_add_region(address_space_t *as, uintptr_t base, size_t size,
                  uint32_t perms, region_type_t type)
{
    if (as->region_count >= MAX_REGIONS_PER_THREAD)
        return -1;

    uintptr_t end = base + size;
    for (int i = 0; i < as->region_count; i++) {
        if (!as->regions[i].active) continue;
        uintptr_t ex_start = as->regions[i].base;
        uintptr_t ex_end   = ex_start + as->regions[i].size;
        if (base < ex_end && end > ex_start) {
            if (base == ex_start && size == as->regions[i].size)
                return 0;
            return -2;
        }
    }

    int idx = as->region_count;
    as->regions[idx] = (mem_region_t){
        .base = base, .size = size, .perms = perms,
        .type = type, .active = true, .owner_pid = as->pid,
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
        uintptr_t r_start = as->regions[i].base;
        uintptr_t r_end   = r_start + as->regions[i].size;
        if (addr >= r_start && end <= r_end) return true;
    }
    return false;
}
```

## 3.2 MPU HAL (Hardware Abstraction)

### 3.2.1 Interface Genérica

```c
/* ===== mpu_hal.h ===== */

#ifndef MPU_HAL_H
#define MPU_HAL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uintptr_t   base;
    size_t      size;
    uint32_t    perms;
    bool        enable;
} mpu_region_config_t;

void     mpu_init(void);
void     mpu_set_region(uint8_t slot, const mpu_region_config_t *cfg);
void     mpu_disable_region(uint8_t slot);
void     mpu_enable(void);
void     mpu_disable(void);
uint8_t  mpu_get_num_regions(void);

#endif
```

### 3.2.2 Implementação ARMv7-M

```c
/* ===== mpu_armv7m.c ===== */

#include "mpu_hal.h"
#include "address_space.h"

#define MPU_BASE  0xE000ED90
#define MPU_TYPE  (*(volatile uint32_t *)(MPU_BASE + 0x00))
#define MPU_CTRL  (*(volatile uint32_t *)(MPU_BASE + 0x04))
#define MPU_RNR   (*(volatile uint32_t *)(MPU_BASE + 0x08))
#define MPU_RBAR  (*(volatile uint32_t *)(MPU_BASE + 0x0C))
#define MPU_RASR  (*(volatile uint32_t *)(MPU_BASE + 0x10))

static uint8_t encode_size(size_t size)
{
    uint8_t bits = 0;
    size_t s = size;
    while (s > 1) { s >>= 1; bits++; }
    return bits - 1;
}

static uint32_t encode_ap_and_xn(uint32_t perms)
{
    uint32_t ap, xn = 0;
    if (!(perms & PERM_EXEC)) xn = 1;

    if (perms & PERM_USER) {
        if (perms & PERM_WRITE)     ap = 0x3;
        else if (perms & PERM_READ) ap = 0x2;
        else                        ap = 0x0;
    } else {
        if (perms & PERM_WRITE)     ap = 0x1;
        else if (perms & PERM_READ) ap = 0x5;
        else                        ap = 0x0;
    }
    return (ap << 24) | (xn << 28);
}

static uint32_t encode_mem_attrs(uintptr_t base)
{
    if (base >= 0x40000000)
        return (0 << 19) | (1 << 18) | (0 << 17) | (1 << 16);
    else if (base >= 0x20000000)
        return (1 << 19) | (1 << 18) | (1 << 17) | (1 << 16);
    else
        return (0 << 19) | (0 << 18) | (1 << 17) | (0 << 16);
}

void mpu_init(void)
{
    MPU_CTRL = 0;
    uint8_t num = mpu_get_num_regions();
    for (uint8_t i = 0; i < num; i++) {
        MPU_RNR = i;
        MPU_RBAR = 0;
        MPU_RASR = 0;
    }
}

uint8_t mpu_get_num_regions(void) { return (MPU_TYPE >> 8) & 0xFF; }

void mpu_set_region(uint8_t slot, const mpu_region_config_t *cfg)
{
    if (!cfg->enable) { MPU_RNR = slot; MPU_RASR = 0; return; }

    uint32_t rasr = (encode_size(cfg->size) << 1)
                  | encode_ap_and_xn(cfg->perms)
                  | encode_mem_attrs(cfg->base)
                  | (1 << 0);

    MPU_RBAR = (cfg->base & 0xFFFFFFE0) | (1 << 4) | (slot & 0xF);
    MPU_RASR = rasr;
}

void mpu_disable_region(uint8_t slot) { MPU_RNR = slot; MPU_RASR = 0; }

void mpu_enable(void)
{
    MPU_CTRL = (1 << 2) | (1 << 0);  /* PRIVDEFENA + ENABLE */
    __asm volatile("dsb\nisb" ::: "memory");
}

void mpu_disable(void)
{
    MPU_CTRL = 0;
    __asm volatile("dsb\nisb" ::: "memory");
}
```

### 3.2.3 Implementação RISC-V PMP

```c
/* ===== pmp_riscv.c ===== */

#include "mpu_hal.h"

#define PMP_R     (1 << 0)
#define PMP_W     (1 << 1)
#define PMP_X     (1 << 2)
#define PMP_NAPOT (3 << 3)

#define CSR_READ(csr, val)  __asm volatile("csrr %0, " #csr : "=r"(val))
#define CSR_WRITE(csr, val) __asm volatile("csrw " #csr ", %0" :: "r"(val))

static void write_pmpcfg(int idx, uint8_t cfg)
{
    int reg = idx / 4;
    int shift = (idx % 4) * 8;
    uint32_t val;

    switch (reg) {
    case 0: CSR_READ(pmpcfg0, val); break;
    case 1: CSR_READ(pmpcfg1, val); break;
    case 2: CSR_READ(pmpcfg2, val); break;
    case 3: CSR_READ(pmpcfg3, val); break;
    default: return;
    }

    val &= ~(0xFFu << shift);
    val |= ((uint32_t)cfg << shift);

    switch (reg) {
    case 0: CSR_WRITE(pmpcfg0, val); break;
    case 1: CSR_WRITE(pmpcfg1, val); break;
    case 2: CSR_WRITE(pmpcfg2, val); break;
    case 3: CSR_WRITE(pmpcfg3, val); break;
    }
}

static void write_pmpaddr(int idx, uint32_t addr)
{
    switch (idx) {
    case 0:  CSR_WRITE(pmpaddr0,  addr); break;
    case 1:  CSR_WRITE(pmpaddr1,  addr); break;
    case 2:  CSR_WRITE(pmpaddr2,  addr); break;
    case 3:  CSR_WRITE(pmpaddr3,  addr); break;
    case 4:  CSR_WRITE(pmpaddr4,  addr); break;
    case 5:  CSR_WRITE(pmpaddr5,  addr); break;
    case 6:  CSR_WRITE(pmpaddr6,  addr); break;
    case 7:  CSR_WRITE(pmpaddr7,  addr); break;
    case 8:  CSR_WRITE(pmpaddr8,  addr); break;
    case 9:  CSR_WRITE(pmpaddr9,  addr); break;
    case 10: CSR_WRITE(pmpaddr10, addr); break;
    case 11: CSR_WRITE(pmpaddr11, addr); break;
    case 12: CSR_WRITE(pmpaddr12, addr); break;
    case 13: CSR_WRITE(pmpaddr13, addr); break;
    case 14: CSR_WRITE(pmpaddr14, addr); break;
    case 15: CSR_WRITE(pmpaddr15, addr); break;
    }
}

static uint32_t napot_encode(uintptr_t base, size_t size)
{
    return (base + (size / 2 - 1)) >> 2;
}

void mpu_init(void)
{
    for (int i = 0; i < 16; i++) {
        write_pmpaddr(i, 0);
        write_pmpcfg(i, 0);
    }
}

uint8_t mpu_get_num_regions(void) { return 16; }

void mpu_set_region(uint8_t slot, const mpu_region_config_t *cfg)
{
    if (!cfg->enable) { write_pmpcfg(slot, 0); return; }

    uint8_t pmpcfg = PMP_NAPOT;
    if (cfg->perms & PERM_READ)  pmpcfg |= PMP_R;
    if (cfg->perms & PERM_WRITE) pmpcfg |= PMP_W;
    if (cfg->perms & PERM_EXEC)  pmpcfg |= PMP_X;

    write_pmpaddr(slot, napot_encode(cfg->base, cfg->size));
    write_pmpcfg(slot, pmpcfg);
}

void mpu_disable_region(uint8_t slot) { write_pmpcfg(slot, 0); }
void mpu_enable(void)  { __asm volatile("sfence.vma" ::: "memory"); }
void mpu_disable(void) { /* Zero all entries in mpu_init */ }
```

## 3.3 MPU Context Switch

```c
/* ===== mpu_switch.c ===== */

#include "mpu_hal.h"
#include "thread.h"

extern uint8_t _kernel_text_start[], _kernel_text_end[];
extern uint8_t _kernel_data_start[], _kernel_data_end[];

static const mpu_region_config_t kernel_regions[MPU_KERNEL_SLOTS] = {
    [0] = { .base = 0x08000000, .size = 0x40000,
            .perms = PERM_READ | PERM_EXEC, .enable = true },
    [1] = { .base = 0x20000000, .size = 0x4000,
            .perms = PERM_READ | PERM_WRITE, .enable = true },
};

void mpu_switch_to(thread_t *thread)
{
    mpu_disable();

    mpu_set_region(0, &kernel_regions[0]);
    mpu_set_region(1, &kernel_regions[1]);

    if (thread->privilege == PRIV_USER) {
        address_space_t *as = &thread->addr_space;
        for (uint8_t i = 0; i < MAX_REGIONS_PER_THREAD; i++) {
            uint8_t slot = MPU_KERNEL_SLOTS + i;
            if (i < as->region_count && as->regions[i].active) {
                mpu_region_config_t cfg = {
                    .base = as->regions[i].base,
                    .size = as->regions[i].size,
                    .perms = as->regions[i].perms,
                    .enable = true,
                };
                mpu_set_region(slot, &cfg);
            } else {
                mpu_disable_region(slot);
            }
        }
    } else {
        for (uint8_t i = 0; i < MAX_REGIONS_PER_THREAD; i++)
            mpu_disable_region(MPU_KERNEL_SLOTS + i);
    }

    mpu_enable();
}
```

```
DIAGRAMA: O QUE ACONTECE NO CONTEXT SWITCH

Thread A (rodando)                      Thread B (vai rodar)
     │                                       │
     │ [SysTick / yield / block]             │
     ▼                                       │
┌─────────────────┐                          │
│  PendSV fires   │                          │
└────────┬────────┘                          │
         ▼                                   │
┌──────────────────────────────────────┐     │
│  1. Hardware salva r0-r3,r12,lr,     │     │
│     pc,xpsr no PSP de Thread A       │     │
└────────┬─────────────────────────────┘     │
         ▼                                   │
┌──────────────────────────────────────┐     │
│  2. Software salva r4-r11 no PSP     │     │
│     Salva PSP em thread_A->ctx.sp    │     │
└────────┬─────────────────────────────┘     │
         ▼                                   │
┌──────────────────────────────────────┐     │
│  3. Scheduler escolhe Thread B       │     │
└────────┬─────────────────────────────┘     │
         ▼                                   │
┌──────────────────────────────────────┐     │
│  4. *** MPU RECONFIGURE ***          │◄────┘
│                                      │
│  Slot 0: Kernel .text  (priv RX)     │ ← Fixo
│  Slot 1: Kernel .data  (priv RW)     │ ← Fixo
│  Slot 2: Thread_B .text (user RX)    │ ← addr_space
│  Slot 3: Thread_B data  (user RW)    │ ← addr_space
│  Slot 4: Thread_B stack (user RW)    │ ← addr_space
│  Slot 5: Thread_B mmap  (user RW)    │ ← se fez mmap
│  Slot 6-7: <desabilitado>            │
│                                      │
│  DSB; ISB                            │
└────────┬─────────────────────────────┘
         ▼
┌──────────────────────────────────────┐
│  5. Restaura r4-r11 do PSP de B     │
│  6. BX LR → Thread Mode, PSP        │
│     Thread B roda em unprivileged    │
└──────────────────────────────────────┘
```

## 3.4 Fault Handler

```c
/* ===== fault.c ===== */

#include "thread.h"
#include "scheduler.h"

void MemManage_Handler(void)
{
    volatile uint32_t *CFSR  = (volatile uint32_t *)0xE000ED28;
    volatile uint32_t *MMFAR = (volatile uint32_t *)0xE000ED34;

    uint32_t cfsr  = *CFSR;
    uint32_t mmfar = *MMFAR;
    bool mmarvalid = cfsr & (1 << 7);
    bool mstkerr   = cfsr & (1 << 4);
    bool munstkerr = cfsr & (1 << 3);

    thread_t *current = get_current_thread();

    if (current && current->privilege == PRIV_USER) {
        if (mstkerr || munstkerr)
            thread_kill_process(current->pid);
        else
            thread_exit(current);

        *CFSR = cfsr;
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    } else {
        while (1) { __asm volatile("bkpt #0"); }
    }
}
```

---

# Capítulo 4: Alocador de Memória Física

## 4.1 O Problema

Threads em userspace precisam de memória para stacks, buffers de heap, e
dados. O kernel precisa alocar regiões de SRAM que respeitem os requisitos
de alinhamento da MPU.

Requisitos:

- **Alinhamento**: base alinhada ao tamanho (ARMv7-M) ou a 32 bytes (ARMv8-M)
- **Tamanho power-of-2**: obrigatório em ARMv7-M e PMP NAPOT
- **Sem fragmentação excessiva**: pool de memória é pequeno
- **Rápido**: alocação acontece em syscalls e na criação de threads

A solução natural: **Buddy Allocator**.

## 4.2 Implementação

```c
/* ===== phys_alloc.h ===== */

#ifndef PHYS_ALLOC_H
#define PHYS_ALLOC_H

#include <stdint.h>
#include <stddef.h>

void      phys_alloc_init(void);
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

#define MIN_BLOCK_SHIFT  8
#define MIN_BLOCK_SIZE   (1u << MIN_BLOCK_SHIFT)   /* 256 */
#define MAX_BLOCK_SHIFT  16                         /* 64KB */
#define NUM_ORDERS       (MAX_BLOCK_SHIFT - MIN_BLOCK_SHIFT + 1)

typedef struct block_header {
    struct block_header *next;
    uint8_t             order;
    bool                free;
} block_header_t;

static block_header_t *free_lists[NUM_ORDERS];
static uintptr_t pool_base;
static uintptr_t pool_end;
static size_t    total_free;

void phys_alloc_init(void)
{
    pool_base = ((uintptr_t)_user_pool_start + MIN_BLOCK_SIZE - 1)
                & ~(MIN_BLOCK_SIZE - 1);
    pool_end  = (uintptr_t)_user_pool_end;
    total_free = 0;

    for (int i = 0; i < NUM_ORDERS; i++)
        free_lists[i] = NULL;

    uintptr_t addr = pool_base;
    while (addr + MIN_BLOCK_SIZE <= pool_end) {
        uint8_t order = 0;
        for (int o = NUM_ORDERS - 1; o >= 0; o--) {
            size_t sz = MIN_BLOCK_SIZE << o;
            if ((addr & (sz - 1)) == 0 && addr + sz <= pool_end) {
                order = o;
                break;
            }
        }

        size_t block_size = MIN_BLOCK_SIZE << order;
        block_header_t *blk = (block_header_t *)addr;
        blk->order = order;
        blk->free = true;
        blk->next = free_lists[order];
        free_lists[order] = blk;

        total_free += block_size;
        addr += block_size;
    }
}

uintptr_t phys_alloc(size_t requested_size)
{
    size_t size = MIN_BLOCK_SIZE;
    uint8_t order = 0;
    while (size < requested_size && order < NUM_ORDERS - 1) {
        size <<= 1;
        order++;
    }
    if (size < requested_size) return 0;

    uint8_t found_order = order;
    while (found_order < NUM_ORDERS && free_lists[found_order] == NULL)
        found_order++;

    if (found_order >= NUM_ORDERS) return 0;

    block_header_t *blk = free_lists[found_order];
    free_lists[found_order] = blk->next;

    while (found_order > order) {
        found_order--;
        size_t half = MIN_BLOCK_SIZE << found_order;
        block_header_t *buddy = (block_header_t *)((uintptr_t)blk + half);
        buddy->order = found_order;
        buddy->free = true;
        buddy->next = free_lists[found_order];
        free_lists[found_order] = buddy;
    }

    blk->order = order;
    blk->free = false;
    total_free -= (MIN_BLOCK_SIZE << order);
    return (uintptr_t)blk;
}

void phys_free(uintptr_t addr, size_t size)
{
    uint8_t order = 0;
    size_t s = MIN_BLOCK_SIZE;
    while (s < size) { s <<= 1; order++; }

    block_header_t *blk = (block_header_t *)addr;

    while (order < NUM_ORDERS - 1) {
        size_t block_size = MIN_BLOCK_SIZE << order;
        uintptr_t buddy_addr = addr ^ block_size;

        if (buddy_addr < pool_base || buddy_addr >= pool_end)
            break;

        block_header_t *buddy = (block_header_t *)buddy_addr;
        if (!buddy->free || buddy->order != order)
            break;

        block_header_t **pp = &free_lists[order];
        while (*pp && *pp != buddy) pp = &(*pp)->next;
        if (*pp) *pp = buddy->next;

        if (buddy_addr < addr) { addr = buddy_addr; blk = buddy; }
        order++;
    }

    blk->order = order;
    blk->free = true;
    blk->next = free_lists[order];
    free_lists[order] = blk;
    total_free += (MIN_BLOCK_SIZE << order);
}

size_t phys_free_bytes(void) { return total_free; }
```

```
BUDDY ALLOCATOR EM AÇÃO

Estado inicial (pool = 16KB):
  Order 6 (16KB): [████████████████]  ← 1 bloco livre

Alocar 2KB:
  Split 16K → 2x 8K → split 8K → 2x 4K → split 4K → 2x 2K
  Retorna 1 bloco de 2KB

  Order 3 (2KB):  [USED] [free]
  Order 4 (4KB):               [free    ]
  Order 5 (8KB):                          [free            ]

Liberar 2KB:
  Merge com buddy → 4KB
  Merge com buddy → 8KB
  Merge com buddy → 16KB
  → Voltou ao estado original
```

---

# Capítulo 5: Memory Domains

## 5.1 Conceito

Memory Domains permitem declarar variáveis globais em C normalmente,
mas com a garantia de que apenas threads autorizadas podem acessá-las.

```
PROBLEMA:
  driver_uart.c tem "static uint8_t rx_buf[256]"
  driver_spi.c  tem "static uint8_t tx_buf[128]"
  Sem proteção: qualquer thread acessa qualquer buffer.

SOLUÇÃO:
  Agrupar variáveis de cada módulo numa seção do linker.
  Cada seção = uma região MPU.
  Só threads com permissão têm a região no address space.
```

## 5.2 Macros de Declaração

```c
/* ===== mem_domain.h ===== */

#ifndef MEM_DOMAIN_H
#define MEM_DOMAIN_H

#include <stdint.h>
#include <stddef.h>
#include "address_space.h"

#define DOMAIN_DATA(name) \
    __attribute__((section(".domain_" #name ".data")))

#define DOMAIN_BSS(name) \
    __attribute__((section(".domain_" #name ".bss")))

typedef struct mem_domain {
    const char     *name;
    uintptr_t       data_start;
    uintptr_t       data_end;
    uint32_t        perms;
} mem_domain_t;

#define DEFINE_MEM_DOMAIN(dname, permissions)                            \
    extern uint8_t _domain_##dname##_start[];                           \
    extern uint8_t _domain_##dname##_end[];                             \
    const mem_domain_t __domain_desc_##dname                            \
        __attribute__((section(".domain_table"), used)) = {             \
        .name       = #dname,                                           \
        .data_start = (uintptr_t)_domain_##dname##_start,              \
        .data_end   = (uintptr_t)_domain_##dname##_end,                \
        .perms      = (permissions),                                    \
    }

#define PRIVATE         DOMAIN_BSS(MODULE_NAME)
#define PRIVATE_INIT    DOMAIN_DATA(MODULE_NAME)

int thread_add_domain(thread_t *t, const mem_domain_t *domain);
int thread_share_domain(thread_t *t, const mem_domain_t *domain,
                        uint32_t perms_override);

#endif
```

## 5.3 Uso em Drivers

```c
/* ===== drivers/uart_driver.c ===== */

#define MODULE_NAME uart
#include "mem_domain.h"

PRIVATE static uint8_t rx_buffer[512];
PRIVATE static volatile uint16_t rx_head;
PRIVATE static volatile uint16_t rx_tail;
PRIVATE static uint8_t tx_buffer[512];
PRIVATE static volatile uint16_t tx_head;
PRIVATE static volatile uint16_t tx_tail;
PRIVATE_INIT static uint32_t baud_rate = 115200;
PRIVATE static volatile uint32_t error_count;

DEFINE_MEM_DOMAIN(uart, PERM_READ | PERM_WRITE | PERM_USER);

#undef MODULE_NAME
```

## 5.4 Kernel: Associando Domains a Threads

```c
/* ===== mem_domain.c ===== */

#include "mem_domain.h"
#include "mpu_hal.h"
#include "thread.h"

static size_t round_up_po2(size_t size)
{
    if (size <= 256) return 256;
    size_t po2 = 1;
    while (po2 < size) po2 <<= 1;
    return po2;
}

int thread_add_domain(thread_t *t, const mem_domain_t *domain)
{
    uintptr_t base = domain->data_start;
    size_t raw_size = domain->data_end - domain->data_start;
    if (raw_size == 0) return -1;

    size_t mpu_size = round_up_po2(raw_size);
    if (base & (mpu_size - 1)) return -2;
    if (t->addr_space.region_count >= MAX_REGIONS_PER_THREAD) return -3;

    return as_add_region(&t->addr_space, base, mpu_size,
                         domain->perms, REGION_DATA);
}

int thread_share_domain(thread_t *t, const mem_domain_t *domain,
                        uint32_t perms_override)
{
    uint32_t effective = (domain->perms & perms_override) | PERM_USER;
    uintptr_t base = domain->data_start;
    size_t mpu_size = round_up_po2(domain->data_end - domain->data_start);

    return as_add_region(&t->addr_space, base, mpu_size,
                         effective, REGION_SHARED);
}
```

```
MEMÓRIA COM DOMAINS

SRAM (0x20000000)
├── 0x20000000 ─── kernel_data ──────────── PRIV ONLY
├── 0x20004000 ─── kernel_stack ─────────── PRIV ONLY (MSP)
├── 0x20005000 ─── domain_uart ──────────── Thread "uart": RW
│                  rx_buffer, tx_buffer      Thread "spi":  FAULT!
│                  baud_rate, counters
├── 0x20005800 ─── domain_spi ───────────── Thread "spi":    RW
│                  spi_rx_buf, spi_tx_buf    Thread "sensor": RW (shared)
│                  spi_state                  Thread "uart":   FAULT!
├── 0x20005C00 ─── domain_sensor ────────── Thread "sensor": RW
│                  readings, calibration     EXCLUSIVE
├── 0x20005D00 ─── domain_shared_ipc ───── Thread "sensor":  RW
│                  published_temperature     Thread "monitor": RO
│                                            Thread "uart":    FAULT!
├── 0x20005E00 ─── user_pool ──────────────
│                  Stacks, mmap buffers
└── 0x2003FFFF
```

---

# Capítulo 6: API de MMAP

## 6.1 Design

A API de `mmap` retorna endereços físicos. Três modos:

| Flag | Operação | Exemplo |
|---|---|---|
| `MMAP_ANONYMOUS` | Aloca região de SRAM zerada | Buffer de trabalho |
| `MMAP_PERIPHERAL` | Concede acesso a MMIO | Registros do USART |
| `MMAP_SHARED` | Compartilha região existente | Buffer de IPC |

## 6.2 Interface Userspace

```c
/* ===== user_syscall.h ===== */

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define MMAP_ANONYMOUS  (1 << 0)
#define MMAP_PERIPHERAL (1 << 1)
#define MMAP_SHARED     (1 << 2)
#define MMAP_FIXED      (1 << 3)

#define SYS_MMAP    1
#define SYS_MUNMAP  2

static inline void *sys_mmap(void *addr_hint, size_t size,
                             uint32_t perms, uint32_t flags)
{
    register uint32_t r0 __asm__("r0") = (uint32_t)addr_hint;
    register uint32_t r1 __asm__("r1") = size;
    register uint32_t r2 __asm__("r2") = perms;
    register uint32_t r3 __asm__("r3") = flags;

    __asm__ volatile(
        "svc %[nr]"
        : "+r"(r0) : [nr] "i"(SYS_MMAP), "r"(r1), "r"(r2), "r"(r3)
        : "memory"
    );
    return (void *)r0;
}

static inline int sys_munmap(void *addr, size_t size)
{
    register uint32_t r0 __asm__("r0") = (uint32_t)addr;
    register uint32_t r1 __asm__("r1") = size;

    __asm__ volatile(
        "svc %[nr]"
        : "+r"(r0) : [nr] "i"(SYS_MUNMAP), "r"(r1) : "memory"
    );
    return (int)r0;
}

#endif
```

## 6.3 Implementação no Kernel

```c
/* ===== syscall_mmap.c ===== */

#include "thread.h"
#include "phys_alloc.h"
#include "mpu_hal.h"
#include <string.h>

typedef struct {
    const char  *name;
    uintptr_t   base;
    size_t      size;
    uint32_t    allowed_perms;
} peripheral_desc_t;

static const peripheral_desc_t periph_table[] = {
    { "GPIOA",  0x40020000, 1024, PERM_READ | PERM_WRITE | PERM_USER },
    { "USART2", 0x40004400, 1024, PERM_READ | PERM_WRITE | PERM_USER },
    { "SPI1",   0x40013000, 1024, PERM_READ | PERM_WRITE | PERM_USER },
    { "I2C1",   0x40005400, 1024, PERM_READ | PERM_WRITE | PERM_USER },
    { "TIM2",   0x40000000, 1024, PERM_READ | PERM_WRITE | PERM_USER },
    { NULL, 0, 0, 0 }
};

#define MAX_CAPS_PER_PROC 8
#define MAX_PROCS         8

static struct {
    uint16_t pid;
    struct { uintptr_t periph_base; uint32_t max_perms; } caps[MAX_CAPS_PER_PROC];
    uint8_t cap_count;
} proc_caps[MAX_PROCS];

static bool check_periph_capability(uint16_t pid, uintptr_t base,
                                    uint32_t perms)
{
    for (int p = 0; p < MAX_PROCS; p++) {
        if (proc_caps[p].pid != pid) continue;
        for (int c = 0; c < proc_caps[p].cap_count; c++) {
            if (proc_caps[p].caps[c].periph_base == base)
                return (perms & proc_caps[p].caps[c].max_perms) == perms;
        }
    }
    return false;
}

void *handle_mmap(thread_t *caller, void *addr_hint, size_t size,
                  uint32_t perms, uint32_t flags)
{
    address_space_t *as = &caller->addr_space;
    perms |= PERM_USER;

    if (flags & MMAP_ANONYMOUS) {
        size_t alloc_size = 256;
        while (alloc_size < size) alloc_size <<= 1;

        if (as->region_count >= MAX_REGIONS_PER_THREAD) return NULL;

        uintptr_t phys = phys_alloc(alloc_size);
        if (phys == 0) return NULL;

        memset((void *)phys, 0, alloc_size);
        if (perms & PERM_WRITE) perms &= ~PERM_EXEC;

        int idx = as_add_region(as, phys, alloc_size, perms, REGION_HEAP);
        if (idx < 0) { phys_free(phys, alloc_size); return NULL; }

        if (caller == get_current_thread()) {
            mpu_region_config_t cfg = {
                .base = phys, .size = alloc_size,
                .perms = perms, .enable = true,
            };
            mpu_set_region(MPU_KERNEL_SLOTS + idx, &cfg);
        }
        return (void *)phys;
    }

    if (flags & MMAP_PERIPHERAL) {
        uintptr_t periph_base = (uintptr_t)addr_hint;
        const peripheral_desc_t *pd = NULL;
        for (int i = 0; periph_table[i].name; i++) {
            if (periph_table[i].base == periph_base) {
                pd = &periph_table[i]; break;
            }
        }
        if (!pd) return NULL;
        if (!check_periph_capability(caller->pid, periph_base, perms))
            return NULL;

        perms &= ~PERM_EXEC;
        size_t map_size = 32;
        while (map_size < pd->size) map_size <<= 1;

        if (as->region_count >= MAX_REGIONS_PER_THREAD) return NULL;

        int idx = as_add_region(as, periph_base, map_size,
                                perms, REGION_PERIPHERAL);
        if (idx < 0) return NULL;

        if (caller == get_current_thread()) {
            mpu_region_config_t cfg = {
                .base = periph_base, .size = map_size,
                .perms = perms, .enable = true,
            };
            mpu_set_region(MPU_KERNEL_SLOTS + idx, &cfg);
        }
        return (void *)periph_base;
    }

    return NULL;
}

int handle_munmap(thread_t *caller, void *addr, size_t size)
{
    address_space_t *as = &caller->addr_space;
    uintptr_t target = (uintptr_t)addr;

    for (int i = 0; i < as->region_count; i++) {
        if (as->regions[i].base != target || !as->regions[i].active)
            continue;
        if (as->regions[i].type == REGION_CODE ||
            as->regions[i].type == REGION_STACK)
            return -1;
        if (as->regions[i].type == REGION_HEAP)
            phys_free(as->regions[i].base, as->regions[i].size);
        if (caller == get_current_thread())
            mpu_disable_region(MPU_KERNEL_SLOTS + i);
        as_remove_region(as, target);
        return 0;
    }
    return -1;
}
```

---

# Capítulo 7: Thread Control Block e Context Switch

## 7.1 TCB Completo

```c
/* ===== thread.h ===== */

#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <stdbool.h>
#include "address_space.h"

typedef enum {
    THREAD_READY, THREAD_RUNNING, THREAD_BLOCKED, THREAD_DEAD,
} thread_state_t;

typedef enum { PRIV_KERNEL, PRIV_USER } privilege_t;

typedef enum {
    BLOCKED_ON_NONE, BLOCKED_ON_SEND, BLOCKED_ON_RECV,
    BLOCKED_ON_CALL, BLOCKED_ON_REPLY, BLOCKED_ON_NOTIFY,
    BLOCKED_ON_RECV_OR_NOTIFY,
} blocked_reason_t;

typedef enum {
    WAKEUP_IPC, WAKEUP_NOTIFY, WAKEUP_TIMEOUT,
} wakeup_reason_t;

typedef struct {
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;
    uint32_t sp;
    uint32_t exc_return;
} thread_context_t;

#define IPC_MSG_REGS 6

typedef struct {
    uint32_t label;
    uint32_t words[IPC_MSG_REGS];
} ipc_msg_t;

typedef struct thread {
    thread_context_t    ctx;

    uint16_t            tid;
    uint16_t            pid;
    char                name[16];

    thread_state_t      state;
    uint8_t             priority;
    struct thread       *next;

    privilege_t         privilege;
    address_space_t     addr_space;
    uintptr_t           stack_base;
    size_t              stack_size;

    ipc_msg_t           ipc_msg;
    uint16_t            ipc_badge;
    uint16_t            ipc_from_tid;
    struct thread       *ipc_next;

    blocked_reason_t    blocked_on;
    uint16_t            blocked_ep;
    uint16_t            blocked_notify;
    uint16_t            reply_from_tid;
    wakeup_reason_t     wakeup_reason;

    uint32_t            notify_wait_mask;
    uint32_t            notify_received;
    uint32_t            timeout_ticks;
} thread_t;

thread_t *thread_create_user(const char *name, void (*entry)(void *),
                             void *arg, uint8_t priority, uint16_t pid,
                             uintptr_t code_start, size_t code_size,
                             size_t stack_size);
void      thread_exit(thread_t *t);
void      thread_kill_process(uint16_t pid);
thread_t *get_current_thread(void);
thread_t *thread_get_by_tid(uint16_t tid);

#endif
```

## 7.2 Criação de User Thread

```c
/* ===== thread.c ===== */

#include "thread.h"
#include "phys_alloc.h"
#include "scheduler.h"
#include <string.h>

#define MAX_THREADS 16
static thread_t thread_pool[MAX_THREADS];
static uint16_t next_tid = 1;

static thread_t *thread_alloc(void)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_pool[i].state == THREAD_DEAD || thread_pool[i].tid == 0) {
            memset(&thread_pool[i], 0, sizeof(thread_t));
            return &thread_pool[i];
        }
    }
    return NULL;
}

thread_t *thread_create_user(const char *name, void (*entry)(void *),
                             void *arg, uint8_t priority, uint16_t pid,
                             uintptr_t code_start, size_t code_size,
                             size_t stack_size)
{
    thread_t *t = thread_alloc();
    if (!t) return NULL;

    strncpy(t->name, name, sizeof(t->name) - 1);
    t->tid = next_tid++;
    t->pid = pid;
    t->priority = priority;
    t->privilege = PRIV_USER;
    t->state = THREAD_READY;

    address_space_t *as = &t->addr_space;
    as->pid = pid;
    as->region_count = 0;

    /* Código (Flash, RX) */
    size_t code_po2 = 256;
    while (code_po2 < code_size) code_po2 <<= 1;
    as_add_region(as, code_start, code_po2,
                  PERM_READ | PERM_EXEC | PERM_USER, REGION_CODE);

    /* Stack (SRAM, RW, XN) */
    size_t stack_po2 = 256;
    while (stack_po2 < stack_size) stack_po2 <<= 1;
    uintptr_t stack_phys = phys_alloc(stack_po2);
    if (!stack_phys) return NULL;

    memset((void *)stack_phys, 0xCC, stack_po2);
    t->stack_base = stack_phys;
    t->stack_size = stack_po2;
    as_add_region(as, stack_phys, stack_po2,
                  PERM_READ | PERM_WRITE | PERM_USER, REGION_STACK);

    /* Exception frame inicial */
    uint32_t *sp = (uint32_t *)(stack_phys + stack_po2);
    *(--sp) = 0x01000000;
    *(--sp) = (uint32_t)entry;
    *(--sp) = 0xFFFFFFFD;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = (uint32_t)arg;
    *(--sp) = 0xFFFFFFFD;
    for (int i = 0; i < 8; i++) *(--sp) = 0;

    t->ctx.sp = (uint32_t)sp;
    t->ctx.exc_return = 0xFFFFFFFD;

    scheduler_enqueue(t);
    return t;
}
```

## 7.3 PendSV Handler

```asm
/* ===== context_switch.S ===== */

    .syntax unified
    .thumb

    .global PendSV_Handler
    .type PendSV_Handler, %function

PendSV_Handler:
    cpsid   i
    mrs     r0, psp
    isb

    tst     lr, #0x10
    it      eq
    vstmdbeq r0!, {s16-s31}

    stmdb   r0!, {r4-r11, lr}
    bl      scheduler_switch
    ldmia   r0!, {r4-r11, lr}

    tst     lr, #0x10
    it      eq
    vldmiaeq r0!, {s16-s31}

    msr     psp, r0
    isb
    cpsie   i
    bx      lr
```

```c
/* ===== scheduler.c ===== */

#include "thread.h"
#include "mpu_hal.h"

extern void mpu_switch_to(thread_t *thread);
static thread_t *current = NULL;
static thread_t *ready_queue = NULL;

uint32_t scheduler_switch(uint32_t current_sp)
{
    if (current) {
        current->ctx.sp = current_sp;
        if (current->state == THREAD_RUNNING)
            current->state = THREAD_READY;
        if (current->state == THREAD_READY)
            scheduler_enqueue(current);
    }

    current = scheduler_dequeue();
    if (!current) while (1) __asm volatile("wfi");

    current->state = THREAD_RUNNING;
    mpu_switch_to(current);
    return current->ctx.sp;
}

thread_t *get_current_thread(void) { return current; }
```

---

# Capítulo 8: IPC (Inter-Process Communication)

## 8.1 Por Que IPC é Central

Em um microkernel, IPC é o **único** mecanismo para comunicação. Se o IPC
é lento, o sistema inteiro é lento. Se o IPC é inseguro, o isolamento é
ilusão.

## 8.2 Dois Mecanismos Complementares

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

## 8.3 Estruturas

```c
/* ===== ipc.h ===== */

#ifndef IPC_H
#define IPC_H

#include "thread.h"

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

#define MAX_ENDPOINTS       32
#define MAX_NOTIFICATIONS   16

typedef struct endpoint {
    uint16_t    id;
    thread_t    *send_queue;
    thread_t    *recv_queue;
    uint16_t    owner_pid;
    bool        active;
} endpoint_t;

typedef struct notification {
    uint16_t            id;
    volatile uint32_t   bits;
    thread_t            *waiting_thread;
    uint16_t            owner_pid;
    bool                active;
} notification_t;

typedef struct {
    ipc_msg_t   msg;
    uint16_t    badge;
    uint32_t    notify_bits;
    bool        is_notification;
} ipc_recv_result_t;

int      ep_create(uint16_t owner_pid);
int      ipc_send(thread_t *sender, uint16_t ep_id, const ipc_msg_t *msg);
int      ipc_recv(thread_t *receiver, uint16_t ep_id, ipc_msg_t *out_msg,
                  uint16_t *out_badge);
int      ipc_call(thread_t *caller, uint16_t ep_id, ipc_msg_t *inout_msg);
int      ipc_reply(thread_t *replier, uint16_t target_tid,
                   const ipc_msg_t *msg);
int      ipc_reply_recv(thread_t *server, uint16_t reply_tid,
                        const ipc_msg_t *reply_msg, uint16_t recv_ep_id,
                        ipc_msg_t *out_msg, uint16_t *out_badge);

int      notify_create(uint16_t owner_pid);
void     notify_signal(uint16_t notify_id, uint32_t bits);
uint32_t notify_wait(thread_t *thread, uint16_t notify_id, uint32_t mask);
uint32_t notify_poll(uint16_t notify_id, uint32_t mask);

int      ipc_recv_with_notify(thread_t *receiver, uint16_t ep_id,
                              uint16_t notify_id, uint32_t notify_mask,
                              ipc_recv_result_t *result);

#endif
```

## 8.4 Implementação do Endpoint

```c
/* ===== ipc.c ===== */

#include "ipc.h"
#include "scheduler.h"

static endpoint_t     endpoints[MAX_ENDPOINTS];
static notification_t notifications[MAX_NOTIFICATIONS];

int ep_create(uint16_t owner_pid)
{
    for (int i = 0; i < MAX_ENDPOINTS; i++) {
        if (!endpoints[i].active) {
            endpoints[i] = (endpoint_t){
                .id = i, .send_queue = NULL, .recv_queue = NULL,
                .owner_pid = owner_pid, .active = true,
            };
            return i;
        }
    }
    return -1;
}

int ipc_send(thread_t *sender, uint16_t ep_id, const ipc_msg_t *msg)
{
    if (ep_id >= MAX_ENDPOINTS || !endpoints[ep_id].active) return -1;
    endpoint_t *ep = &endpoints[ep_id];

    sender->ipc_msg = *msg;
    sender->ipc_badge = sender->pid;

    if (ep->recv_queue) {
        thread_t *receiver = ep->recv_queue;
        ep->recv_queue = receiver->ipc_next;

        receiver->ipc_msg = sender->ipc_msg;
        receiver->ipc_badge = sender->ipc_badge;
        receiver->ipc_from_tid = sender->tid;
        receiver->wakeup_reason = WAKEUP_IPC;
        receiver->state = THREAD_READY;
        receiver->blocked_on = BLOCKED_ON_NONE;
        scheduler_enqueue(receiver);
        return 0;
    }

    sender->state = THREAD_BLOCKED;
    sender->blocked_on = BLOCKED_ON_SEND;
    sender->blocked_ep = ep_id;
    sender->ipc_next = NULL;
    thread_t **tail = &ep->send_queue;
    while (*tail) tail = &(*tail)->ipc_next;
    *tail = sender;

    scheduler_yield();
    return 0;
}

int ipc_recv(thread_t *receiver, uint16_t ep_id, ipc_msg_t *out_msg,
             uint16_t *out_badge)
{
    if (ep_id >= MAX_ENDPOINTS || !endpoints[ep_id].active) return -1;
    endpoint_t *ep = &endpoints[ep_id];

    if (ep->send_queue) {
        thread_t *sender = ep->send_queue;
        ep->send_queue = sender->ipc_next;

        *out_msg = sender->ipc_msg;
        if (out_badge) *out_badge = sender->ipc_badge;
        receiver->ipc_from_tid = sender->tid;

        if (sender->blocked_on == BLOCKED_ON_CALL)
            sender->blocked_on = BLOCKED_ON_REPLY;
        else {
            sender->state = THREAD_READY;
            sender->blocked_on = BLOCKED_ON_NONE;
            scheduler_enqueue(sender);
        }
        return 0;
    }

    receiver->state = THREAD_BLOCKED;
    receiver->blocked_on = BLOCKED_ON_RECV;
    receiver->blocked_ep = ep_id;
    receiver->ipc_next = NULL;
    thread_t **tail = &ep->recv_queue;
    while (*tail) tail = &(*tail)->ipc_next;
    *tail = receiver;

    scheduler_yield();
    *out_msg = receiver->ipc_msg;
    if (out_badge) *out_badge = receiver->ipc_badge;
    return 0;
}

int ipc_call(thread_t *caller, uint16_t ep_id, ipc_msg_t *inout_msg)
{
    endpoint_t *ep = &endpoints[ep_id];
    if (!ep->active) return -1;

    caller->ipc_msg = *inout_msg;
    caller->ipc_badge = caller->pid;

    if (ep->recv_queue) {
        thread_t *server = ep->recv_queue;
        ep->recv_queue = server->ipc_next;

        server->ipc_msg = caller->ipc_msg;
        server->ipc_badge = caller->ipc_badge;
        server->ipc_from_tid = caller->tid;
        server->wakeup_reason = WAKEUP_IPC;
        server->state = THREAD_READY;
        server->blocked_on = BLOCKED_ON_NONE;
        scheduler_enqueue(server);
    } else {
        caller->ipc_next = NULL;
        thread_t **tail = &ep->send_queue;
        while (*tail) tail = &(*tail)->ipc_next;
        *tail = caller;
    }

    caller->state = THREAD_BLOCKED;
    caller->blocked_on = BLOCKED_ON_CALL;
    caller->blocked_ep = ep_id;
    scheduler_yield();

    *inout_msg = caller->ipc_msg;
    return 0;
}

int ipc_reply(thread_t *replier, uint16_t target_tid, const ipc_msg_t *msg)
{
    thread_t *caller = thread_get_by_tid(target_tid);
    if (!caller) return -1;
    if (caller->blocked_on != BLOCKED_ON_REPLY &&
        caller->blocked_on != BLOCKED_ON_CALL) return -2;

    caller->ipc_msg = *msg;
    caller->ipc_badge = replier->pid;
    caller->blocked_on = BLOCKED_ON_NONE;
    caller->state = THREAD_READY;
    scheduler_enqueue(caller);
    return 0;
}

int ipc_reply_recv(thread_t *server, uint16_t reply_tid,
                   const ipc_msg_t *reply_msg, uint16_t recv_ep_id,
                   ipc_msg_t *out_msg, uint16_t *out_badge)
{
    if (reply_tid != 0)
        ipc_reply(server, reply_tid, reply_msg);
    return ipc_recv(server, recv_ep_id, out_msg, out_badge);
}
```

```
CALL/REPLY FLOW:

  CLIENT                    KERNEL                     SERVER
  ──────                    ──────                     ──────

  sys_call(ep, msg)
    │ SVC ──────────────►  Copy msg, Client→BLOCKED
                           Server em recv? ─── Sim ──► Acordar
                                                          │
                                                   Processar request
                                                          │
                                                   sys_reply_recv()
                           ◄──────────────────────────────┘
  Client acorda            Reply→Client, Recv→Server BLOCKED
  msg = resposta
```

## 8.5 Notification (IPC Assíncrono)

```c
/* ===== notification.c ===== */

#include "ipc.h"
#include "scheduler.h"

int notify_create(uint16_t owner_pid)
{
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!notifications[i].active) {
            notifications[i] = (notification_t){
                .id = i, .bits = 0, .waiting_thread = NULL,
                .owner_pid = owner_pid, .active = true,
            };
            return i;
        }
    }
    return -1;
}

void notify_signal(uint16_t notify_id, uint32_t bits)
{
    if (notify_id >= MAX_NOTIFICATIONS) return;
    notification_t *n = &notifications[notify_id];
    if (!n->active) return;

    uint32_t primask;
    __asm volatile("mrs %0, primask; cpsid i" : "=r"(primask));

    n->bits |= bits;

    if (n->waiting_thread &&
        n->waiting_thread->state == THREAD_BLOCKED &&
        (n->bits & n->waiting_thread->notify_wait_mask))
    {
        thread_t *waiter = n->waiting_thread;
        uint32_t delivered = n->bits & waiter->notify_wait_mask;
        waiter->notify_received = delivered;
        n->bits &= ~delivered;

        n->waiting_thread = NULL;
        waiter->blocked_on = BLOCKED_ON_NONE;
        waiter->wakeup_reason = WAKEUP_NOTIFY;
        waiter->state = THREAD_READY;
        scheduler_enqueue(waiter);

        if (waiter->priority > get_current_thread()->priority)
            scheduler_request_preempt();
    }

    __asm volatile("msr primask, %0" :: "r"(primask));
}

uint32_t notify_wait(thread_t *thread, uint16_t notify_id, uint32_t mask)
{
    if (notify_id >= MAX_NOTIFICATIONS) return 0;
    notification_t *n = &notifications[notify_id];

    uint32_t primask;
    __asm volatile("mrs %0, primask; cpsid i" : "=r"(primask));

    uint32_t pending = n->bits & mask;
    if (pending) {
        n->bits &= ~pending;
        __asm volatile("msr primask, %0" :: "r"(primask));
        return pending;
    }

    thread->state = THREAD_BLOCKED;
    thread->blocked_on = BLOCKED_ON_NOTIFY;
    thread->notify_wait_mask = mask;
    thread->notify_received = 0;
    n->waiting_thread = thread;

    __asm volatile("msr primask, %0" :: "r"(primask));
    scheduler_yield();
    return thread->notify_received;
}

uint32_t notify_poll(uint16_t notify_id, uint32_t mask)
{
    notification_t *n = &notifications[notify_id];
    uint32_t primask;
    __asm volatile("mrs %0, primask; cpsid i" : "=r"(primask));
    uint32_t pending = n->bits & mask;
    n->bits &= ~pending;
    __asm volatile("msr primask, %0" :: "r"(primask));
    return pending;
}
```

## 8.6 Multiplexing: Notification + Endpoint

```c
/* ===== ipc_multiplex.c ===== */

#include "ipc.h"
#include "scheduler.h"

int ipc_recv_with_notify(thread_t *receiver, uint16_t ep_id,
                         uint16_t notify_id, uint32_t notify_mask,
                         ipc_recv_result_t *result)
{
    endpoint_t     *ep = &endpoints[ep_id];
    notification_t *n  = &notifications[notify_id];

    uint32_t primask;
    __asm volatile("mrs %0, primask; cpsid i" : "=r"(primask));

    if (ep->send_queue) {
        thread_t *sender = ep->send_queue;
        ep->send_queue = sender->ipc_next;

        result->msg = sender->ipc_msg;
        result->badge = sender->ipc_badge;
        result->notify_bits = 0;
        result->is_notification = false;

        if (sender->blocked_on == BLOCKED_ON_CALL)
            sender->blocked_on = BLOCKED_ON_REPLY;
        else {
            sender->state = THREAD_READY;
            sender->blocked_on = BLOCKED_ON_NONE;
            scheduler_enqueue(sender);
        }

        __asm volatile("msr primask, %0" :: "r"(primask));
        return 0;
    }

    uint32_t pending = n->bits & notify_mask;
    if (pending) {
        n->bits &= ~pending;
        result->notify_bits = pending;
        result->is_notification = true;

        __asm volatile("msr primask, %0" :: "r"(primask));
        return 0;
    }

    /* Bloquear em ambos */
    receiver->state = THREAD_BLOCKED;
    receiver->blocked_on = BLOCKED_ON_RECV_OR_NOTIFY;
    receiver->blocked_ep = ep_id;
    receiver->blocked_notify = notify_id;
    receiver->notify_wait_mask = notify_mask;

    receiver->ipc_next = NULL;
    thread_t **tail = &ep->recv_queue;
    while (*tail) tail = &(*tail)->ipc_next;
    *tail = receiver;
    n->waiting_thread = receiver;

    __asm volatile("msr primask, %0" :: "r"(primask));
    scheduler_yield();

    if (receiver->wakeup_reason == WAKEUP_IPC) {
        n->waiting_thread = NULL;
        result->msg = receiver->ipc_msg;
        result->badge = receiver->ipc_badge;
        result->is_notification = false;
    } else {
        thread_t **pp = &ep->recv_queue;
        while (*pp && *pp != receiver) pp = &(*pp)->ipc_next;
        if (*pp) *pp = receiver->ipc_next;
        result->notify_bits = receiver->notify_received;
        result->is_notification = true;
    }

    return 0;
}
```

## 8.7 Custo do IPC

```
╔═══════════════════════════════════════════════════════════════════╗
║                  ANÁLISE DE CUSTO DO IPC                         ║
╠═══════════════════════════════════════════════════════════════════╣
║  One-way:                                    ~150 cycles         ║
║  Round-trip (CALL + REPLY):                  ~300 cycles         ║
║  Round-trip com REPLY_RECV:                  ~250 cycles         ║
║                                                                   ║
║  @ 168 MHz (STM32F4):  ~1.5 μs  round-trip                      ║
║  @ 480 MHz (STM32H7):  ~0.5 μs  round-trip                      ║
║                                                                   ║
║  Comparação:                                                      ║
║    Chamada de função direta:     ~5 ns                           ║
║    IPC síncrono microkernel:     ~500-1500 ns                    ║
║    Linux IPC (pipe):             ~5000-10000 ns                  ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

# Capítulo 9: Interrupções em Userspace

## 9.1 O Problema e a Solução

```
Interrupção SEMPRE cai no kernel.
Driver que sabe tratar está em userspace.

Solução: IRQ Binding + Notification

  HW IRQ → Kernel stub (mask + signal) → Driver thread acorda
```

## 9.2 Fluxo Detalhado

```
  ┌──────────────────────┐
  │ HARDWARE IRQ         │
  │ NVIC/PLIC dispara    │
  └──────────┬───────────┘
             ▼
  ┌──────────────────────────────────────────────────┐
  │ KERNEL STUB (~20 cycles)                          │
  │  a) NVIC_DisableIRQ(n)     // previne re-entrada  │
  │  b) notify_signal(ntfy, bit) // acorda driver     │
  │  c) return                  // PendSV pode seguir  │
  └──────────┬───────────────────────────────────────┘
             ▼
  ┌──────────────────────────────────────────────────┐
  │ DRIVER THREAD (userspace)                         │
  │  a) Lê registros do periférico (MMIO mmap'd)     │
  │  b) Processa dados                                │
  │  c) Limpa flags de IRQ no periférico              │
  │  d) sys_irq_ack() → kernel reabilita NVIC        │
  │  e) Volta ao loop de espera                       │
  └──────────────────────────────────────────────────┘
```

## 9.3 Implementação

```c
/* ===== irq_dispatch.h ===== */

#ifndef IRQ_DISPATCH_H
#define IRQ_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>

#define SYS_IRQ_BIND    60
#define SYS_IRQ_ENABLE  61
#define SYS_IRQ_DISABLE 62
#define SYS_IRQ_ACK     63

typedef struct {
    uint16_t    notify_id;
    uint32_t    notify_bit;
    uint16_t    owner_pid;
    bool        active;
    bool        masked;
    uint32_t    fire_count;
} irq_binding_t;

#define MAX_IRQ_BINDINGS 64

int irq_bind(uint16_t irq_num, uint16_t notify_id,
             uint32_t notify_bit, uint16_t caller_pid);
int irq_enable(uint16_t irq_num, uint16_t caller_pid);
int irq_disable(uint16_t irq_num, uint16_t caller_pid);
int irq_ack(uint16_t irq_num, uint16_t caller_pid);

#endif
```

```c
/* ===== irq_dispatch.c ===== */

#include "irq_dispatch.h"
#include "ipc.h"

static irq_binding_t irq_bindings[MAX_IRQ_BINDINGS];

int irq_bind(uint16_t irq_num, uint16_t notify_id,
             uint32_t notify_bit, uint16_t caller_pid)
{
    if (irq_num >= MAX_IRQ_BINDINGS) return -1;

    irq_bindings[irq_num] = (irq_binding_t){
        .notify_id = notify_id, .notify_bit = notify_bit,
        .owner_pid = caller_pid, .active = true,
        .masked = true, .fire_count = 0,
    };
    return 0;
}

int irq_enable(uint16_t irq_num, uint16_t caller_pid)
{
    if (irq_num >= MAX_IRQ_BINDINGS) return -1;
    irq_binding_t *b = &irq_bindings[irq_num];
    if (!b->active || b->owner_pid != caller_pid) return -2;
    b->masked = false;
    NVIC_EnableIRQ(irq_num);
    return 0;
}

int irq_ack(uint16_t irq_num, uint16_t caller_pid)
{
    return irq_enable(irq_num, caller_pid);
}

int irq_disable(uint16_t irq_num, uint16_t caller_pid)
{
    if (irq_num >= MAX_IRQ_BINDINGS) return -1;
    irq_binding_t *b = &irq_bindings[irq_num];
    if (!b->active || b->owner_pid != caller_pid) return -2;
    b->masked = true;
    NVIC_DisableIRQ(irq_num);
    return 0;
}

static void kernel_irq_dispatch(uint16_t irq_num)
{
    irq_binding_t *b = &irq_bindings[irq_num];
    if (!b->active) { NVIC_DisableIRQ(irq_num); return; }

    b->fire_count++;
    NVIC_DisableIRQ(irq_num);
    b->masked = true;
    notify_signal(b->notify_id, b->notify_bit);
}

void Default_IRQ_Handler(void)
{
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    kernel_irq_dispatch((ipsr & 0xFF) - 16);
}

/* Stubs individuais via macro */
#define IRQ_STUB(n) \
    void IRQ##n##_Handler(void) { kernel_irq_dispatch(n); }

IRQ_STUB(37)  /* USART2 */
IRQ_STUB(35)  /* SPI1 */
```

## 9.4 Diagrama Temporal

```
  Thread "app"           Kernel              Thread "uart_drv"
  (prio 2)               (priv)              (prio 5)
  ══════════             ══════              ══════════════════

  [RUNNING]                                   [BLOCKED]
       │
       │         ◄── USART2 IRQ ──
       │              │
       │         NVIC_Disable(37)
       │         notify_signal()
       │         uart prio > app prio
       │         → PendSV!
       │              │
       │         PendSV: save app, MPU→uart, restore uart
       │                                      │
  [PREEMPTED]                            [RUNNING]
                                         lê uart->DR
                                         sys_irq_ack(37)
                                         recv_with_notify()
                                              │ SVC
                                         uart→BLOCKED
                                         schedule: app
                                         MPU→app
                                              │
  [RUNNING] ◄────────────────────────────────┘
```

---

# Capítulo 10: Driver Completo em Userspace

## 10.1 Driver UART

```c
/* ===== drivers/uart_driver.c ===== */

#define MODULE_NAME uart
#include "mem_domain.h"
#include "user_syscall.h"

PRIVATE static uint8_t  rx_buf[512];
PRIVATE static uint16_t rx_head, rx_tail;
PRIVATE static uint8_t  tx_buf[512];
PRIVATE static uint16_t tx_head, tx_tail;
PRIVATE static uint32_t stats_rx, stats_tx, stats_err;

DEFINE_MEM_DOMAIN(uart, PERM_READ | PERM_WRITE | PERM_USER);

typedef volatile struct {
    uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} uart_regs_t;

#define USART2_BASE  0x40004400
#define USART2_IRQn  37
#define NOTIFY_IRQ   (1u << 0)

#define UART_MSG_WRITE  1
#define UART_MSG_READ   2
#define UART_MSG_STATUS 3

void uart_driver_main(void *arg)
{
    (void)arg;

    uart_regs_t *uart = (uart_regs_t *)sys_mmap(
        (void *)USART2_BASE, sizeof(uart_regs_t),
        PERM_READ | PERM_WRITE, MMAP_PERIPHERAL);
    if (!uart) sys_exit(1);

    int notify_id = sys_notify_create();
    sys_irq_bind(USART2_IRQn, notify_id, NOTIFY_IRQ);
    int ep_id = sys_ep_create();
    sys_register_service("uart0", ep_id);

    uart->BRR = 0x0683;
    uart->CR1 = (1<<13)|(1<<3)|(1<<2)|(1<<5);
    rx_head = rx_tail = tx_head = tx_tail = 0;
    stats_rx = stats_tx = stats_err = 0;
    sys_irq_enable(USART2_IRQn);

    ipc_msg_t msg, reply;
    uint16_t badge, last_client = 0;
    bool have_reply = false;

    while (1) {
        ipc_recv_result_t result;

        if (have_reply) {
            sys_reply(last_client, &reply);
            have_reply = false;
        }

        sys_recv_with_notify(ep_id, notify_id, NOTIFY_IRQ, &result);

        if (result.is_notification) {
            while (uart->SR & (1<<5)) {
                rx_buf[rx_head & 511] = uart->DR;
                rx_head++; stats_rx++;
            }
            if (uart->SR & (1<<3)) { (void)uart->DR; stats_err++; }
            sys_irq_ack(USART2_IRQn);
        } else {
            msg = result.msg;
            switch (msg.label) {
            case UART_MSG_WRITE: {
                uint32_t len = msg.words[0];
                if (len > 20) len = 20;
                uint8_t *data = (uint8_t *)&msg.words[1];
                for (uint32_t i = 0; i < len; i++) {
                    while (!(uart->SR & (1<<7)));
                    uart->DR = data[i]; stats_tx++;
                }
                reply.label = 0; reply.words[0] = len;
                last_client = msg.words[5]; have_reply = true;
                break;
            }
            case UART_MSG_READ: {
                uint32_t req = msg.words[0];
                if (req > 20) req = 20;
                uint32_t avail = (rx_head - rx_tail) & 511;
                uint32_t n = req < avail ? req : avail;
                reply.label = 0; reply.words[0] = n;
                uint8_t *out = (uint8_t *)&reply.words[1];
                for (uint32_t i = 0; i < n; i++)
                    out[i] = rx_buf[rx_tail++ & 511];
                last_client = msg.words[5]; have_reply = true;
                break;
            }
            case UART_MSG_STATUS:
                reply.label = 0;
                reply.words[0] = stats_rx;
                reply.words[1] = stats_tx;
                reply.words[2] = stats_err;
                reply.words[3] = (rx_head - rx_tail) & 511;
                last_client = msg.words[5]; have_reply = true;
                break;
            default:
                reply.label = -1;
                last_client = msg.words[5]; have_reply = true;
            }
        }
    }
}
```

## 10.2 App Cliente

```c
/* ===== apps/client_app.c ===== */

#include "user_syscall.h"

void client_app_main(void *arg)
{
    (void)arg;
    int uart_ep = sys_lookup_service("uart0");
    if (uart_ep < 0) sys_exit(1);

    /* Escrever */
    ipc_msg_t msg = { .label = 1 }; /* UART_MSG_WRITE */
    const char *text = "Hello!\r\n";
    msg.words[0] = 8;
    __builtin_memcpy(&msg.words[1], text, 8);
    sys_call(uart_ep, &msg);

    /* Ler */
    msg.label = 2; /* UART_MSG_READ */
    msg.words[0] = 10;
    sys_call(uart_ep, &msg);
    if (msg.label == 0) {
        uint32_t got = msg.words[0];
        uint8_t *data = (uint8_t *)&msg.words[1];
        /* processar data[0..got-1] */
    }

    sys_exit(0);
}
```

---

# Capítulo 11: Inicialização do Sistema

## 11.1 Sequência de Boot

```
  Reset → Hardware init → kernel_main()
    ├── mpu_init()
    ├── phys_alloc_init()
    ├── scheduler_init()
    ├── Criar threads (drivers + apps)
    │   ├── thread_create_user(...)
    │   ├── thread_add_domain(...)
    │   └── grant_periph_capability(...)
    ├── mpu_enable()
    └── scheduler_start()  → primeira thread roda
```

## 11.2 Código

```c
/* ===== main.c ===== */

#include "kernel.h"
#include "mem_domain.h"
#include "phys_alloc.h"
#include "mpu_hal.h"
#include "thread.h"
#include "scheduler.h"

extern const mem_domain_t __domain_desc_uart;
extern const mem_domain_t __domain_desc_spi;
extern const mem_domain_t __domain_desc_sensor;
extern const mem_domain_t __domain_desc_shared_ipc;

extern void uart_driver_main(void *);
extern void spi_driver_main(void *);
extern void sensor_app_main(void *);
extern void monitor_app_main(void *);

extern uint8_t _app_uart_text_start[], _app_uart_text_end[];
extern uint8_t _app_spi_text_start[], _app_spi_text_end[];
extern uint8_t _app_sensor_text_start[], _app_sensor_text_end[];

void kernel_main(void)
{
    mpu_init();
    phys_alloc_init();
    scheduler_init();

    /* UART Driver */
    {
        thread_t *t = thread_create_user(
            "uart_drv", uart_driver_main, NULL, 5, 1,
            (uintptr_t)_app_uart_text_start,
            _app_uart_text_end - _app_uart_text_start, 2048);
        thread_add_domain(t, &__domain_desc_uart);
        grant_periph_capability(1, 0x40004400, PERM_READ|PERM_WRITE);
        grant_irq_capability(1, 37);
    }

    /* SPI Driver */
    {
        thread_t *t = thread_create_user(
            "spi_drv", spi_driver_main, NULL, 5, 2,
            (uintptr_t)_app_spi_text_start,
            _app_spi_text_end - _app_spi_text_start, 2048);
        thread_add_domain(t, &__domain_desc_spi);
        grant_periph_capability(2, 0x40013000, PERM_READ|PERM_WRITE);
        grant_irq_capability(2, 35);
    }

    /* Sensor App */
    {
        thread_t *t = thread_create_user(
            "sensor", sensor_app_main, NULL, 3, 3,
            (uintptr_t)_app_sensor_text_start,
            _app_sensor_text_end - _app_sensor_text_start, 4096);
        thread_add_domain(t, &__domain_desc_sensor);
        thread_add_domain(t, &__domain_desc_spi);
        thread_share_domain(t, &__domain_desc_shared_ipc,
                           PERM_READ|PERM_WRITE|PERM_USER);
    }

    /* Monitor App (read-only shared) */
    {
        thread_t *t = thread_create_user(
            "monitor", monitor_app_main, NULL, 1, 4,
            (uintptr_t)_app_sensor_text_start,
            _app_sensor_text_end - _app_sensor_text_start, 1024);
        thread_share_domain(t, &__domain_desc_shared_ipc,
                           PERM_READ|PERM_USER);
    }

    mpu_enable();
    scheduler_start();
}
```

---

# Capítulo 12: Budget de Slots MPU

```
╔═══════════════════════════════════════════════════════════════════╗
║              BUDGET DE SLOTS MPU POR THREAD                      ║
╠═══════════════════════════════════════════════════════════════════╣
║  Total: 8 (ARMv7-M) ou 16 (ARMv8-M/PMP)                        ║
║                                                                   ║
║  Slot 0:  Kernel .text         [fixo]                            ║
║  Slot 1:  Kernel .data         [fixo]                            ║
║  Slot 2:  App .text            [obrigatório]       1 slot        ║
║  Slot 3:  Thread stack         [obrigatório]       1 slot        ║
║  Slots 4-7: domains, mmap, shared               4 slots livres  ║
║                                                                   ║
║  UART driver:                                                     ║
║    Slot 4: domain_uart, Slot 5: USART2 periph → sobram 2        ║
║                                                                   ║
║  Sensor app:                                                      ║
║    Slot 4: domain_sensor, Slot 5: domain_spi,                    ║
║    Slot 6: shared_ipc → sobra 1                                  ║
║                                                                   ║
║  COM 16 SLOTS: 2 kernel + 14 user = muito confortável           ║
╚═══════════════════════════════════════════════════════════════════╝

Estratégias pra economizar:
  1. Combinar domains adjacentes num slot
  2. Sub-regions ARMv7-M (8 fatias por região)
  3. Design: threads fazem poucas coisas
  4. Usar IPC em vez de shared memory (0 slots)
  5. Escolher ARMv8-M (16 regiões, alinhamento 32B)
```

---

# Capítulo 13: Payloads Grandes

## 13.1 Shared Memory Buffer

```c
/* Ambas as threads têm o domain shared_ipc mapeado */

DOMAIN_BSS(shared_ipc) static uint8_t shared_buf[4096];
DEFINE_MEM_DOMAIN(shared_ipc, PERM_READ | PERM_WRITE | PERM_USER);

/* Client escreve no buffer, manda offset+size via IPC */
void send_large(int ep, const uint8_t *data, size_t len)
{
    memcpy(shared_buf, data, len);
    ipc_msg_t msg = { .label = MSG_WRITE_LARGE };
    msg.words[0] = (uint32_t)shared_buf;
    msg.words[1] = len;
    sys_call(ep, &msg);
}
```

## 13.2 Grant (Transferência Temporária)

```c
/*
 * Grant: kernel move região do address space do granter
 * pro address space do target. Zero-copy real.
 *
 * 1. Client chama sys_grant(driver_tid, addr, size, perms)
 * 2. Kernel remove do client, adiciona ao driver
 * 3. Client: MemFault se tentar acessar
 * 4. Reply reverte automaticamente
 */

int handle_grant(thread_t *granter, uint16_t target_tid,
                 uintptr_t addr, size_t size, uint32_t perms)
{
    thread_t *target = thread_get_by_tid(target_tid);
    address_space_t *src = &granter->addr_space;
    address_space_t *dst = &target->addr_space;

    int src_idx = -1;
    for (int i = 0; i < src->region_count; i++) {
        if (src->regions[i].base == addr && src->regions[i].active) {
            src_idx = i; break;
        }
    }
    if (src_idx < 0) return -1;
    if (dst->region_count >= MAX_REGIONS_PER_THREAD) return -2;

    mem_region_t granted = src->regions[src_idx];
    granted.perms = perms | PERM_USER;
    granted.type = REGION_GRANT;

    as_remove_region(src, addr);
    as_add_region(dst, granted.base, granted.size,
                  granted.perms, REGION_GRANT);

    if (granter == get_current_thread())
        mpu_switch_to(granter);

    return 0;
}
```

---

# Capítulo 14: Syscall Handler

```c
/* ===== syscall.c ===== */

#include "thread.h"
#include "ipc.h"
#include "irq_dispatch.h"

__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile(
        "tst    lr, #4      \n"
        "ite    eq           \n"
        "mrseq  r0, msp      \n"
        "mrsne  r0, psp      \n"
        "b      SVC_Handler_C\n"
    );
}

void SVC_Handler_C(uint32_t *frame)
{
    uint8_t svc_num = ((uint8_t *)frame[6])[-2];
    thread_t *caller = get_current_thread();

    switch (svc_num) {
    case SYS_MMAP:
        frame[0] = (uint32_t)handle_mmap(caller,
            (void*)frame[0], frame[1], frame[2], frame[3]);
        break;
    case SYS_MUNMAP:
        frame[0] = handle_munmap(caller, (void*)frame[0], frame[1]);
        break;
    case SYS_EP_CREATE:
        frame[0] = ep_create(caller->pid);
        break;
    case SYS_CALL: {
        ipc_msg_t msg = caller->ipc_msg;
        ipc_call(caller, frame[0], &msg);
        caller->ipc_msg = msg;
        break;
    }
    case SYS_REPLY: {
        ipc_msg_t msg = caller->ipc_msg;
        frame[0] = ipc_reply(caller, frame[0], &msg);
        break;
    }
    case SYS_REPLY_RECV: {
        ipc_msg_t reply = caller->ipc_msg;
        ipc_msg_t recv;
        uint16_t badge;
        ipc_reply_recv(caller, frame[0], &reply, frame[1],
                       &recv, &badge);
        caller->ipc_msg = recv;
        caller->ipc_badge = badge;
        break;
    }
    case SYS_NOTIFY_CREATE:
        frame[0] = notify_create(caller->pid);
        break;
    case SYS_NOTIFY_SIGNAL:
        notify_signal(frame[0], frame[1]);
        break;
    case SYS_NOTIFY_WAIT:
        frame[0] = notify_wait(caller, frame[0], frame[1]);
        break;
    case SYS_NOTIFY_POLL:
        frame[0] = notify_poll(frame[0], frame[1]);
        break;
    case SYS_RECV_WITH_NOTIFY: {
        ipc_recv_result_t result;
        ipc_recv_with_notify(caller, frame[0], frame[1],
                             frame[2], &result);
        caller->ipc_msg = result.msg;
        caller->ipc_badge = result.badge;
        caller->notify_received = result.notify_bits;
        frame[0] = result.is_notification ? 1 : 0;
        break;
    }
    case SYS_IRQ_BIND:
        frame[0] = irq_bind(frame[0], frame[1], frame[2], caller->pid);
        break;
    case SYS_IRQ_ENABLE:
        frame[0] = irq_enable(frame[0], caller->pid);
        break;
    case SYS_IRQ_ACK:
        frame[0] = irq_ack(frame[0], caller->pid);
        break;
    case 10: /* SYS_YIELD */
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        break;
    case 11: /* SYS_EXIT */
        thread_exit(caller);
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        break;
    default:
        frame[0] = (uint32_t)-1;
    }
}
```

---

# Capítulo 15: Primitivas de Sincronização em Userspace

## 15.1 Filosofia

```
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║   "Se pode ser feito em userspace, DEVE ser feito           ║
║    em userspace."                                            ║
║                                                              ║
║   Kernel fornece MECANISMOS (IPC + notification).           ║
║   Userspace implementa POLÍTICAS (mutex, queue, timer...).  ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
```

## 15.2 Mapa Completo

```
┌───────────────────┬───────────┬─────────────────────────────────┐
│    Primitiva      │   Onde?   │   Como?                         │
├───────────────────┼───────────┼─────────────────────────────────┤
│ Mutex             │ Userspace │ Notification bit (token)        │
│ Semaphore         │ Userspace │ Notification bits               │
│ Queue / Channel   │ Userspace │ Shared mem + notification       │
│ Soft Timer        │ User svc  │ Timer server (thread dedicada)  │
│ Event Group       │ Userspace │ Notification (já É isso!)       │
│ Barrier           │ Userspace │ Endpoint + contador             │
│ RWLock            │ Userspace │ Endpoint server com protocolo   │
│ Mailbox           │ Userspace │ = IPC endpoint (já existe!)     │
│ Pipe              │ Userspace │ = Queue unidirecional           │
│ Memory Pool       │ Userspace │ mmap + free list em user lib    │
└───────────────────┴───────────┴─────────────────────────────────┘
```

## 15.3 Hierarquia de Construção

```
                    KERNEL PRIMITIVES
                    ┌────────────────────────┐
                    │  • send / recv / call  │
                    │  • reply / reply_recv  │
                    │  • notify_signal       │
                    │  • notify_wait / poll  │
                    │  • mmap / munmap       │
                    │  • irq_bind / irq_ack  │
                    └───────────┬────────────┘
                                │
              ┌─────────────────┼──────────────────┐
              │                 │                    │
              ▼                 ▼                    ▼
    ┌─────────────────┐ ┌──────────────┐ ┌────────────────────┐
    │   IPC endpoint  │ │ Notification │ │   Shared Memory    │
    │   (sync msg)    │ │ (async bits) │ │   (via mmap)       │
    └────────┬────────┘ └──────┬───────┘ └─────────┬──────────┘
             │                 │                     │
    ─────────┼─────────────────┼─────────────────────┼──────────
             │       USERSPACE LIBRARIES             │
             ▼                 ▼                     ▼
    ┌──────────────┐  ┌──────────────┐     ┌──────────────┐
    │ Mutex        │  │ Semaphore    │     │ Queue        │
    │ (notify bit) │  │ (notify bits)│     │ (shm+notify) │
    └──────────────┘  └──────────────┘     └──────────────┘
    ┌──────────────┐  ┌──────────────┐     ┌──────────────┐
    │ Mailbox      │  │ Event Group  │     │ Timer Server │
    │ (=endpoint)  │  │ (=notify)    │     │ (IPC thread) │
    └──────────────┘  └──────────────┘     └──────────────┘
```

## 15.4 Mutex via Notification Bit

```c
/* ===== libmutex.h ===== */

typedef struct {
    int      notify_id;
    uint32_t bit;
} umutex_t;

static inline int umutex_init(umutex_t *m, int notify_id, uint32_t bit)
{
    m->notify_id = notify_id;
    m->bit = bit;
    sys_notify_signal(notify_id, bit);  /* Token disponível */
    return 0;
}

static inline void umutex_lock(umutex_t *m)
{
    /*
     * Fast path: bit setado → retorna imediatamente (~30 cycles)
     * Slow path: bloqueia até unlock (~150 cycles)
     */
    sys_notify_wait(m->notify_id, m->bit);
}

static inline void umutex_unlock(umutex_t *m)
{
    sys_notify_signal(m->notify_id, m->bit);
}
```

## 15.5 Semáforo via Notification Bits

```c
/* ===== libsemaphore.h ===== */

/*
 * Notification JÁ É um semáforo.
 * 1 bit = binary semaphore
 * N bits = counting semaphore (max 32)
 */

typedef struct {
    int      notify_id;
    uint32_t all_bits;
} usem_t;

static inline int usem_init(usem_t *s, int notify_id,
                            uint8_t base_bit, uint8_t count)
{
    s->notify_id = notify_id;
    s->all_bits = 0;
    for (uint8_t i = 0; i < count; i++)
        s->all_bits |= (1u << (base_bit + i));
    sys_notify_signal(notify_id, s->all_bits);
    return 0;
}

static inline void usem_wait(usem_t *s)
{
    uint32_t got = sys_notify_wait(s->notify_id, s->all_bits);
    uint32_t one_bit = got & (-got);  /* Lowest set bit */
    uint32_t rest = got & ~one_bit;
    if (rest) sys_notify_signal(s->notify_id, rest);
}

static inline void usem_post(usem_t *s)
{
    uint32_t one_bit = s->all_bits & (-s->all_bits);
    sys_notify_signal(s->notify_id, one_bit);
}
```

## 15.6 Queue via Shared Memory + Notification

```c
/* ===== libqueue.h ===== */

/*
 * Ring buffer em shared memory + notification pra sinalizar.
 * ZERO IPC overhead quando tem espaço/itens (fast path).
 */

#define QUEUE_HAS_ITEM   (1u << 0)
#define QUEUE_HAS_SPACE  (1u << 1)

typedef struct {
    volatile uint32_t head, tail;
    uint32_t item_size, capacity;
    uint8_t  data[];
} queue_shared_t;

typedef struct {
    queue_shared_t *shm;
    int             notify_id;
} uqueue_t;

static inline int uqueue_init(uqueue_t *q, void *shared_mem,
                               size_t mem_size, uint32_t item_size,
                               int notify_id)
{
    q->shm = (queue_shared_t *)shared_mem;
    q->notify_id = notify_id;
    q->shm->head = q->shm->tail = 0;
    q->shm->item_size = item_size;
    q->shm->capacity = (mem_size - sizeof(queue_shared_t)) / item_size;
    sys_notify_signal(notify_id, QUEUE_HAS_SPACE);
    return 0;
}

static inline int uqueue_send(uqueue_t *q, const void *item, bool block)
{
    while (((q->shm->head + 1) % q->shm->capacity) == q->shm->tail) {
        if (!block) return -1;
        sys_notify_wait(q->notify_id, QUEUE_HAS_SPACE);
    }
    uint32_t idx = q->shm->head % q->shm->capacity;
    __builtin_memcpy(&q->shm->data[idx * q->shm->item_size],
                     item, q->shm->item_size);
    __asm volatile("dmb" ::: "memory");
    q->shm->head++;
    sys_notify_signal(q->notify_id, QUEUE_HAS_ITEM);
    return 0;
}

static inline int uqueue_recv(uqueue_t *q, void *item, bool block)
{
    while (q->shm->head == q->shm->tail) {
        if (!block) return -1;
        sys_notify_wait(q->notify_id, QUEUE_HAS_ITEM);
    }
    uint32_t idx = q->shm->tail % q->shm->capacity;
    __builtin_memcpy(item, &q->shm->data[idx * q->shm->item_size],
                     q->shm->item_size);
    __asm volatile("dmb" ::: "memory");
    q->shm->tail++;
    sys_notify_signal(q->notify_id, QUEUE_HAS_SPACE);
    return 0;
}
```

## 15.7 Event Group = Notification (já existe!)

```c
/* ===== libevent.h ===== */

/* Notification JÁ É um event group. Wrapper trivial. */

static inline uint32_t event_wait_any(int notify_id, uint32_t bits)
{
    return sys_notify_wait(notify_id, bits);
}

static inline uint32_t event_wait_all(int notify_id, uint32_t bits)
{
    uint32_t collected = 0;
    while ((collected & bits) != bits) {
        collected |= sys_notify_wait(notify_id, bits & ~collected);
    }
    return collected;
}

static inline void event_set(int notify_id, uint32_t bits)
{
    sys_notify_signal(notify_id, bits);
}
```

## 15.8 Software Timers via Timer Server

```c
/* ===== timer_server.c ===== */

#define MODULE_NAME timer_svc
#include "mem_domain.h"
#include "user_syscall.h"

#define TIMER_MSG_ONESHOT   1
#define TIMER_MSG_PERIODIC  2
#define TIMER_MSG_CANCEL    3

typedef struct {
    uint32_t expire_tick;
    uint32_t period;       /* 0 = oneshot */
    uint16_t owner_notify;
    uint32_t owner_bit;
    bool     active;
    uint16_t timer_id;
} timer_entry_t;

#define MAX_TIMERS 32

PRIVATE static timer_entry_t timers[MAX_TIMERS];
PRIVATE static uint16_t next_timer_id;

DEFINE_MEM_DOMAIN(timer_svc, PERM_READ | PERM_WRITE | PERM_USER);

static void process_expired(uint32_t now)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active || timers[i].expire_tick > now) continue;
        sys_notify_signal(timers[i].owner_notify, timers[i].owner_bit);
        if (timers[i].period > 0)
            timers[i].expire_tick = now + timers[i].period;
        else
            timers[i].active = false;
    }
}

void timer_server_main(void *arg)
{
    (void)arg;
    int ep_id = sys_ep_create();
    sys_register_service("timer", ep_id);

    int tick_notify = sys_notify_create();
    sys_irq_bind(SYSTICK_IRQn, tick_notify, 1);

    next_timer_id = 1;
    for (int i = 0; i < MAX_TIMERS; i++) timers[i].active = false;

    ipc_msg_t msg, reply;
    uint16_t badge, last_client = 0;
    bool have_reply = false;

    while (1) {
        ipc_recv_result_t result;
        if (have_reply) { sys_reply(last_client, &reply); have_reply = false; }
        sys_recv_with_notify(ep_id, tick_notify, 0x1, &result);

        if (result.is_notification) {
            process_expired(sys_get_ticks());
            sys_irq_ack(SYSTICK_IRQn);
        } else {
            msg = result.msg;
            switch (msg.label) {
            case TIMER_MSG_ONESHOT:
            case TIMER_MSG_PERIODIC: {
                int slot = -1;
                for (int i = 0; i < MAX_TIMERS; i++)
                    if (!timers[i].active) { slot = i; break; }
                if (slot < 0) {
                    reply.label = -1;
                } else {
                    uint32_t now = sys_get_ticks();
                    timers[slot] = (timer_entry_t){
                        .expire_tick  = now + msg.words[0],
                        .period       = (msg.label == TIMER_MSG_PERIODIC)
                                        ? msg.words[0] : 0,
                        .owner_notify = msg.words[1],
                        .owner_bit    = msg.words[2],
                        .active       = true,
                        .timer_id     = next_timer_id++,
                    };
                    reply.label = 0;
                    reply.words[0] = timers[slot].timer_id;
                }
                last_client = msg.words[5]; have_reply = true;
                break;
            }
            case TIMER_MSG_CANCEL: {
                uint16_t id = msg.words[0];
                reply.label = -1;
                for (int i = 0; i < MAX_TIMERS; i++) {
                    if (timers[i].active && timers[i].timer_id == id) {
                        timers[i].active = false;
                        reply.label = 0; break;
                    }
                }
                last_client = msg.words[5]; have_reply = true;
                break;
            }}
        }
    }
}
```

```c
/* ===== libtimer.h (client API) ===== */

typedef struct {
    int server_ep;
    int my_notify;
} utimer_ctx_t;

static inline int utimer_init(utimer_ctx_t *ctx)
{
    ctx->server_ep = sys_lookup_service("timer");
    ctx->my_notify = sys_notify_create();
    return (ctx->server_ep < 0 || ctx->my_notify < 0) ? -1 : 0;
}

static inline void usleep_ticks(utimer_ctx_t *ctx, uint32_t ticks)
{
    uint32_t bit = (1u << 0);
    ipc_msg_t msg = { .label = 1 }; /* TIMER_MSG_ONESHOT */
    msg.words[0] = ticks;
    msg.words[1] = ctx->my_notify;
    msg.words[2] = bit;
    sys_call(ctx->server_ep, &msg);
    sys_notify_wait(ctx->my_notify, bit);
}

static inline int utimer_periodic(utimer_ctx_t *ctx, uint32_t period,
                                   uint32_t bit)
{
    ipc_msg_t msg = { .label = 2 }; /* TIMER_MSG_PERIODIC */
    msg.words[0] = period;
    msg.words[1] = ctx->my_notify;
    msg.words[2] = bit;
    sys_call(ctx->server_ep, &msg);
    return (msg.label == 0) ? msg.words[0] : -1;
}
```

## 15.9 Comparação de Custo

```
╔═══════════════════════════════╦══════════════╦═══════════════════╗
║  Operação                     ║  FreeRTOS    ║  Microkernel      ║
╠═══════════════════════════════╬══════════════╬═══════════════════╣
║  Mutex lock (no contention)   ║  ~20 cyc     ║  ~30 cyc          ║
║  Mutex lock (contention)      ║  ~80 cyc     ║  ~150 cyc         ║
║  Semaphore give               ║  ~20 cyc     ║  ~20 cyc          ║
║  Semaphore take (available)   ║  ~25 cyc     ║  ~30 cyc          ║
║  Semaphore take (wait)        ║  ~80 cyc     ║  ~150 cyc         ║
║  Queue send (space avail)     ║  ~50 cyc     ║  ~40 cyc          ║
║  Queue send (full, wait)      ║  ~100 cyc    ║  ~160 cyc         ║
║  Timer create                 ║  ~50 cyc     ║  ~300 cyc (IPC)   ║
║  Timer expire                 ║  ~30 cyc     ║  ~50 cyc (signal) ║
╠═══════════════════════════════╬══════════════╬═══════════════════╣
║  PROTEÇÃO DE MEMÓRIA          ║  Nenhuma     ║  Total (MPU)      ║
║  ISOLAMENTO DE FALHAS         ║  Nenhum      ║  Total            ║
╚═══════════════════════════════╩══════════════╩═══════════════════╝
```

---

# Capítulo 16: Resumo e Referências

## 16.1 Lista Completa de Syscalls

| # | Nome | Descrição |
|---|---|---|
| 1 | `SYS_MMAP` | Alocar memória ou mapear periférico |
| 2 | `SYS_MUNMAP` | Liberar região |
| 30 | `SYS_EP_CREATE` | Criar endpoint de IPC |
| 31 | `SYS_SEND` | Enviar mensagem (bloqueante) |
| 32 | `SYS_RECV` | Receber mensagem (bloqueante) |
| 33 | `SYS_CALL` | Send + Recv atômico (RPC) |
| 34 | `SYS_REPLY` | Responder a um CALL |
| 35 | `SYS_REPLY_RECV` | Reply + Recv atômico |
| 40 | `SYS_NOTIFY_CREATE` | Criar notification |
| 41 | `SYS_NOTIFY_SIGNAL` | Sinalizar bits (non-blocking) |
| 42 | `SYS_NOTIFY_WAIT` | Esperar bits (bloqueante) |
| 43 | `SYS_NOTIFY_POLL` | Checar bits (non-blocking) |
| 50 | `SYS_RECV_WITH_NOTIFY` | Recv multiplexado |
| 60 | `SYS_IRQ_BIND` | Associar IRQ → notification |
| 61 | `SYS_IRQ_ENABLE` | Habilitar IRQ no NVIC |
| 63 | `SYS_IRQ_ACK` | Re-habilitar IRQ pós-tratamento |
| 10 | `SYS_YIELD` | Ceder CPU |
| 11 | `SYS_EXIT` | Terminar thread |

## 16.2 Diagrama de Arquitetura Final

```
┌─────────────────────────────────────────────────────────────────────┐
│                       SISTEMA COMPLETO                               │
│                                                                       │
│  ╔══════════════════════════════════════════════════════════════════╗ │
│  ║                    USERSPACE (unprivileged)                      ║ │
│  ║                                                                  ║ │
│  ║  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          ║ │
│  ║  │  UART Driver │  │  SPI Driver  │  │  Sensor App  │          ║ │
│  ║  │  Domain:uart │  │  Domain:spi  │  │  Domain:sens │          ║ │
│  ║  │  MMIO:USART2 │  │  MMIO:SPI1   │  │  Shared:ipc  │          ║ │
│  ║  │  IRQ:37      │  │  IRQ:35      │  │              │          ║ │
│  ║  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          ║ │
│  ║         │ SVC              │ SVC              │ SVC              ║ │
│  ╠═════════╪══════════════════╪══════════════════╪══════════════════╣ │
│  ║         ▼                  ▼                  ▼                  ║ │
│  ║  ┌──────────────────────────────────────────────────────────┐   ║ │
│  ║  │                    KERNEL (~2000 LOC)                     │   ║ │
│  ║  │  Scheduler │ IPC │ Notification │ IRQ Dispatch           │   ║ │
│  ║  │  MPU Mgr   │ Phys Alloc │ Capability Mgr │ Syscall Hdlr │   ║ │
│  ║  └──────────────────────────────────────────────────────────┘   ║ │
│  ╚══════════════════════════════════════════════════════════════════╝ │
│                                                                       │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ HARDWARE: MPU │ NVIC │ USART2 │ SPI1 │ GPIO │ Timers         │  │
│  └────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

## 16.3 Decisões de Design

```
╔═══════════════════════════════════════════════════════════════╗
║                 DECISÕES FUNDAMENTAIS                        ║
╠═══════════════════════════════════════════════════════════════╣
║  1. Endereços são físicos (sem MMU)                          ║
║  2. Alinhamento power-of-2 (buddy allocator)                 ║
║  3. Slots MPU são escassos (design threads enxutas)          ║
║  4. PRIVDEFENA=1 (kernel acessa tudo)                        ║
║  5. IPC síncrono nos registros (zero-copy ~24 bytes)         ║
║  6. IRQ = notification (kernel mask + signal)                ║
║  7. Capabilities pra periféricos e IRQs                      ║
║  8. W^X enforced (heap nunca exec, code nunca write)         ║
║  9. reply_recv é essencial (50% menos transições)            ║
║ 10. recv_with_notify (multiplexing IRQ + IPC)                ║
║ 11. Semaphore/mutex/queue/timer = userspace library          ║
║ 12. Timer = server thread (IPC + notification)               ║
╚═══════════════════════════════════════════════════════════════╝
```

## 16.4 Referências e Inspirações

| Sistema | Contribuição |
|---|---|
| **seL4** | Capabilities, IPC síncrono, Notifications |
| **L4 family** | IPC via registros, call/reply_recv |
| **Zephyr RTOS** | Memory domains, MPU management |
| **Tock OS** | Isolation via type system + MPU, grant model |
| **QNX** | Microkernel produção, message passing, IRQ userspace |
| **MINIX 3** | Driver restart, isolation, reliability |
| **Fuchsia/Zircon** | Handles, channels, ports, API moderna |

## 16.5 Roadmap de Implementação

```
Fase 1: Base
  □ Scheduler (round-robin com prioridade)
  □ Context switch (PendSV)
  □ MPU init + switch
  □ Kernel/user mode transition
  → Teste: duas threads user alternando

Fase 2: Memória
  □ Buddy allocator
  □ mmap anonymous
  □ mmap peripheral
  □ Memory domains + linker script
  → Teste: thread aloca memória, outra tenta acessar → fault

Fase 3: IPC
  □ Endpoints (send/recv/call/reply)
  □ reply_recv
  □ Notifications (signal/wait/poll)
  □ recv_with_notify
  → Teste: client-server ping-pong benchmark

Fase 4: IRQ
  □ IRQ binding
  □ IRQ dispatch (kernel stub)
  □ irq_enable/ack syscalls
  → Teste: UART driver em userspace recebendo bytes

Fase 5: Userspace Libraries
  □ Mutex (notification bit)
  □ Semaphore (notification bits)
  □ Queue (shared memory)
  □ Timer server
  → Teste: aplicação completa com driver + app + timer

Fase 6: Polish
  □ Capability system completo
  □ Process restart
  □ Name server
  □ Grant mechanism
  □ Stress testing + fuzzing
```

---

*Este documento serve como guia de implementação. Cada capítulo pode ser
implementado incrementalmente. Comece pelo scheduler e MPU switch,
depois adicione IPC, depois IRQ dispatch, depois mmap. Teste cada camada
isoladamente antes de integrar.*

*Referência de tamanho estimado do kernel: ~1500-2000 linhas de C + ~100
linhas de assembly. Bibliotecas userspace: ~500-800 linhas adicionais.*
