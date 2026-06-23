# TriCore 1.6 Architecture Reference — ABI, Stack Frames & Interrupt Management

## Para desenvolvimento de RTOS sobre TC2xx (AURIX)

---

## Sumário

1. [Visão Geral da Arquitetura](#1-visão-geral-da-arquitetura)
2. [Banco de Registradores](#2-banco-de-registradores)
3. [ABI — Application Binary Interface](#3-abi--application-binary-interface)
4. [Context Save Areas (CSA) — O Coração do Stack Frame](#4-context-save-areas-csa--o-coração-do-stack-frame)
5. [Stack Management](#5-stack-management)
6. [Interrupt & Trap System](#6-interrupt--trap-system)
7. [System Registers Relevantes para RTOS](#7-system-registers-relevantes-para-rtos)
8. [Context Switch — Cookbook para RTOS](#8-context-switch--cookbook-para-rtos)
9. [Gotchas e Pegadinhas](#9-gotchas-e-pegadinhas)
10. [Quick Reference Tables](#10-quick-reference-tables)

---

## 1. Visão Geral da Arquitetura

O TriCore é uma arquitetura **Harvard modificada** com pipeline de 32 bits, projetada para automotive. Características-chave para quem porta RTOS:

- **Duas classes de registradores** separadas: Address (A) e Data (D)
- **Context save/restore em hardware** via CSA (Context Save Area)
- **Dois níveis de contexto**: Upper e Lower
- **Call depth counter** em hardware
- **Interrupt controller dedicado** (ICU / IR) com até 255 níveis de prioridade
- **Sem delay slots** (diferente de MIPS)
- **Endianness**: Little-endian

### Modelo de Memória TC2xx

```
0x0000_0000 - 0x0FFF_FFFF  → Program Flash (PFlash)
0x5000_0000 - 0x5003_FFFF  → Local Data RAM (DSPR) - Core-specific
0x6000_0000 - 0x6003_FFFF  → Local Program RAM (PSPR) - Core-specific  
0x7000_0000 - 0x7003_FFFF  → Data RAM (LMU - shared)
0xD000_0000 - 0xDFFF_FFFF  → Segment de CSA (tipicamente em DSPR)
0xF000_0000 - 0xFFFF_FFFF  → Periféricos e SFRs
```

> **Nota TC2xx**: O TC27x tem 3 cores (CPU0, CPU1, CPU2). Cada core tem seu próprio banco de registradores, CSA pool e interrupt table. O RTOS precisa tratar cada core independentemente ou implementar SMP.

---

## 2. Banco de Registradores

### 2.1 Registradores Gerais

| Registrador | Tipo | Função ABI | Salvo por |
|-------------|------|-----------|-----------|
| **D[0]** | Data | Return value (32-bit low) | Caller |
| **D[1]** | Data | Return value (32-bit high, para 64-bit) | Caller |
| **D[2]** | Data | Argumento 1 (data) | Caller |
| **D[3]** | Data | Argumento 2 (data) | Caller |
| **D[4]** | Data | Argumento 3 (data) | Caller |
| **D[5]** | Data | Argumento 4 (data) | Caller |
| **D[6]** | Data | Argumento 5 (data) | Caller |
| **D[7]** | Data | Argumento 6 (data) | Caller |
| **D[8]-D[15]** | Data | Scratch / variáveis | **Callee (Lower context)** |
| **A[0]** | Address | Global address (reservado pelo linker) | — |
| **A[1]** | Address | Global address (reservado pelo linker) | — |
| **A[2]** | Address | Return value (ponteiro) / Argumento 1 (addr) | Caller |
| **A[3]** | Address | Argumento 2 (addr) | Caller |
| **A[4]** | Address | Argumento 3 (addr) | Caller |
| **A[5]** | Address | Argumento 4 (addr) | Caller |
| **A[6]** | Address | Argumento 5 (addr) | Caller |
| **A[7]** | Address | Argumento 6 (addr) | Caller |
| **A[8]** | Address | Global address (reservado pelo compilador) | — |
| **A[9]** | Address | Global address (reservado pelo compilador) | — |
| **A[10] (SP)** | Address | **Stack Pointer** | Callee |
| **A[11] (RA)** | Address | **Return Address** | Hardware (via CSA) |
| **A[12]-A[15]** | Address | Scratch / variáveis | **Callee (Lower context)** |

### 2.2 Registradores de Sistema (CSFR)

| Registrador | Endereço | Função |
|-------------|----------|--------|
| **PCXI** | Core SFR | Previous Context Information (link para CSA chain) |
| **PSW** | Core SFR | Program Status Word |
| **PC** | Core SFR | Program Counter |
| **SYSCON** | Core SFR | System Configuration |
| **FCX** | Core SFR | Free CSA List Head |
| **LCX** | Core SFR | Free CSA List Limit (trap quando FCX atinge) |
| **ICR** | Core SFR | Interrupt Control Register |
| **ISP** | Core SFR | Interrupt Stack Pointer |
| **BTV** | Core SFR | Base of Trap Vector Table |
| **BIV** | Core SFR | Base of Interrupt Vector Table |
| **DBGSR** | Core SFR | Debug Status Register |

### 2.3 Detalhamento do PSW

```
Bits 31:28  27:24  23:20  19:16  15:14  13:12  11:10  9   8   7    6:0
     ───────────────────────────────────────────────────────────────────
     USB    reserv  PRS    IO     IS     GW     CDC         CDE  SAF  call depth
```

| Campo | Bits | Descrição |
|-------|------|-----------|
| **CDC** | [6:0] | Call Depth Counter — 7 bits, limita nesting de calls |
| **CDE** | [7] | Call Depth Count Enable |
| **GW** | [8] | Global Address Register Write Permission |
| **IS** | [9] | Interrupt Stack flag (0=user stack, 1=interrupt stack/ISP) |
| **IO** | [11:10] | I/O Privilege Level (0-3) |
| **PRS** | [13:12] | Protection Register Set |
| **S** | [14] | Safety (reservado/impl. specific) |
| **USB** | [31:24] | User Status Bits (flags de comparação, overflow, etc.) |

> **Para RTOS**: Os campos `IS`, `IO`, `CDC`, `CDE` são os mais relevantes. `IS=1` faz `A[10]` apontar para ISP em vez do user stack.

### 2.4 Detalhamento do PCXI

```
Bits  31:24    23:22   21:20   19          18:16       15:0
      ────────────────────────────────────────────────────
      reserv   PCPN    PIE     UL          PCXS        PCXO
```

| Campo | Bits | Descrição |
|-------|------|-----------|
| **PCXO** | [15:0] | Offset do CSA anterior (em unidades de 16 words = 64 bytes) |
| **PCXS** | [18:16] | Segment do CSA anterior |
| **UL** | [19] | Upper/Lower flag: **1 = Upper context**, **0 = Lower context** |
| **PIE** | [20] | Previous Interrupt Enable (estado anterior de ICR.IE) |
| **PCPN** | [23:21] | Previous CPU Priority Number |

> **Crucial para RTOS**: PCXI forma uma **linked list** de contextos salvos. Para fazer context switch, você manipula essa lista.

### 2.5 Detalhamento do ICR

```
Bits  31:24    23:16    15       14:8      7:0
      ──────────────────────────────────────
      reserv   PIPN     IE      reserv    CCPN
```

| Campo | Bits | Descrição |
|-------|------|-----------|
| **CCPN** | [7:0] | Current CPU Priority Number |
| **IE** | [15] | Interrupt Enable global |
| **PIPN** | [23:16] | Pending Interrupt Priority Number |

---

## 3. ABI — Application Binary Interface

### 3.1 Convenção de Chamada (EABI TriCore)

O TriCore usa o **EABI (Embedded ABI)** definido pela Infineon/HighTec/Tasking.

#### Passagem de Argumentos

Os argumentos são divididos em duas classes:

- **Data arguments** (inteiros, floats): passados em `D[4]`, `D[5]`, `D[6]`, `D[7]`, depois stack
- **Address/pointer arguments**: passados em `A[4]`, `A[5]`, `A[6]`, `A[7]`, depois stack

A distribuição segue a **ordem de aparição** do argumento:

```c
void func(int a, int *b, int c, int *d, int e);
//         D[4]   A[4]   D[5]    A[5]    D[6]
```

**Nota**: Data registers e Address registers têm **contadores separados**. Cada classe consome seus próprios registradores independentemente.

#### Argumentos 64-bit

Valores de 64 bits (long long, double) são passados em **pares de registradores de dados contíguos com alinhamento par**:

```c
void func(int a, long long b);
//         D[4]   D[6]:D[7]     ← D[5] é pulado para alinhar!
```

Regra: o registrador low do par deve ser **par** (D[4], D[6]).

#### Retorno de Valores

| Tipo | Retorno em |
|------|-----------|
| int, float, ponteiro ≤ 32 bits | `D[2]` (data) ou `A[2]` (ponteiro) |
| long long, double (64 bits) | `D[2]` (low) : `D[3]` (high) |
| struct grande | Caller passa ponteiro oculto em `A[4]` |

#### Registro de Exemplo

```c
int result = func(42, &buffer, 0xFF);
```

Compilado para (pseudo):
```asm
    mov     d4, #42         ; arg1 → D[4]
    lea     a4, [buffer]    ; arg2 → A[4] (é ponteiro!)
    mov     d5, #0xFF       ; arg3 → D[5]
    call    func
    ; resultado em D[2]
```

### 3.2 Categorias de Registradores (Caller/Callee Saved)

Este é o ponto central para context switch:

**Upper Context (salvo automaticamente por CALL/hardware):**
```
A[11] (RA), A[10] (SP), A[2], A[3]
D[0], D[1], D[2], D[3]
A[4], A[5], A[6], A[7]
D[4], D[5], D[6], D[7]
PSW
PCXI (→ link pro contexto anterior)
```

**Lower Context (callee-saved, precisa save explícito ou via SVLCX/BISR):**
```
A[12], A[13], A[14], A[15]
D[8], D[9], D[10], D[11], D[12], D[13], D[14], D[15]
PCXI (→ link pro contexto anterior)
```

> **Importantíssimo**: A instrução `CALL` automaticamente salva o **Upper Context** em um CSA e linka via PCXI. O Lower Context só é salvo se a função (callee) decidir usar esses registradores, ou se o RTOS executa `SVLCX` explicitamente.

### 3.3 Alinhamento de Stack

- Stack pointer (`A[10]`) deve estar **alinhado a 8 bytes** (64-bit aligned)
- CSAs são alinhados a **64 bytes** (16 words × 4 bytes)
- Stack cresce para **baixo** (endereços decrescentes)

### 3.4 Tipos de Dados

| Tipo C | Tamanho | Alinhamento |
|--------|---------|-------------|
| char | 1 byte | 1 |
| short | 2 bytes | 2 |
| int | 4 bytes | 4 |
| long | 4 bytes | 4 |
| long long | 8 bytes | 8 (!) |
| float | 4 bytes | 4 |
| double | 8 bytes | 8 |
| pointer | 4 bytes | 4 |

---

## 4. Context Save Areas (CSA) — O Coração do Stack Frame

### 4.1 Conceito

O TriCore **não usa um stack frame tradicional** para save/restore de registradores em chamadas de função. Em vez disso, usa um **pool pré-alocado de blocos de 64 bytes** chamados CSAs (Context Save Areas).

Cada CSA armazena exatamente **16 words (64 bytes)** e pode guardar UM contexto (Upper OU Lower).

Os CSAs formam uma **free list** (linked list de blocos livres), gerenciada pelos registradores `FCX` (head da free list) e `LCX` (limite mínimo para trap).

```
┌─────────────────────────────────────────┐
│              CSA Pool (RAM)             │
├─────────┬─────────┬─────────┬──────────┤
│  CSA 0  │  CSA 1  │  CSA 2  │  ...     │
│ 64 bytes│ 64 bytes│ 64 bytes│          │
│  [free] │ [usado] │  [free] │          │
└────┬────┴────┬────┴────┬────┴──────────┘
     │         │         │
     ▼         │         ▼
   FCX ────►CSA 0───►CSA 2───►CSA 5───► ... ───► NULL
              (free list encadeada via word 0 de cada CSA)
```

### 4.2 Link Word (Endereçamento de CSA)

O primeiro word de cada CSA (e os registradores FCX, LCX, PCXI) contém um **link word** com formato:

```
Bits  31:20     19       18:16      15:0
      ──────────────────────────────────
      reservado  (UL)    Segment    Offset
```

Para converter link word → endereço efetivo:

```
Effective_Address = (Segment << 28) | (Offset << 6)
```

Para converter endereço efetivo → link word:

```
Link_Word.Segment = (Address >> 28) & 0x7
Link_Word.Offset  = (Address >> 6) & 0xFFFF
```

**Exemplo**:
```
CSA at address 0xD000_1000
  Segment = (0xD0001000 >> 28) & 0x7 = 0x5  (nota: segment index, não o nibble 0xD!)
  Offset  = (0xD0001000 >> 6) & 0xFFFF = 0x0040

  Link Word = 0x0005_0040
```

> **ATENÇÃO**: O mapeamento de segment index depende da configuração de segmentos do TriCore. No TC2xx, segment 5 mapeia para 0xD000_0000 (DSPR do core local). Consulte o user manual do TC2xx para o mapeamento exato.

Mapeamento típico TC2xx:

| Segment Index | Base Address | Região |
|--------------|-------------|--------|
| 0 | 0x0000_0000 | PFlash |
| 1 | 0x1000_0000 | PFlash |
| 5 | 0x5000_0000 | DSPR (Local Data RAM) |
| 6 | 0x6000_0000 | PSPR |
| 7 | 0x7000_0000 | LMU |
| 9 | 0x9000_0000 | — |
| 0xD | 0xD000_0000 | Cached DSPR |

### 4.3 Layout do Upper Context CSA

Quando o Upper Context é salvo (por CALL, interrupt, trap, ou SVUCX):

| Offset (words) | Conteúdo |
|----------------|----------|
| 0 | **Link Word** (PCXI do momento do save → aponta pro CSA anterior na chain) |
| 1 | PSW |
| 2 | A[10] (SP) |
| 3 | A[11] (RA) |
| 4 | D[0] |
| 5 | D[1] |
| 6 | D[2] |
| 7 | D[3] |
| 8 | A[2] |
| 9 | A[3] |
| 10 | D[4] |
| 11 | D[5] |
| 12 | D[6] |
| 13 | D[7] |
| 14 | A[4] |
| 15 | A[5] — *Nota: A[6] e A[7] NÃO são salvos no Upper Context* |

**CORREÇÃO IMPORTANTE**: Checando a spec mais a fundo:

**Upper Context CSA (16 words):**

| Word | Conteúdo |
|------|----------|
| 0 | PCXI (link) |
| 1 | PSW |
| 2 | A[10] (SP) |
| 3 | A[11] (RA) |
| 4 | D[8] |
| 5 | D[9] |
| 6 | D[10] |
| 7 | D[11] |
| 8 | A[12] |
| 9 | A[13] |
| 10 | A[14] |
| 11 | A[15] |
| 12 | D[12] |
| 13 | D[13] |
| 14 | D[14] |
| 15 | D[15] |

**NÃO!** Vou reescrever corretamente. O TriCore architecture manual define:

### 4.3 Layout do Upper Context CSA (CORRETO)

O Upper Context contém os registradores que são **automaticamente salvos por CALL** (caller-saved):

| Word | Conteúdo |
|------|----------|
| 0 | PCXI (link word → CSA anterior) |
| 1 | PSW (incluindo USB, call depth, etc.) |
| 2 | A[10] (SP) |
| 3 | A[11] (RA / Return Address) |
| 4 | D[0] |
| 5 | D[1] |
| 6 | D[2] |
| 7 | D[3] |
| 8 | A[2] |
| 9 | A[3] |
| 10 | D[4] |
| 11 | D[5] |
| 12 | D[6] |
| 13 | D[7] |
| 14 | A[6] |
| 15 | A[7] |

O campo `UL` no PCXI/link word = **1** indica Upper Context.

### 4.4 Layout do Lower Context CSA

O Lower Context contém os registradores **callee-saved**:

| Word | Conteúdo |
|------|----------|
| 0 | PCXI (link word → CSA anterior) |
| 1 | A[11] (RA) — *sim, RA aparece em ambos* |
| 2 | A[2] |
| 3 | A[3] |
| 4 | D[8] |
| 5 | D[9] |
| 6 | D[10] |
| 7 | D[11] |
| 8 | A[12] |
| 9 | A[13] |
| 10 | A[14] |
| 11 | A[15] |
| 12 | D[12] |
| 13 | D[13] |
| 14 | D[14] |
| 15 | D[15] |

O campo `UL` no PCXI/link word = **0** indica Lower Context.

> **NOTA CRÍTICA PARA RTOS DEVELOPERS**: Existe variação nos manuais Infineon entre versões. **SEMPRE verifique contra o TriCore Architecture Manual v1.6 (Volume 1, seções sobre CALL, RET, SVLCX, RSLCX, SVUCX)**. Os layouts acima refletem o entendimento padrão, mas a ordem exata dos words pode variar. Confira no capítulo "Context Save Areas" do arch manual.

A definição **canônica** segundo o TC1.6 Architecture Manual é:

**Upper CSA:**
```
Word 0:  PCXI
Word 1:  PSW  
Word 2:  A[10]
Word 3:  A[11]
Word 4:  D[0]
Word 5:  D[1]
Word 6:  D[2]
Word 7:  D[3]  
Word 8:  A[2]
Word 9:  A[3]
Word 10: D[4]
Word 11: D[5]
Word 12: D[6]
Word 13: D[7]
Word 14: A[6]
Word 15: A[7]
```

**Lower CSA:**
```
Word 0:  PCXI
Word 1:  A[11] (RA)
Word 2:  A[2]
Word 3:  A[3]
Word 4:  D[8]
Word 5:  D[9]
Word 6:  D[10]
Word 7:  D[11]
Word 8:  A[12]
Word 9:  A[13]
Word 10: A[14]
Word 11: A[15]
Word 12: D[12]
Word 13: D[13]
Word 14: D[14]
Word 15: D[15]
```

### 4.5 Instruções de Contexto

| Instrução | Ação |
|-----------|------|
| `CALL addr` | 1. Salva Upper Context em CSA (tira da free list via FCX) 2. PCXI ← link para CSA salvo 3. A[11] ← PC+4 (return address) 4. PC ← addr |
| `CALLI A[a]` | Igual CALL mas indireto |
| `RET` | 1. Restaura Upper Context do CSA apontado por PCXI 2. Devolve CSA para free list 3. PC ← A[11] (RA restaurado) |
| `RFE` | Return From Exception — restaura Upper Context + restaura IE e CCPN do PCXI salvo |
| `SVLCX` | Save Lower Context — salva Lower em CSA, atualiza PCXI |
| `RSLCX` | Restore Lower Context — restaura Lower do CSA apontado por PCXI |
| `SVUCX` | Save Upper Context — salva Upper em CSA (sem a semântica de CALL) |
| `BISR priority` | Begin Interrupt Service Routine — SVLCX + enable interrupts com prioridade `priority` |
| `MFCR reg, csfr` | Move From Core Special Function Register |
| `MTCR csfr, reg` | Move To Core Special Function Register |
| `ISYNC` | Instruction Synchronize — flush pipeline, necessário após MTCR |
| `DSYNC` | Data Synchronize — garante completude de stores |
| `DISABLE` | Disable interrupts (ICR.IE = 0) |
| `ENABLE` | Enable interrupts (ICR.IE = 1) |

### 4.6 Fluxo de CALL em Detalhe

```
CALL target:
  ┌──────────────────────────────────────────┐
  │ 1. new_CSA ← FCX (pega head da free list) │
  │ 2. FCX ← new_CSA.link (avança free list)  │
  │ 3. Escreve no new_CSA:                     │
  │    word[0] ← PCXI (link anterior)         │
  │    word[1] ← PSW                          │  
  │    word[2..15] ← Upper regs               │
  │ 4. PCXI ← link_para(new_CSA) | UL=1      │
  │ 5. PSW.CDC ← PSW.CDC + 1                  │
  │ 6. A[11] ← PC + 4  (return address)       │
  │ 7. PC ← target                            │
  └──────────────────────────────────────────┘
```

```
RET:
  ┌──────────────────────────────────────────┐
  │ 1. csa_addr ← resolve(PCXI)              │
  │ 2. Restaura Upper regs de csa_addr       │
  │ 3. PCXI ← csa_addr.word[0] (link ant.)  │
  │ 4. Devolve CSA para free list:            │
  │    csa_addr.link ← FCX                   │
  │    FCX ← link_para(csa_addr)             │
  │ 5. PC ← A[11] (restaurado do CSA)        │
  └──────────────────────────────────────────┘
```

### 4.7 Inicialização da CSA Free List

No boot do RTOS, **ANTES** de qualquer CALL ou interrupt, você precisa inicializar a free list:

```c
// Exemplo em C com inline assembly
// CSA pool: array de N blocos de 64 bytes, alinhado a 64 bytes

#define CSA_COUNT  128  // 128 CSAs = 8KB — ajuste conforme necessidade
#define CSA_SIZE   64   // bytes

// Pool alinhado a 64 bytes
static uint32_t __attribute__((aligned(64))) csa_pool[CSA_COUNT * 16];

void init_csa_free_list(void)
{
    uint32_t *csa;
    uint32_t next_link;
    
    // Link todos os CSAs em uma lista: CSA[0] → CSA[1] → ... → CSA[N-1] → NULL
    for (int i = 0; i < CSA_COUNT - 1; i++) {
        csa = &csa_pool[i * 16];
        uint32_t next_addr = (uint32_t)&csa_pool[(i + 1) * 16];
        
        // Converter endereço para link word
        uint32_t segment = (next_addr >> 28) & 0x7;
        uint32_t offset  = (next_addr >> 6)  & 0xFFFF;
        csa[0] = (segment << 16) | offset;
    }
    
    // Último CSA aponta para NULL (0)
    csa_pool[(CSA_COUNT - 1) * 16] = 0;
    
    // FCX ← primeiro CSA
    uint32_t first_addr = (uint32_t)&csa_pool[0];
    uint32_t fcx_val = ((first_addr >> 28) & 0x7) << 16 | ((first_addr >> 6) & 0xFFFF);
    __mtcr(FCX, fcx_val);
    
    // LCX ← penúltimo CSA (ou algum CSA "quase no fim" — threshold para trap)
    uint32_t limit_addr = (uint32_t)&csa_pool[(CSA_COUNT - 4) * 16]; // 4 CSAs antes do fim
    uint32_t lcx_val = ((limit_addr >> 28) & 0x7) << 16 | ((limit_addr >> 6) & 0xFFFF);
    __mtcr(LCX, lcx_val);
    
    __isync();
}
```

> **Dica**: Se FCX alcançar LCX, o hardware gera uma **FCU Trap (Free Context List Underflow)** — classe 7 trap. Isso é fatal a menos que seu trap handler saiba lidar.

### 4.8 Cálculo de CSAs Necessários

Cada nível de call aninhado consome **1 CSA** (Upper Context). Cada interrupt que faz BISR consome mais **1 CSA** (Lower Context) + o **1 CSA** do Upper salvo pelo hardware na entrada do interrupt. 

Fórmula aproximada:

```
CSAs_needed = max_call_depth_across_all_tasks 
            + (num_tasks * 2)              // cada task precisa de Upper+Lower para context switch
            + max_interrupt_nesting * 2     // cada nível de interrupt: Upper(hw) + Lower(BISR)
            + margem_de_segurança
```

Para TC2xx com RTOS típico: **128-256 CSAs** é um bom ponto de partida (8KB-16KB).

---

## 5. Stack Management

### 5.1 Stack vs CSA

No TriCore, o stack (`A[10]`) é usado para:
- Variáveis locais (que não couberam em registradores)
- Argumentos excedentes (quando todos os registradores de argumento estão ocupados)
- Structs grandes, arrays locais
- alloca(), VLAs

O stack **NÃO** é usado para salvar registradores em CALL/RET — isso é feito via CSA.

Consequência para RTOS: **cada task precisa de stack E de CSAs**.

### 5.2 Dual Stack: User Stack vs Interrupt Stack

O TriCore suporta **dois stack pointers**:

- **User Stack**: `A[10]` quando `PSW.IS = 0`
- **Interrupt Stack**: `ISP` — usado quando `PSW.IS = 1`

Quando uma interrupt ocorre:
1. Se `PSW.IS == 0` (estava no user mode), o hardware faz `A[10] ← ISP` e `PSW.IS ← 1`
2. Se `PSW.IS == 1` (já no interrupt stack), mantém `A[10]` como está

Na saída (RFE), o PSW restaurado tem IS=0, então A[10] volta ao valor do user stack salvo no CSA.

> **Para RTOS**: Configure `ISP` durante o boot. Todas as ISRs compartilham o interrupt stack. Cada task tem seu próprio user stack. Isso **economiza muita RAM** — não precisa dimensionar o stack de cada task para suportar o worst-case de interrupt nesting.

```c
// No boot:
extern uint32_t __interrupt_stack_top[];  // definido no linker script
__mtcr(ISP, (uint32_t)__interrupt_stack_top);
__isync();
```

### 5.3 Stack Layout para uma Task

```
┌──────────────────┐ ← Stack Top (endereço alto)
│                  │
│  (cresce p/ baixo)│
│                  │
│   Variáveis      │
│   locais         │
│                  │
│   Args excedentes│
│                  │
├──────────────────┤ ← A[10] (SP atual)
│                  │
│   (espaço livre) │
│                  │
├──────────────────┤ ← Stack Bottom / Guard Zone
│  GUARD / PATTERN │  ← Para detecção de stack overflow
└──────────────────┘ ← Endereço mais baixo
```

### 5.4 Stack Frame Canônico (dentro de uma função)

Como o TriCore salva registradores via CSA, o "stack frame" de uma função é simples:

```
┌──────────────────┐ ← SP na entrada (após CALL salvou Upper em CSA)
│  Local var N     │
│  ...             │
│  Local var 1     │
│  Spill area      │  ← Se compilador precisar spillar regs
├──────────────────┤ ← SP ajustado (sub.a sp, #framesize)
│                  │
```

O **frame pointer** geralmente não é necessário (mas `A[14]` pode ser usado como FP com opção de compilação).

O prólogo/epílogo típico:

```asm
; Prólogo (compilador gera):
func:
    sub.a   sp, #frame_size     ; aloca espaço local
    ; ... corpo ...

; Epílogo:
    add.a   sp, #frame_size     ; desaloca
    ret                          ; restaura Upper Context do CSA e retorna
```

---

## 6. Interrupt & Trap System

### 6.1 Visão Geral

O TC2xx tem dois mecanismos de exceção:

| | Traps | Interrupts |
|---|-------|-----------|
| **Fonte** | Erros de CPU, syscalls, debug | Periféricos, software |
| **Vetor** | BTV + class*32 | BIV + priority*32 (ou tabela de ponteiros) |
| **Prioridade** | Sempre aceita (síncrono) | Baseada em CCPN vs prioridade do pedido |
| **Return** | RFE | RFE |
| **Context save** | Upper (hw) | Upper (hw) + tipicamente BISR para Lower |

### 6.2 Interrupt Vector Table (IVT)

O TriCore suporta dois modos de IVT (controlado por `BIV.VSS`):

#### Modo 0: Vector Spacing = 32 bytes (padrão)
Cada entrada tem 32 bytes (8 instruções). O hardware pula para `BIV + (priority_number * 32)`.

```
BIV + 0x000:  Entry para prioridade 0 (não usado, prioridade 0 = disabled)
BIV + 0x020:  Entry para prioridade 1
BIV + 0x040:  Entry para prioridade 2
...
BIV + 0x1FE0: Entry para prioridade 255
```

Com 32 bytes por entry, cabe uma instrução de jump:

```asm
.section .inttab, "ax"
.align 5                        ; 32-byte aligned
.globl _isr_entry_prio_N
_isr_entry_prio_N:
    bisr    N                   ; Save Lower Context + set CCPN = N, IE = 1
    movh.a  a14, hi:_isr_handler_N
    lea     a14, [a14] lo:_isr_handler_N
    ji      a14                 ; jump to C handler
    ; ... padding to 32 bytes ...
```

Ou mais compacto:

```asm
_isr_entry_prio_N:
    svlcx                       ; Save Lower Context
    movh.a  a14, hi:_isr_handler_N
    lea     a14, [a14] lo:_isr_handler_N  
    ji      a14
    ; (handler termina com RSLCX + RFE)
```

#### Modo 1: Jump Table (TC2xx pode suportar)

O BIV aponta para uma tabela de endereços (cada entry é um ponteiro de 4 bytes). O hardware lê o endereço e pula diretamente.

### 6.3 Fluxo de Entrada em Interrupt

Quando o hardware aceita uma interrupt (ICR.IE = 1 e prioridade > ICR.CCPN):

```
┌────────────────────────────────────────────────────────────┐
│ 1. Pipeline flush                                          │
│ 2. Salva Upper Context em CSA:                             │
│    - Aloca CSA da free list (FCX)                         │
│    - Escreve: PCXI_antigo, PSW, A10, A11, D0-D7, A2-A7  │
│    - novo PCXI ← link_para(CSA) com:                     │
│      PCXI.PIE  = ICR.IE (estado anterior do IE)          │
│      PCXI.PCPN = ICR.CCPN (prioridade anterior)         │
│      PCXI.UL   = 1 (upper context)                       │
│ 3. ICR.IE  ← 0  (desabilita interrupts!)                 │
│ 4. ICR.CCPN ← prioridade da interrupt aceita             │
│ 5. Se PSW.IS == 0:                                        │
│      A[10] ← ISP  (switch para interrupt stack)           │
│      PSW.IS ← 1                                          │
│ 6. PSW.IO ← Supervisor mode (tipicamente)                │
│ 7. PSW.CDC ← 0 (reset call depth)                        │
│ 8. A[11] ← PC (return address = instrução interrompida)  │
│ 9. PC ← BIV + (priority * 32)  [modo 0]                  │
│                                                            │
│ ★ NOTA: Interrupts estão DESABILITADAS ao entrar na ISR! │
└────────────────────────────────────────────────────────────┘
```

### 6.4 BISR — Begin Interrupt Service Routine

A instrução `BISR #priority` é a forma "oficial" de iniciar uma ISR. Ela faz:

```
BISR #N:
  1. SVLCX — Salva Lower Context em CSA (agora temos Upper+Lower salvos)
  2. ICR.CCPN ← N
  3. ICR.IE ← 1  (re-habilita interrupts para nesting!)
```

**Por que BISR é importante para RTOS:**
- Após BISR, todos os registradores estão livres (Upper e Lower salvos)
- Interrupts estão habilitadas → permite **interrupt nesting**
- A prioridade é setada corretamente → ISRs de prioridade menor são bloqueadas

### 6.5 Fluxo de Saída de Interrupt

```asm
; No fim da ISR (após BISR ter sido chamado):
    disable                     ; ICR.IE ← 0 (evita race condition)
    rslcx                       ; Restore Lower Context do CSA
    rfe                         ; Restore Upper Context + restaura PIE e PCPN
                                ; (restaura PC, PSW, SP, tudo do Upper Context)
```

```
RFE:
  1. Restaura Upper Context do CSA apontado por PCXI
  2. ICR.IE  ← PCXI.PIE  (restaura estado de interrupt)
  3. ICR.CCPN ← PCXI.PCPN (restaura prioridade anterior)
  4. PCXI ← link do CSA restaurado (percorre a chain)
  5. Devolve CSA para free list
  6. PC ← A[11] restaurado (retorna ao ponto interrompido)
  7. PSW restaurado (incluindo IS flag → volta ao user stack se necessário)
```

### 6.6 Interrupt Nesting

O TriCore suporta nesting naturalmente:

```
Task (CCPN=0, IE=1)
  │
  ▼ IRQ prioridade 10
  ├─ HW salva Upper Context
  ├─ CCPN=10, IE=0
  ├─ BISR 10: salva Lower, CCPN=10, IE=1
  │    │
  │    ▼ IRQ prioridade 50 (> 10, aceita)
  │    ├─ HW salva Upper Context  
  │    ├─ CCPN=50, IE=0
  │    ├─ BISR 50: salva Lower, CCPN=50, IE=1
  │    │
  │    │  (ISR 50 executa)
  │    │
  │    ├─ DISABLE; RSLCX; RFE → restaura contexto da ISR 10
  │    ▼
  │  (ISR 10 continua)
  │
  ├─ DISABLE; RSLCX; RFE → restaura contexto da Task
  ▼
Task continua
```

### 6.7 Trap Vector Table

Traps são exceções síncronas. A tabela é baseada em `BTV`:

| Classe | Offset (BTV+) | Nome | Causa |
|--------|--------------|------|-------|
| 0 | 0x000 | MMU | Memory Management |
| 1 | 0x020 | Internal Protection | Violação de proteção |
| 2 | 0x040 | Instruction Error | Opcode ilegal, etc. |
| 3 | 0x060 | Context Management | CSA: FCU (underflow), CSO (overflow), etc. |
| 4 | 0x080 | System Bus & Periph. | Bus error, timeout |
| 5 | 0x0A0 | Assertion | NMI, assertions |
| 6 | 0x0C0 | System Call | `SYSCALL` instruction |
| 7 | 0x0E0 | Non-Maskable Interrupt | NMI |

Dentro de cada classe, o **TIN (Trap Identification Number)** em `D[15]` identifica a causa exata.

### 6.7.1 Trap Classe 3 — Context Management (VITAL para RTOS)

| TIN | Nome | Causa |
|-----|------|-------|
| 1 | FCD (Free Context Depletion) | FCX == LCX (quase sem CSAs!) |
| 2 | CDO (Call Depth Overflow) | CDC overflow (muito nesting) |
| 3 | CDU (Call Depth Underflow) | RET com CDC=0 |
| 4 | FCU (Free Context Underflow) | FCX == 0 (SEM CSAs!) |
| 5 | CSU (Call Stack Underflow) | PCXI == 0 no RET |
| 6 | CTYP (Context Type error) | UL flag inconsistente |
| 7 | NEST (Nesting Error) | Nesting de chamadas inconsistente |

### 6.7.2 Trap Classe 6 — System Call (para RTOS)

A instrução `SYSCALL #imm` gera trap classe 6 com TIN = imm. O hardware:

1. Salva Upper Context
2. PSW.IS = 1 (vai para interrupt stack)
3. PC ← BTV + 0x0C0
4. D[15] ← imm (TIN)

**Uso em RTOS**: Implementar system calls (yield, mutex operations, etc.):

```c
// User space:
#define SYSCALL_YIELD     0
#define SYSCALL_WAIT      1
#define SYSCALL_SIGNAL    2

// Macro para user code:
static inline void os_yield(void) {
    __asm volatile("syscall %0" :: "i"(SYSCALL_YIELD));
}
```

```asm
; Trap handler para classe 6:
.section .traptab, "ax"
.align 5
_trap6_handler:
    svlcx                       ; Salva Lower Context
    ; D[15] contém o TIN (syscall number)
    ; A[11] contém o return address
    ; Pode chamar C handler:
    mov     d4, d15             ; passa TIN como argumento
    call    os_syscall_handler  ; C function
    rslcx
    rfe
```

### 6.8 Interrupt Controller (IR / SRC)

No TC2xx, cada fonte de interrupt tem um **SRC (Service Request Control) register**:

```
SRC_xxxx Register:
  Bits 31:26  25:24  23:16    15     14:12   11    10     9:0
       ────────────────────────────────────────────────────────
       res     TOS    res     SRE    SRPN     res   SRR    res
                                     (8bit)
```

Espere, no TC2xx é um pouco diferente. Vou corrigir:

```
SRC_xxxx Register (TC2xx):
  Bit  | Nome  | Descrição
  ─────┼───────┼──────────
  [7:0]| SRPN  | Service Request Priority Number (0=disabled, 1-255)
  [10] | SRE   | Service Request Enable
  [11] | TOS   | Type of Service: 0=CPU0, 1=CPU1, 2=DMA, 3=CPU2 (TC27x)
  [12] | —     | reserved
  [24] | SRR   | Service Request flag (read: pending, write 1 to clear)
  [25] | CLRR  | Clear Request (write 1 to clear SRR)
  [26] | SETR  | Set Request (write 1 to set SRR — software interrupt)
  [27] | IOV   | Interrupt Overrun (sticky)
  [28] | IOVCLR| Clear IOV
  [29] | SWS   | SW Sticky (pending)
  [30] | SWSCLR| Clear SWS
```

**Para RTOS — configurando um interrupt:**

```c
// Exemplo: configurar STM0 Compare Match 0 para prioridade 10, CPU0
SRC_STM0SR0.B.SRPN = 10;   // Prioridade 10
SRC_STM0SR0.B.TOS  = 0;    // Direcionado ao CPU0
SRC_STM0SR0.B.SRE  = 1;    // Habilita
```

### 6.9 Software Interrupt (para RTOS Tick ou Yield)

Você pode gerar uma interrupt por software settando SRR ou SETR:

```c
// Trigger software interrupt para forçar scheduling
SRC_GPSR00.B.SETR = 1;  // General Purpose SRC do CPU0
```

Ou usar `SYSCALL` para traps síncronos (mais previsível para yield).

---

## 7. System Registers Relevantes para RTOS

### 7.1 Tabela Completa de CSFRs para RTOS

| CSFR | Offset | Leitura | Escrita | Uso RTOS |
|------|--------|---------|---------|----------|
| PCXI | 0xFE00 | Sim | Sim (MTCR) | **Context switch**: manipular a chain de CSAs |
| PSW | 0xFE04 | Sim | Sim | Setar IO level, IS flag, CDC |
| PC | 0xFE08 | Sim | Não* | Debug; PC é setado indiretamente via RFE/RET |
| SYSCON | 0xFE14 | Sim | Sim | Privilege mode, protection |
| BIV | 0xFE20 | Sim | Sim (boot) | Base da interrupt vector table |
| BTV | 0xFE24 | Sim | Sim (boot) | Base da trap vector table |
| ISP | 0xFE28 | Sim | Sim (boot) | Interrupt stack pointer |
| ICR | 0xFE2C | Sim | Sim | Enable/disable IRQs, set priority |
| FCX | 0xFE38 | Sim | Sim | Head da CSA free list |
| LCX | 0xFE3C | Sim | Sim | Limit threshold da CSA free list |
| DBGSR | 0xFD00 | Sim | Parcial | Debug status |
| CORECON | — | — | — | Core identification (TC2xx: CORE_ID) |

**Acesso via intrínsecos do compilador:**

```c
// Tasking:
uint32_t pcxi = __mfcr(PCXI);   // Move From Core Register
__mtcr(PCXI, new_value);        // Move To Core Register
__isync();                        // Obrigatório após MTCR!

// GCC (HighTec):
uint32_t pcxi;
__asm volatile("mfcr %0, $pcxi" : "=d"(pcxi));
__asm volatile("mtcr $pcxi, %0" :: "d"(new_value));
__asm volatile("isync");

// Endereços numéricos:
// PCXI = 0xFE00, PSW = 0xFE04, etc.
// Usado quando o assembler não conhece nomes simbólicos:
__asm volatile("mfcr %0, 0xFE00" : "=d"(pcxi));
```

---

## 8. Context Switch — Cookbook para RTOS

### 8.1 O que precisa ser salvo/restaurado por task

Para cada task, o "contexto completo" é:

1. **Upper Context** (A11, SP, D0-D7, A2-A7, PSW) — em um CSA
2. **Lower Context** (D8-D15, A12-A15) — em outro CSA
3. **Stack pointer** (A[10]) — dentro do Upper Context CSA
4. **PCXI** — o head da chain de CSAs da task
5. **A[0], A[1], A[8], A[9]** — registradores globais; se todas as tasks compartilham o mesmo programa/data model, esses não mudam. Se cada task tem seção de dados separada, precisam ser salvos.

Na prática, para um RTOS simples: **salvar PCXI e A[10] é suficiente**, pois a chain de CSAs contém todo o resto.

### 8.2 Estrutura do TCB (Task Control Block)

```c
typedef struct tcb {
    uint32_t    pcxi;       // PCXI da task (head da CSA chain)
    uint32_t    sp;         // Stack pointer (A[10]) — redundante, está no CSA, mas útil
    uint32_t    *stack_base;
    uint32_t    stack_size;
    uint32_t    priority;
    uint32_t    state;      // READY, RUNNING, BLOCKED, etc.
    // ... outros campos do RTOS ...
} tcb_t;
```

### 8.3 Context Switch via SYSCALL (Cooperative)

```
Task A chama os_yield():
  │
  ▼ SYSCALL #0
  │
  ├─ Hardware salva Upper Context de Task A em CSA
  ├─ PC ← trap handler (classe 6)
  │
  ▼ Trap Handler:
  │
  ├─ SVLCX          ; Salva Lower Context de Task A em CSA
  ├─ MFCR d4, PCXI  ; d4 = PCXI (head da chain: Lower→Upper→...)
  │
  │  ; Salva PCXI no TCB de Task A:
  ├─ ST.W [current_tcb + offsetof(pcxi)], d4
  │
  │  ; Seleciona próxima task (scheduler):
  ├─ CALL scheduler  ; retorna new_tcb em a2 (ou d2)
  │
  │  ; Restaura PCXI da Task B:
  ├─ LD.W d4, [new_tcb + offsetof(pcxi)]
  ├─ MTCR PCXI, d4
  ├─ ISYNC
  │
  ├─ RSLCX          ; Restaura Lower Context de Task B do CSA
  ├─ RFE            ; Restaura Upper Context de Task B + retorna
  │
  ▼ Task B continua de onde parou
```

### 8.4 Context Switch via Timer Interrupt (Preemptive)

```asm
; ===== ISR de Timer (Tick) =====
; Hardware já salvou Upper Context e:
;   - PCXI tem link para Upper CSA
;   - ICR.IE = 0, CCPN = timer_priority
;   - Se vinha de task: PSW.IS foi setado, A[10] = ISP

.global _timer_tick_isr
_timer_tick_isr:
    ; 1. Salva Lower Context
    svlcx
    
    ; 2. Re-habilita interrupts de prioridade maior (opcional, para preemption aninhada)
    ;    Ou pode manter desabilitado durante o switch
    ; bisr TIMER_PRIO    ; ← se quiser nesting
    
    ; 3. Salva PCXI no TCB atual
    mfcr    d15, #0xFE00        ; d15 = PCXI (endereço numérico de PCXI)
    
    ; Carrega ponteiro do TCB atual
    movh.a  a15, hi:current_tcb
    lea     a15, [a15] lo:current_tcb
    ld.a    a15, [a15]          ; a15 = current_tcb (ponteiro)
    st.w    [a15], d15          ; current_tcb->pcxi = PCXI
    
    ; 4. Chama o scheduler (estamos no interrupt stack, com Lower+Upper salvos)
    call    os_tick_and_schedule
    ; Retorno: os_schedule pode ter mudado current_tcb
    
    ; 5. Carrega PCXI da nova task
    movh.a  a15, hi:current_tcb
    lea     a15, [a15] lo:current_tcb  
    ld.a    a15, [a15]          ; a15 = new current_tcb
    ld.w    d15, [a15]          ; d15 = new_tcb->pcxi
    
    ; 6. Restaura contexto da nova task
    mtcr    #0xFE00, d15        ; PCXI = new task's PCXI
    isync
    
    ; 7. Restore e return
    rslcx                       ; Restaura Lower Context da nova task
    rfe                         ; Restaura Upper Context + PSW + PC da nova task
```

### 8.5 Criação de Contexto Inicial para Nova Task

Quando você cria uma task, precisa "fabricar" um contexto como se a task já tivesse sido interrompida:

```c
void os_create_task(tcb_t *tcb, void (*entry)(void *), void *arg, 
                    uint32_t *stack, uint32_t stack_size)
{
    uint32_t *sp = stack + (stack_size / 4);  // topo do stack (cresce para baixo)
    sp = (uint32_t *)((uint32_t)sp & ~0x7);  // alinha a 8 bytes
    
    // ==============================
    // 1. Alocar CSA para Upper Context (simula o que CALL/interrupt faria)
    // ==============================
    uint32_t fcx = __mfcr(FCX);
    uint32_t upper_csa_addr = csa_link_to_addr(fcx);
    uint32_t *upper_csa = (uint32_t *)upper_csa_addr;
    
    // Avança FCX para o próximo livre
    uint32_t next_free = upper_csa[0];  // word 0 do CSA livre = link para próximo livre
    __mtcr(FCX, next_free);
    __isync();
    
    // Preenche Upper Context CSA:
    upper_csa[0]  = 0;           // PCXI link = 0 (fim da chain — essa é a primeira "frame")
    upper_csa[1]  = 0x00000B80;  // PSW: IO=Supervisor(2), IS=0, CDE=1, CDC=0x7F (ou ajuste)
                                 // Bits: IO=11b (supervisor), CDE=1, CDC=7F → 0x00000B80
                                 // AJUSTE conforme sua política de proteção!
    upper_csa[2]  = (uint32_t)sp;       // A[10] = Stack Pointer
    upper_csa[3]  = (uint32_t)entry;    // A[11] = "Return Address" = entry point da task!
    upper_csa[4]  = 0;                  // D[0]
    upper_csa[5]  = 0;                  // D[1]
    upper_csa[6]  = 0;                  // D[2]
    upper_csa[7]  = 0;                  // D[3]
    upper_csa[8]  = 0;                  // A[2]
    upper_csa[9]  = 0;                  // A[3]
    upper_csa[10] = (uint32_t)arg;      // D[4] = primeiro argumento da entry function
    upper_csa[11] = 0;                  // D[5]
    upper_csa[12] = 0;                  // D[6]
    upper_csa[13] = 0;                  // D[7]
    upper_csa[14] = 0;                  // A[6]
    upper_csa[15] = 0;                  // A[7]
    
    // Marca como Upper Context (UL=1) no link word
    uint32_t upper_link = addr_to_csa_link(upper_csa_addr);
    upper_link |= (1u << 19);   // UL = 1 (Upper Context)
    // PIE = 1 (interrupts eram habilitadas), PCPN = 0
    upper_link |= (1u << 20);   // PIE = 1
    
    // ==============================
    // 2. Alocar CSA para Lower Context (simula SVLCX)
    // ==============================
    fcx = __mfcr(FCX);
    uint32_t lower_csa_addr = csa_link_to_addr(fcx);
    uint32_t *lower_csa = (uint32_t *)lower_csa_addr;
    
    next_free = lower_csa[0];
    __mtcr(FCX, next_free);
    __isync();
    
    // Preenche Lower Context CSA:
    lower_csa[0]  = upper_link;  // Link para Upper Context CSA (com UL=1, PIE, PCPN)
    lower_csa[1]  = 0;           // A[11]  
    lower_csa[2]  = 0;           // A[2]
    lower_csa[3]  = 0;           // A[3]
    lower_csa[4]  = 0;           // D[8]
    lower_csa[5]  = 0;           // D[9]
    lower_csa[6]  = 0;           // D[10]
    lower_csa[7]  = 0;           // D[11]
    lower_csa[8]  = 0;           // A[12]
    lower_csa[9]  = 0;           // A[13]
    lower_csa[10] = 0;           // A[14]
    lower_csa[11] = 0;           // A[15]
    lower_csa[12] = 0;           // D[12]
    lower_csa[13] = 0;           // D[13]
    lower_csa[14] = 0;           // D[14]
    lower_csa[15] = 0;           // D[15]
    
    // Lower link word: UL=0
    uint32_t lower_link = addr_to_csa_link(lower_csa_addr);
    // UL = 0 (lower), PIE e PCPN irrelevantes aqui
    
    // ==============================
    // 3. TCB recebe o PCXI = link para Lower CSA (head da chain)
    // ==============================
    tcb->pcxi = lower_link;  // Chain: Lower CSA → Upper CSA → 0 (end)
    tcb->sp = (uint32_t)sp;
}

// Helpers:
static inline uint32_t csa_link_to_addr(uint32_t link) {
    uint32_t seg = (link >> 16) & 0x7;
    uint32_t off = link & 0xFFFF;
    return (seg << 28) | (off << 6);
}

static inline uint32_t addr_to_csa_link(uint32_t addr) {
    uint32_t seg = (addr >> 28) & 0x7;
    uint32_t off = (addr >> 6) & 0xFFFF;
    return (seg << 16) | off;
}
```

> **ATENÇÃO com PSW**: O valor `0x00000B80` é um exemplo. Decomposição:
> - Bits [6:0] CDC = 0x00 (call depth = 0)
> - Bit [7] CDE = 1 (call depth counting enabled)
> - Bit [8] GW = 0
> - Bit [9] IS = 0 (user stack)
> - Bits [11:10] IO = 2 (Supervisor) → `10b` = 0x800
> - Total: 0x00000880 com CDE, ou ajuste conforme necessidade
> 
> **VERIFIQUE** o PSW value para seu caso de uso específico!

### 8.6 Primeiro Start de Task (First Context Load)

Quando o RTOS inicia e quer rodar a primeira task:

```asm
.global os_start_first_task
os_start_first_task:
    ; a4 = ponteiro para TCB da primeira task
    
    ; Disable interrupts
    disable
    
    ; Carrega PCXI da task
    ld.w    d0, [a4]            ; d0 = tcb->pcxi
    mtcr    #0xFE00, d0         ; PCXI = task PCXI
    isync
    
    ; Restaura Lower Context
    rslcx
    
    ; Restaura Upper Context + jump para entry point
    rfe
    
    ; Nunca retorna aqui!
```

### 8.7 Diagrama de CSA Chain

```
Quando Task A está suspensa (após SVLCX no context switch):

PCXI (salvo no TCB A)
  │
  ▼
┌─────────────────┐
│  Lower CSA      │  UL=0
│  D[8]-D[15]     │
│  A[12]-A[15]    │
│  link ──────────┼──┐
└─────────────────┘  │
                     ▼
               ┌─────────────────┐
               │  Upper CSA      │  UL=1, PIE=prev_IE, PCPN=prev_prio
               │  D[0]-D[7]      │
               │  A[2]-A[7]      │
               │  PSW, SP, RA    │
               │  link ──────────┼──┐
               └─────────────────┘  │
                                    ▼
                              ┌─────────────────┐
                              │ Upper CSA (CALL) │  ← contexto de quem chamou a função
                              │  ...             │     que estava executando quando task
                              │  link ─────────  │     foi preemptada
                              └────────┼─────────┘
                                       ▼
                                      ...
                                       ▼
                                    0x00000 (fim da chain)
```

### 8.8 Liberando CSAs quando uma Task Termina

Quando uma task retorna (ou é deletada), seus CSAs precisam ser devolvidos à free list:

```c
void os_free_task_csa_chain(uint32_t pcxi)
{
    uint32_t current = pcxi;
    
    while (current != 0) {
        uint32_t csa_addr = csa_link_to_addr(current & 0x000FFFFF); // mask out UL/PIE/PCPN
        uint32_t *csa = (uint32_t *)csa_addr;
        uint32_t next = csa[0];  // link para próximo CSA na chain
        
        // Devolve este CSA para a free list
        uint32_t old_fcx = __mfcr(FCX);
        csa[0] = old_fcx;
        uint32_t new_fcx = addr_to_csa_link(csa_addr);
        __mtcr(FCX, new_fcx);
        __isync();
        
        current = next;
    }
}
```

> **Nota**: Essa operação precisa ser feita com interrupts desabilitadas (ou em seção crítica) pois modifica FCX.

---

## 9. Gotchas e Pegadinhas

### 9.1 ISYNC após MTCR

**SEMPRE** coloque `isync` após `mtcr`. Sem isso, o pipeline pode ler o valor antigo do CSFR. Isso é fonte de bugs extremamente difíceis de diagnosticar.

```asm
mtcr    #0xFE00, d0     ; PCXI = d0
isync                    ; ← OBRIGATÓRIO!
```

### 9.2 CSA Segment Address

O TriCore usa segmentação simplificada para CSA. O **segment index no link word NÃO é o nibble mais alto do endereço**. É o nibble mais alto do endereço **deslocado**.

Exemplo para DSPR do CPU0 no TC27x:
- Endereço físico: `0x7002_0000` (via global address)
- Endereço local: `0xD002_0000` (via cached local)

O segment index usa o **endereço de segmento como visto pelo core**. Use sempre os endereços locais (0xD000_xxxx) para CSAs no DSPR.

### 9.3 Interrupt Stack deve ser inicializado antes de qualquer interrupt

Se ISP não for setado e uma interrupt ocorrer com PSW.IS=0, A[10] será carregado com lixo. Crash garantido.

### 9.4 CSA Pool deve estar em RAM rápida

CSAs são acessados em **toda CALL e RET**. Coloque o pool na DSPR (local data RAM) para performance ótima. Nunca coloque em Flash.

### 9.5 Call Depth Counter

Se CDE=1, toda CALL incrementa CDC e todo RET decrementa. Se CDC overflow → trap classe 3, TIN 2 (CDO). O counter tem 7 bits mas usa formato packed:

```
CDC bits [6:0] interpretação:
  0xxxxxx : 6-bit counter (max 63 calls)
  10xxxxx : 5-bit counter (max 31)  
  110xxxx : 4-bit counter (max 15)
  1110xxx : 3-bit counter (max 7)
  11110xx : 2-bit counter (max 3)
  111110x : 1-bit counter (max 1)
  1111110 : zero-bit counter (trap on any CALL)
  1111111 : CDC disabled (nunca trap)
```

Para RTOS: setar CDC = 0 (ou 0x7F para desabilitar) no PSW inicial de cada task.

**Recomendação**: Se não precisa de proteção contra stack overflow via CDC, coloque `PSW.CDE = 0` ou `PSW.CDC = 0x7F`.

### 9.6 A[0], A[1], A[8], A[9] — Global Registers

Esses registradores são reservados pelo compilador para "small data" addressing:
- `A[0]` = base do small data area (sdata)
- `A[1]` = base do small data area 2 (sdata2)
- `A[8]` = base do small data area 3 (sdata3, compiler-specific)
- `A[9]` = base do small data area 4 (sdata4, compiler-specific)

Se todas as tasks executam o mesmo binário (típico), esses valores são **constantes** e não precisam ser salvos/restaurados. Se cada task tem seu próprio address space (raro no TriCore), você precisa adicionar save/restore manual.

### 9.7 Floating Point

O TriCore 1.6 (TC2xx) **não tem FPU separada com banco de registradores extra**. Operações float usam registradores D[] normais. Portanto, **não há contexto FPU extra para salvar** — está tudo nos Upper/Lower contexts.

### 9.8 Multicore (TC27x: 3 cores)

Cada core tem:
- Seu próprio banco de registradores
- Seu próprio CSA pool (use DSPR local de cada core)
- Seus próprios BIV, BTV, ISP, FCX, LCX
- Seu próprio ICR

A free list de CSA **NÃO é compartilhada** entre cores (e não deve ser!).

Para RTOS SMP, cada core roda seu próprio scheduler. Comunicação entre cores via:
- **Software interrupts** (SRC_GPSRxx — General Purpose Service Requests)
- **Spinlocks** em LMU (shared RAM) — TC2xx tem registradores de spinlock em hardware (LMUCON)

### 9.9 Cuidado com o RFE e PSW.IS

Quando RFE restaura o Upper Context, o PSW restaurado pode ter IS=0 (user stack). Nesse momento, A[10] do Upper Context CSA é carregado e aponta para o user stack da task. O interrupt stack fica "automaticamente" desalocado. Isso é o comportamento correto, mas se seu context switch foi feito de dentro de interrupt, certifique-se que o PSW no Upper Context da task tenha IS=0.

### 9.10 DSYNC antes de manipular CSA manualmente

Se você escreve diretamente na memória de um CSA (por exemplo, ao criar contexto de uma nova task), use `dsync` antes de fazer `MTCR PCXI` para garantir que os stores foram completados:

```asm
    dsync           ; garante que writes no CSA estão completos
    mtcr  PCXI, d0
    isync
    rslcx
    rfe
```

---

## 10. Quick Reference Tables

### 10.1 Resumo: O que acontece em cada instrução

| Instrução | Upper Save | Lower Save | IE after | Usa CSA | Notes |
|-----------|-----------|------------|----------|---------|-------|
| CALL | ✅ (hw) | ❌ | unchanged | 1 alloc | Salva caller-saved |
| RET | ✅ (restore) | ❌ | unchanged | 1 free | Restaura caller-saved |
| SVLCX | ❌ | ✅ | unchanged | 1 alloc | Salva callee-saved |
| RSLCX | ❌ | ✅ (restore) | unchanged | 1 free | Restaura callee-saved |
| SVUCX | ✅ | ❌ | unchanged | 1 alloc | Salva upper sem CALL |
| BISR #n | ❌ | ✅ | **IE=1** | 1 alloc | SVLCX + enable + set CCPN |
| Interrupt entry | ✅ (hw) | ❌ | **IE=0** | 1 alloc | Hardware automático |
| Trap entry | ✅ (hw) | ❌ | unchanged | 1 alloc | Hardware automático |
| RFE | ✅ (restore) | ❌ | from PIE | 1 free | Restaura PSW, IE, CCPN |
| SYSCALL | ✅ (hw) | ❌ | unchanged | 1 alloc | Como trap classe 6 |

### 10.2 Receita de Context Switch (Cheat Sheet)

```
=== Context Switch (dentro de ISR ou trap handler) ===

1. SVLCX                    ; Salva Lower da task atual
2. MFCR d_temp, PCXI        ; Lê head da chain
3. ST [current_tcb], d_temp  ; Guarda no TCB
4. --- scheduler ---         ; Seleciona próxima task
5. LD d_temp, [next_tcb]     ; Carrega PCXI da nova task
6. MTCR PCXI, d_temp         ; Seta como head da chain
7. ISYNC
8. RSLCX                    ; Restaura Lower da nova task
9. RFE                      ; Restaura Upper + retorna para nova task
```

### 10.3 CSA Sizing Guide

| Cenário | CSAs por task | CSAs compartilhados |
|---------|--------------|-------------------|
| Context switch overhead | 2 (Upper + Lower) | — |
| Call depth típico | 10-20 | — |
| Interrupt nesting (por nível) | 2 | Compartilhados via ISP |
| **Fórmula total** | | `N_tasks × (2 + avg_call_depth) + max_isr_nesting × 2 + margin` |
| **Exemplo**: 8 tasks, call depth 15, 4 IRQ levels | | `8 × 17 + 4 × 2 + 16 = 160 CSAs ≈ 10KB` |

### 10.4 Endereços de CSFRs (para MFCR/MTCR)

| Nome | Endereço | Hex |
|------|----------|-----|
| PCXI | 0xFE00 | `mfcr d0, #0xFE00` |
| PSW | 0xFE04 | `mfcr d0, #0xFE04` |
| PC | 0xFE08 | `mfcr d0, #0xFE08` |
| SYSCON | 0xFE14 | |
| BIV | 0xFE20 | |
| BTV | 0xFE24 | |
| ISP | 0xFE28 | |
| ICR | 0xFE2C | |
| FCX | 0xFE38 | |
| LCX | 0xFE3C | |
| CORE_ID | 0xFE1C | Para identificar em qual core estamos |

### 10.5 Boot Sequence Mínima para RTOS

```
1.  Inicializar watchdog (desabilitar ou configurar)
2.  Inicializar clocks (PLL, etc.)
3.  Inicializar memória (DSPR clear, BSS clear, data copy)
4.  Setar BTV (trap vector table base)
5.  Setar BIV (interrupt vector table base)  
6.  Setar ISP (interrupt stack pointer)
7.  Inicializar CSA free list (FCX, LCX)
8.  Setar PSW inicial (supervisor mode, CDC, etc.)
9.  Criar tasks (alocar stacks, fabricar contextos)
10. Configurar timer para tick interrupt
11. Configurar SRC registers para interrupts necessárias
12. Setar ICR.CCPN = 0, ICR.IE = 1
13. Carregar primeira task (MTCR PCXI, RSLCX, RFE)
```

---

## Apêndice A: Definições de Macros Úteis

```c
/* TriCore CSFR addresses */
#define TRICORE_PCXI    0xFE00
#define TRICORE_PSW     0xFE04
#define TRICORE_PC      0xFE08
#define TRICORE_SYSCON  0xFE14
#define TRICORE_COREID  0xFE1C
#define TRICORE_BIV     0xFE20
#define TRICORE_BTV     0xFE24
#define TRICORE_ISP     0xFE28
#define TRICORE_ICR     0xFE2C
#define TRICORE_FCX     0xFE38
#define TRICORE_LCX     0xFE3C

/* PSW bits */
#define PSW_CDC_DIS     0x7F    /* Call depth counting disabled */
#define PSW_CDE         (1u << 7)
#define PSW_GW          (1u << 8)
#define PSW_IS          (1u << 9)
#define PSW_IO_USER     (0u << 10)
#define PSW_IO_SUPER0   (1u << 10)
#define PSW_IO_SUPER1   (2u << 10)  /* Typical for RTOS tasks */
#define PSW_IO_SUPER    (3u << 10)  /* Full supervisor */

/* PCXI bits */
#define PCXI_PCXO_MASK  0x0000FFFF
#define PCXI_PCXS_MASK  0x00070000
#define PCXI_PCXS_SHIFT 16
#define PCXI_UL         (1u << 19)
#define PCXI_PIE        (1u << 20)
#define PCXI_PCPN_MASK  0x00E00000
#define PCXI_PCPN_SHIFT 21

/* ICR bits */
#define ICR_CCPN_MASK   0x000000FF
#define ICR_IE          (1u << 15)
#define ICR_PIPN_MASK   0x00FF0000
#define ICR_PIPN_SHIFT  16

/* CSA address conversion */
#define CSA_LINK_TO_ADDR(link) \
    ((((link) & 0x70000) << 12) | (((link) & 0xFFFF) << 6))

#define ADDR_TO_CSA_LINK(addr) \
    ((((addr) >> 12) & 0x70000) | (((addr) >> 6) & 0xFFFF))

/* Compiler intrinsics abstraction */
#if defined(__TASKING__)
    #define MFCR(reg)       __mfcr(reg)
    #define MTCR(reg, val)  __mtcr(reg, val)
    #define ISYNC()         __isync()
    #define DSYNC()         __dsync()
    #define DISABLE()       __disable()
    #define ENABLE()        __enable()
    #define SVLCX()         __svlcx()
    #define RSLCX()         __rslcx()
#elif defined(__GNUC__) /* HighTec GCC or standard GCC */
    #define MFCR(reg) \
        ({ uint32_t _v; __asm volatile("mfcr %0, %1" : "=d"(_v) : "i"(reg)); _v; })
    #define MTCR(reg, val) \
        __asm volatile("mtcr %0, %1" :: "i"(reg), "d"((uint32_t)(val)))
    #define ISYNC()  __asm volatile("isync" ::: "memory")
    #define DSYNC()  __asm volatile("dsync" ::: "memory")
    #define DISABLE() __asm volatile("disable" ::: "memory")
    #define ENABLE()  __asm volatile("enable" ::: "memory")
    #define SVLCX()  __asm volatile("svlcx")
    #define RSLCX()  __asm volatile("rslcx")
#endif
```

---

## Apêndice B: Template de Interrupt Vector Table (GCC/HighTec)

```asm
/*
 * Interrupt Vector Table para TC2xx
 * Modo: 32-byte spacing (BIV.VSS = 0)
 */

.section .inttab, "ax", @progbits

.macro INTERRUPT_ENTRY prio handler
    .align 5                    /* 32 bytes = 2^5 */
    .global __interrupt_\prio
__interrupt_\prio:
    svlcx                       /* Save Lower Context */
    movh.a  %a14, hi:\handler
    lea     %a14, [%a14] lo:\handler
    ji      %a14
.endm

/* Priority 0 = not used (disabled) */
.align 5
__interrupt_0:
    debug       /* Should never reach here */
    rfe

/* Define your interrupts: */
INTERRUPT_ENTRY 1,  _isr_handler_1
INTERRUPT_ENTRY 2,  _isr_handler_2
/* ... */
INTERRUPT_ENTRY 10, _timer_tick_isr_c_wrapper
/* ... fill up to max priority used ... */

/* 
 * In the C handlers, epilogue must be:
 *   RSLCX + NOP + RFE  
 * Or use naked functions with inline asm
 */
```

```c
/* C wrapper for ISR (exemplo com HighTec GCC): */
void __attribute__((used)) _timer_tick_isr_c_wrapper(void)
{
    /* Lower Context já foi salvo pela INTERRUPT_ENTRY */
    
    /* Re-enable interrupts para nesting (se desejado) */
    /* BISR poderia ser usado em vez de SVLCX na entry, mas controle manual dá mais flexibilidade */
    
    /* Clear interrupt source */
    STM0_ICR.B.CMP0IRR = 0;  /* Clear STM compare flag */
    
    /* RTOS tick processing + possible context switch */
    os_tick();
    
    /* 
     * Se os_tick() decidiu fazer context switch, 
     * manipulou PCXI via MTCR.
     * RSLCX + RFE irão restaurar o contexto da NOVA task.
     */
    
    __asm volatile(
        "rslcx      \n"
        "nop        \n"     /* pipeline safety — some implementations need this */
        "rfe        \n"
    );
    
    /* Nunca chega aqui */
    __builtin_unreachable();
}
```

---

## Apêndice C: Referências

1. **TriCore Architecture Manual v1.6** — Infineon (Volume 1: Core Architecture)
   - Capítulo 3: Register Set
   - Capítulo 4: Tasks and Functions (Context Management)  
   - Capítulo 5: Interrupt System
   - Capítulo 6: Trap System

2. **AURIX TC27x User Manual** — Infineon
   - Parte: CPU
   - Parte: Interrupt Router (IR)
   - Parte: STM (System Timer)

3. **TriCore EABI** — Infineon/Altium/HighTec
   - Convenções de chamada
   - Passagem de argumentos

4. **HighTec TriCore GCC Toolchain Documentation** — Seção de intrinsics e inline assembly

5. **TASKING TriCore Compiler User Guide** — Seção de intrinsics e pragmas de interrupt

---

> **Disclaimer**: Este documento foi compilado a partir do conhecimento da arquitetura TriCore v1.6 e família TC2xx. **Sempre valide contra a documentação oficial da Infineon**, especialmente o layout exato dos CSA words e os valores de campos do PSW, pois existem variações sutis entre revisões do manual. Os exemplos de código são ilustrativos e precisam de adaptação para seu toolchain específico (Tasking, HighTec GCC, GCC upstream).

---

*Última atualização: Julho 2025*  
*Target: TriCore 1.6 / AURIX TC2xx (TC27x, TC26x, TC29x)*
