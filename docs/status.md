# ulmk — Status de Implementação e Roadmap

> Documento gerado em 29/06/2026. Descreve o estado atual do kernel,
> o que foi implementado na última sessão, e os próximos itens do roadmap.

---

## 1. Visão Geral da Arquitetura

O **ulmk** é um microkernel de propósito educacional/experimental
para a arquitetura **TriCore TC2xx (AURIX)**, inspirado no seL4 e no QNX.
O modelo de isolamento é **MPU estática** — um único ELF, domínios separados
por faixas DPR/CPR, sem processos separados.

### Filosofia de design
- O kernel é **provedor de mecanismo**, não de política.
- Uma única thread privilegiada (`ulmk_root_thread`) recebe todas as
  capabilities iniciais e constrói o restante do sistema.
- Código de kernel mínimo: scheduler, IPC síncrono, timer, MPU, IRQ routing.
- Otimizado primeiro para **tamanho**, depois para velocidade.

### Plataforma alvo
| Item | Valor |
|------|-------|
| ISA | TriCore 1.6.1 (TC275/TC277/TC297) |
| Toolchain | `tricore-elf-gcc` (HighTec GCC 13) |
| Simulador CI | QEMU Linumiz TC277 fork |
| Memória flash | PFlash `0xA0000000` (cached) |
| SRAM kernel | DSPR Core0 `0x70000000`, 240 KB |
| SRAM compartilhada | LMU `0x90000000`, 32 KB |
| Pool CSA | 16 KB (256 frames × 64 bytes) |

---

## 2. Estado Atual da Implementação

### 2.1 Boot e Startup (`arch/tricore/startup.S`, `kernel/kernel_main.c`)

**Sequência completa implementada:**

```
_start (startup.S)
 │ watchdog disable
 │ CSA pool init: FCX ← pool_end−64, LCX ← pool_start (link words)
 │ BTV ← trap vector table base (256-byte aligned)
 │ BIV ← interrupt vector table base (256-byte aligned)
 │ ISP ← interrupt stack top
 │ .bss zero + .data copy
 │ ulmk_board_init() [weak no-op; override para PLL/flash wait states]
 ▼
ulmk_arch_init()
 │ MPU init (PRS 0 = kernel full access)
 │ STM0 timer init (um-shot, rearmado pelo kernel)
 │ IRQ table init
 ▼
ulmk_kern_main()
 │ scheduler init
 │ phys allocator init (pool .user_pool)
 │ thread pool init
 │ cria ulmk_root_thread (privilege = ULMK_PRIV_DRIVER, ULMK_CAP_ALL)
 │ ulmk_sched_start(idle_ctx, root_thread) → primeiro context switch
 ▼
ulmk_root_thread(boot_info)   ← contexto userspace, IO=1
```

**`ulmk_boot_info_t`** contém: regiões de memória física disponíveis,
frequência do tick, base e tamanho do pool CSA.

### 2.2 Context Switch (`arch/tricore/ctx_switch.S`, `arch/tricore/vectors.S`)

**Cooperativo (syscall YIELD):**

```asm
; _trap_class6 (syscall):
;   hardware salva Upper Context (UL) → novo CSA
;   call ulmk_arch_syscall_entry → salva mais UL
;   call ulmk_sched_schedule → ...
;   call ulmk_arch_ctx_switch(from, to):

ulmk_arch_ctx_switch:
    disable               ; ← fecha janela de atomicidade
    svlcx                 ; aloca CSA Lower Context, PCXI = LL → UL_call → UL_syscall → ...
    mfcr  d15, 0xFE00     ; lê PCXI atual
    st.w  [a4], d15       ; from->ctx.pcxi = PCXI (salva chain inteira)
    ld.w  d15, [a5]       ; d15 = to->ctx.pcxi
    dsync
    mtcr  0xFE00, d15     ; instala PCXI da nova thread
    isync
    enable                ; ← reabre IRQs
    rslcx                 ; restaura Lower Context da nova thread
    rfe                   ; restaura Upper Context + PSW + PC → retorna ao ponto de interrupção
```

**Preemptivo (timer ISR `_arch_tick_preempt_isr`, SRPN=1):**

```
hardware salva UL do thread interrompido
_arch_tick_preempt_isr:
    svlcx                 ; salva LL do thread interrompido
    call  _arch_tick_isr_handler   ; ulmk_kern_tick() → ulmk_sched_tick()
    ; se g_preempt_new_ctx != NULL:
    mfcr  d0, 0xFE00      ; salva PCXI (LL→UL do interrompido) em old_ctx->pcxi
    ld.w  d0, [new_ctx]   ; carrega PCXI da nova thread
    dsync / mtcr / isync
    rslcx + rfe
```

**Invariantes da CSA chain por thread em repouso:**
- Switch cooperativo: `LL_svlcx → UL_call_ctx_switch → UL_call_sched → UL_call_syscall_entry → UL_syscall → chain anterior`
- Switch preemptivo: `LL_svlcx_isr → UL_hw_isr → chain anterior`
- Nenhum dos caminhos tem vazamento de CSA (confirmado por stress test).

### 2.3 Scheduler (`kernel/sched/sched.c`, `kernel/sched/fifo_rt.c`)

**Política:** RT-FIFO com round-robin por prioridade.
- Prioridade numérica menor = maior urgência (0 = mais urgente).
- `pick_next` O(1): pega o head da fila.
- `enqueue` O(n): insere na cauda do grupo de mesma prioridade.

**Preempção por quantum:**
- `ULMK_CONFIG_SCHED_QUANTUM_TICKS` (padrão: 10 ticks = 10 ms com TICK_HZ=1000).
- A cada tick o ISR chama `ulmk_sched_tick()`, que decrementa `ticks_remaining`
  e seta `g_preempt_old_ctx`/`g_preempt_new_ctx` quando o quantum expira ou
  uma thread de maior prioridade ficou pronta.
- O stub `_arch_tick_preempt_isr` lê esses ponteiros e troca contexto
  **sem chamar `ulmk_sched_schedule`** — evita empilhar CSA extra dentro do ISR.

**Atomicidade do context switch:**
- `ulmk_sched_schedule` desabilita IRQs antes de `sched_current = next` e
  da chamada a `ulmk_arch_ctx_switch`.
- Ao retornar para a thread re-escalada, IRQs são reativados com `ENABLE`
  (instrução TriCore, acessível em IO≥1, sem `MTCR ICR` que exige IO≥2).

### 2.4 Timer e Sleep Queue (`kernel/timer/timer.c`)

**Modelo híbrido:** periódico para quantum + tickless para sleeps.

```c
arm_nearest(now):
    deadline = now + TICK_PERIOD_US   // próximo quantum
    if (sleep_head && sleep_head->sleep_until < deadline)
        deadline = sleep_head->sleep_until  // acordar mais cedo se necessário
    ulmk_arch_tick_deadline(delta)
```

- `ulmk_timer_sleep_insert` insere na fila ordenada por deadline.
- `ulmk_timer_tick()` acorda threads vencidas, rearma o timer. **NÃO** chama
  `ulmk_sched_schedule` (para evitar corrupção de CSA dentro do ISR de timer).
- Idle loop (`ulmk_arch_cpu_idle`) detecta fila não vazia e chama
  `ulmk_sched_schedule` a partir de contexto limpo.

### 2.5 IPC Síncrono (`kernel/ipc/ep.c`)

**Modelo:** endpoint rendezvous — `call` bloqueia caller, acorda server;
`reply` desbloqueia caller com resultado.

```
ulmk_ep_call(ep, msg)
  → caller: state=BLOCKED, enfileirado em ep->send_queue
  → server:  acordado (se bloqueado em recv), herda prioridade do caller
  → servidor processa, ulmk_ep_reply(sender, reply_msg)
  → caller: desbloqueia, lê reply

ulmk_ep_recv_or_notif(ep, notif, mask, msg, result)
  → espera por mensagem IPC *ou* notificação de hardware
```

- Payload inline até `ULMK_MSG_WORDS=6` words (24 bytes).
- Campos do resultado (`ulmk_msg_t`, `sender TID`, `notif_bits`) salvos no TCB
  antes do block para sobreviver ao RSLCX/RFE do context switch.

### 2.6 Notificações (`kernel/notif/notif.c`)

Objeto de notificação: bitmask de 32 bits, acordado quando máscara de espera
∩ bits recebidos ≠ 0. Usado pelo driver IRQ para sinalizar threads de driver.

### 2.7 MPU e Domínios de Memória

**TriCore DPR/CPR + PRS (Protection Register Sets):**

| PRS | Usuário |
|-----|---------|
| 0 | Kernel — acesso total (IO=2) |
| 1 | Driver A (IO=1) |
| 2 | Driver B (IO=1) |
| 3 | App/overflow |

- DPR: sem restrição de power-of-2, alinhado a 8 bytes.
- Troca de PRS (`PSW.PRS`) ≈ 3 ciclos (vs. ~50 no ARM Cortex-M).
- Cada thread tem `ulmk_arch_region_t regions[ULMK_ARCH_MAX_REGIONS]` no TCB.
- `ulmk_arch_mpu_switch(regions, count, prs)` reconfigura CPR/DPR + PSW.PRS
  a cada context switch.

**Geração do linker script (3 camadas):**
1. `linker/kernel/*.ld.in` — seções arch-independentes (vectors, kernel_text, stacks).
2. `arch/tricore/linker/*.ld.in` — CSA pool, small-data ABI, OUTPUT_FORMAT.
3. `<chip_dir>/memory.ld` — MEMORY block, `ULMK_MPU_ALIGN`, flags HAVE_CSA/BMHD.

### 2.8 Layer de Atomics (`arch/tricore/arch.c`)

```c
/* CAS por software via DISABLE/ENABLE */
uint32_t ulmk_arch_atomic_cas(volatile uint32_t *ptr,
                             uint32_t expected, uint32_t desired) {
    uint32_t old;
    __asm__ __volatile__("disable" ::: "memory");
    old = *ptr;
    if (old == expected) *ptr = desired;
    __asm__ __volatile__("enable"  ::: "memory");
    return old;
}

/* atomic_add: CAS-retry loop */
uint32_t ulmk_arch_atomic_add(volatile uint32_t *ptr, uint32_t val) {
    uint32_t old, new_val;
    do {
        old     = *ptr;
        new_val = old + val;
    } while (ulmk_arch_atomic_cas(ptr, old, new_val) != old);
    return old;
}
```

**Por que não `CMPSWAP.W`:**
- O fork QEMU Linumiz TC277 trata a instrução como NOP (não emulada).
- `MFCR`/`MTCR` em ICR (0xFE2C) requerem IO≥2 e trapeiam de threads driver.
- `DISABLE`/`ENABLE` acessíveis em IO≥1 → equivalentes em single-core.
- TODO: usar `CMPSWAP.W` em targets multi-core reais (TC275/TC297 com
  múltiplos CPUs compartilhando SRAM global).

---

## 3. Última Sessão — O que foi endereçado

### 3.1 Bug: Não-atomicidade do context switch

**Problema:** entre `sched_current = next` em `ulmk_sched_schedule` e o
`mtcr 0xFE00, d15` (instalação do PCXI) em `ulmk_arch_ctx_switch`, o timer ISR
poderia ser entregue. O ISR via `_arch_tick_preempt_isr` leria `sched_current`
(já apontando para `next`) e salvaria `PCXI` (ainda da thread anterior) em
`next->ctx.pcxi`, corrompendo a CSA chain.

**Sintoma observado:** context switch stress test (8 workers × 100 yields)
causava travamento do timer — o supervisor nunca acordava do `ulmk_usleep(60s)`.

**Correção aplicada:**

*`kernel/sched/sched.c`:*
```c
key = ulmk_arch_cpu_irq_save();   // disable antes de atualizar sched_current
if (next) {
    sched_current = next;
    ...
}
ulmk_arch_ctx_switch(from, to);   // IRQs ainda desabilitados ao entrar
/* ao retornar (re-scheduling desta thread): */
ulmk_arch_cpu_irq_enable();       // enable incondicional (ENABLE, IO >= 1)
```

*`arch/tricore/ctx_switch.S`:*
```asm
ulmk_arch_ctx_switch:
    disable           ; ← bloqueia ISR durante troca de PCXI
    svlcx
    mfcr  d15, 0xFE00
    st.w  [a4], d15
    ld.w  d15, [a5]
    dsync
    mtcr  0xFE00, d15
    isync
    enable            ; ← janela fechada, ISR pode chegar agora
    rslcx
    rfe
```

### 3.2 Bug: Implementação do layer de atomics

**Problema:** `ulmk_arch_atomic_cas` e `ulmk_arch_atomic_add` eram stubs que
retornavam 0. Qualquer uso de contadores compartilhados entre threads
produzia resultados incorretos.

**Correção:** implementação por software via `DISABLE`/`ENABLE` (descrita
na seção 2.8 acima).

### 3.3 Teste de integração `tests/atomic_integ`

Cobre os dois bugs acima num único harness:

- **Phase 1 — Correção atômica:** 2 workers × 50 000 `ulmk_arch_atomic_add`
  sob preempção → `counter` deve ser exatamente 100 000.
- **Phase 2 — Stress de context switch:** 8 workers × 100 `ulmk_thread_yield`
  concorrentes enquanto o timer dispara a cada quantum → nenhum trap de
  gerenciamento de contexto (classe 3) deve ocorrer; todos os workers
  devem completar.

### 3.4 Correção de regressão nos unit tests

Os stubs de host (`ulmk_arch.h`, `ul/microkernel.h`) usados pelos testes
unitários de host (compilados com `cc`) estavam desatualizados em relação
a adições de sessões anteriores:

| Adição faltante | Motivo |
|-----------------|--------|
| `ulmk_arch_irq_key_t`, `irq_save/restore/enable` | `sched.c` passou a usar após fix de atomicidade |
| `ulmk_arch_region_t`, `ULMK_ARCH_MAX_REGIONS`, `ULMK_REGION_STACK` | `thread.c` usa desde a adição de MPU domains |
| `ulmk_msg_t`, `ulmk_recv_or_notif_result_t` | `ulmk_thread_internal.h` inclui desde adição do IPC |
| `ULMK_EP_INVALID`, `ULMK_NOTIF_INVALID`, `ULMK_CAP_ALL`, `UL_PERM_*` | `thread.c` usa nas inicializações do TCB |

Dois testes de comportamento do `sleep_unit` foram atualizados:
- `idle_triggers_schedule`: `ulmk_timer_tick` não chama mais `ulmk_sched_schedule`
  (evita empilhar CSA extra dentro do ISR); o idle loop faz isso em contexto limpo.
- `empty_queue_no_rearm`: `arm_nearest` sempre rearma para o próximo quantum,
  mesmo com a fila vazia — necessário para o preemptive scheduler continuar.

---

## 4. Suite de Testes — Estado Atual

Todos os 18 testes passando:

| Teste | Tipo | O que verifica |
|-------|------|----------------|
| `boot` | integ | boot sequence, BSS zero, root thread |
| `ctx_switch` | integ | fabricação e troca de contexto manual |
| `ctx_switch` stress | integ | 56 ciclos de spawn/exit/preempt |
| `sleep_integ` | integ | `ulmk_usleep`, wake-up ordering, supervisor |
| `preempt_integ` | integ | round-robin preemptivo, quantum |
| `thread_lifecycle_integ` | integ | spawn, kill, suspend/resume |
| `sched_integ` | integ | prioridade estrita, FIFO intra-prioridade |
| `resource_leak_integ` | integ | 20 ondas de spawn/exit sem CSA/stack leak |
| `ipc_integ` | integ | call/reply, recv_or_notif, priority inheritance |
| `mem_integ` | integ | MPU fault (classe 1), domínios de memória |
| `irq_integ` | integ | IRQ binding, notif, re-enable |
| `driver_integ` | integ | driver STM0 em userspace (IO=1) |
| `driver_integ2` | integ | driver STM1, multi-driver ordering |
| `asclin_integ` | integ | UART driver ASCLIN0 em userspace |
| `csa_ctx` | integ | fabricação manual de CSA chain |
| `atomic_integ` | integ | atomics correctness + ctx-switch stress |
| `thread_unit` | unit (host) | API de thread: init, spawn, kill, exit |
| `sched_unit` | unit (host) | scheduler: enqueue, dequeue, pick_next |
| `sleep_unit` | unit (host) | sleep queue: insert, remove, tick, rearm |

---

## 5. Roadmap — Próximos Itens

### 5.1 [ALTA PRIORIDADE] IPC: priority inheritance completo

**O que falta:**
- O servidor deve herdar a prioridade do cliente mais urgente em sua fila
  de envio enquanto processa a mensagem.
- Ao responder (`ulmk_ep_reply`), reverter a prioridade herdada.
- Implementar `ulmk_ep_set_priority_ceiling` para endpoints com ceiling fixo.

**Nível de implementação:**
1. Adicionar `uint8_t base_priority` no TCB (prioridade original).
2. Em `ulmk_ep_enqueue_sender`: se `sender->priority < server->priority`,
   chamar `ulmk_sched_dequeue(server)`, atualizar `server->priority`,
   `ulmk_sched_enqueue(server)`.
3. Em `ulmk_ep_reply`: restaurar `server->priority = server->base_priority`,
   re-enqueue.
4. Integração test: um cliente de prioridade alta + servidor de baixa prioridade
   → verificar que o servidor não é preterido por threads de prioridade média
   durante o processamento.

### 5.2 [ALTA PRIORIDADE] Suporte multi-core (SMP)

**O que falta:**
O kernel atual é completamente single-core. TC275/TC277 têm 3 cores independentes.

**Nível de implementação:**
1. **CSA pools separados por core:** cada core tem seu próprio FCX/LCX.
   Nunca compartilhar CSA entre cores (regra inviolável).
2. **Spinlocks para estruturas globais:** thread pool, endpoint queues,
   phys allocator precisam de proteção inter-core.
3. **IPI (Inter-Processor Interrupt):** usar SRC para enviar sinal de
   "reschedule" ao core alvo quando uma thread de alta prioridade é
   desbloqueada em outro core.
4. **Scheduler per-core:** cada core tem sua run queue; o global scheduler
   faz load-balancing na hora de `ulmk_thread_create` e durante steal.
5. **`ulmk_arch_atomic_cas` para SMP:** substituir `DISABLE`/`ENABLE` por
   `CMPSWAP.W` (instrução TriCore de CAS real, sincronizada via bus locking).

### 5.3 [MÉDIA PRIORIDADE] Capabilities granulares e revogação

**O que falta:**
- Sistema de capability atual: bitmask simples (`cap_flags`) no TCB.
- Falta: árvore de derivação de capabilities, revogação em cascata.

**Nível de implementação:**
1. Substituir bitmask por `cap_slot_t` — índice numa tabela global de slots.
2. Cada slot aponta para um objeto kernel (endpoint, notif, thread) + permissões.
3. `ulmk_cap_derive(src_slot, dst_tid, perms)`: cria filho com subset de permissões.
4. `ulmk_cap_revoke(slot)`: invalida o slot e todos os filhos recursivamente.
5. Necessário para isolamento seguro entre drivers de terceiros.

### 5.4 [MÉDIA PRIORIDADE] Gerenciamento de memória física dinâmico

**O que falta:**
- `ulmk_phys_alloc` atual: first-fit simples, sem retorno da memória ao sistema.
- Falta: capability de memória física (`UL_CAP_MEMORY`) — o root thread
  recebe toda a RAM livre e pode delegar regiões a outros componentes.

**Nível de implementação:**
1. `ulmk_kmem_grant(tid, base, size)`: concede uma faixa de RAM a uma thread.
2. Thread receptora pode usar a faixa para suas stacks, buffers IPC etc.
3. Integração com o linker/MPU: regiões concedidas entram no `regions[]` do TCB.

### 5.5 [MÉDIA PRIORIDADE] Watchdog e health monitoring

**O que falta:**
- TC27x tem WDT por core (WDTCON). Atualmente desabilitado no boot.
- Falta: o MC Supervisor alimenta o watchdog periodicamente; se uma thread
  trava, o WDT expira e reinicia o sistema de forma controlada.

**Nível de implementação:**
1. `ulmk_wdt_init(timeout_ms)` — configura WDT0/WDT1/WDT2 para todos os cores.
2. Thread dedicada de health monitor, prioridade máxima, faz `ulmk_wdt_kick()`
   a cada ciclo, verificando que todas as threads críticas reportaram progresso.
3. Integração com `ulmk_thread_suspend`/`ulmk_kern_exit` para detectar travamentos.

### 5.6 [BAIXA PRIORIDADE] Boot loader e BMHD

**O que falta:**
- `boards/tc27x/bmhd.ld.in` e geração do Boot Mode Header para flash real.
- Suporte a `_start` em non-cached flash (`0x80000000`).
- Implementar `ulmk_board_init` para TC277 EVB: PLL a 200 MHz, EMEM externa.

### 5.7 [BAIXA PRIORIDADE] Tracing e profiling

**O que falta:**
- Sistema de trace ring-buffer para context switches, syscalls, IRQs.
- Exportar via UART (ASCLIN) ou memória compartilhada para análise offline.
- Contadores de performance: CSA usage peak, scheduler latency, IRQ latency.

---

## 6. Débitos Técnicos Conhecidos

| Item | Detalhe |
|------|---------|
| `CMPSWAP.W` | Atomics usam `DISABLE/ENABLE`; em TC27x multi-core real, usar instrução hardware |
| `ulmk_arch_cpu_idle` | Implementado como `nop` (busy-wait); usar `WAIT` com wakeup por STM |
| `ulmk_ep_reply` sem priority inheritance | Servidor não reverte prioridade herdada ao responder |
| `ulmk_arch_phys_alloc` (arch) | Stubs em arch.c; a implementação real está em `phys_alloc.c` diretamente |
| CDC/CDE workaround | PSW CDC limpo manualmente no tick ISR para contornar NEST trap do QEMU |
| Timeout do `atomic_integ` | 60 s de sleep necessários; QEMU é ~30× mais lento que HW real |
