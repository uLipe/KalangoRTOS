/*
 * softirq_demo — KalangoRTOS softirq example
 *
 * Demonstrates the softirq subsystem: a producer task generates periodic
 * "sensor readings" and raises a software IRQ with each value.  The
 * registered handler (executed by the high-priority kSoftIrqD daemon)
 * accumulates the readings and reports the result once enough samples
 * have been collected.
 *
 * Two softirq vectors are used:
 *   VECTOR_SAMPLE  (0) — raised for every new sample; handler accumulates
 *   VECTOR_REPORT  (1) — raised when TARGET_SAMPLES have been collected;
 *                        handler prints the aggregate and exits
 *
 * This also shows that a softirq handler can itself trigger another
 * softirq (VECTOR_SAMPLE's handler raises VECTOR_REPORT on completion).
 */

#include <stdio.h>

#include <platform_qemu.h>
#include <KalangoRTOS/kalango_api.h>

#define VECTOR_SAMPLE        0U
#define VECTOR_REPORT        1U
#define TARGET_SAMPLES       5U
#define PRODUCER_PERIOD_MS  50U   /* ticks (1 tick = 1 ms at 1 kHz) */

typedef struct {
    uint32_t value;
} sample_t;

static volatile uint32_t sample_count;
static volatile uint32_t sample_sum;

static sample_t samples[TARGET_SAMPLES];

/* -------------------------------------------------------------------
 * VECTOR_SAMPLE handler — runs inside kSoftIrqD
 *
 * Accumulates the reading.  When TARGET_SAMPLES is reached it triggers
 * VECTOR_REPORT with the accumulated sum so that reporting is also
 * deferred through the softirq mechanism.
 * ------------------------------------------------------------------- */
static void sample_handler(void *data)
{
    const sample_t *s = (const sample_t *)data;

    sample_sum += s->value;
    sample_count++;

    printf("sample #%lu  value=%lu  running_sum=%lu\n",
           (unsigned long)sample_count,
           (unsigned long)s->value,
           (unsigned long)sample_sum);

    if (sample_count >= TARGET_SAMPLES) {
        static uint32_t final_sum;
        final_sum = sample_sum;
        Kalango_SoftIrqTrigger(VECTOR_REPORT, &final_sum);
    }
}

/* -------------------------------------------------------------------
 * VECTOR_REPORT handler — runs inside kSoftIrqD
 *
 * Prints the final aggregate and signals success.
 * ------------------------------------------------------------------- */
static void report_handler(void *data)
{
    uint32_t total = *(const uint32_t *)data;

    printf("softirq_demo: collected %lu samples, sum=%lu — PASS\n",
           (unsigned long)TARGET_SAMPLES,
           (unsigned long)total);

    platform_exit(0);
}

/* -------------------------------------------------------------------
 * Producer task — runs at normal priority, generates periodic samples
 *
 * Raises VECTOR_SAMPLE for each reading via Kalango_SoftIrqTrigger,
 * which issues SVC #1 → arch SW-IRQ → kSoftIrqD picks up the work.
 * ------------------------------------------------------------------- */
static void producer_task(void *arg)
{
    (void)arg;

    for (uint32_t i = 1U; i <= TARGET_SAMPLES; i++) {
        Kalango_Sleep(PRODUCER_PERIOD_MS);
        samples[i - 1U].value = i * 10U;
        Kalango_SoftIrqTrigger(VECTOR_SAMPLE, &samples[i - 1U]);
    }

    /* park the producer after all samples have been submitted */
    for (;;) {
        Kalango_Sleep(1000U);
    }
}

void Kalango_MainTask(void *arg)
{
    (void)arg;

    sample_count = 0U;
    sample_sum   = 0U;

    Kalango_SoftIrqRequest(VECTOR_SAMPLE, sample_handler);
    Kalango_SoftIrqRequest(VECTOR_REPORT, report_handler);

    TaskSettings prod = {
        .priority   = 4U,
        .stack_size = 512U,
        .function   = producer_task,
        .arg        = NULL,
    };
    Kalango_TaskCreate(&prod);

    printf("softirq_demo: starting\n");
}

int main(void)
{
    Kalango_CoreStart();
    platform_exit(1);
    return 0;
}
