#include "rt_measure.h"

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "app_config.h"
#include "load.h"
#include "logger.h"

#define RT_MEASURE_RING_SIZE          32U
#define RT_MEASURE_TASK_STACK_WORDS   384U
#if RT_MEASURE_LOW_PRIORITY_TEST
#define RT_MEASURE_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)
#else
#define RT_MEASURE_TASK_PRIORITY      (tskIDLE_PRIORITY + 4U)
#endif
#define RT_MEASURE_LOG_INTERVAL       100U
#define RT_MEASURE_DEADLINE_US        1000U

typedef struct
{
    uint32_t event_count;
    uint32_t missed_deadlines;
    uint32_t overflow_count;
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint32_t last_cycles;
    uint32_t last_jitter_cycles;
    uint64_t total_cycles;
} RtMeasureStats_t;

static TIM_HandleTypeDef *measure_timer;
static TaskHandle_t measure_task_handle;
static volatile uint32_t timestamp_ring[RT_MEASURE_RING_SIZE];
static volatile uint8_t ring_head;
static volatile uint8_t ring_tail;
static volatile uint32_t ring_overflow_count;

static void RtMeasure_Task(void *argument);
static void RtMeasure_EnableDwtCycleCounter(void);
static bool RtMeasure_PopTimestamp(uint32_t *timestamp);
static uint32_t RtMeasure_CyclesToUs(uint32_t cycles);
static void RtMeasure_UpdateStats(RtMeasureStats_t *stats, uint32_t latency_cycles);
static void RtMeasure_LogStats(const RtMeasureStats_t *stats, uint32_t latency_cycles);

bool RtMeasure_Init(TIM_HandleTypeDef *timer)
{
    measure_timer = timer;

    if (measure_timer == NULL)
    {
        return false;
    }

    BaseType_t created = xTaskCreate(RtMeasure_Task,
                                     "MeasurementTask",
                                     RT_MEASURE_TASK_STACK_WORDS,
                                     NULL,
                                     RT_MEASURE_TASK_PRIORITY,
                                     &measure_task_handle);

    return created == pdPASS;
}

void RtMeasure_TimerElapsedFromISR(TIM_HandleTypeDef *timer)
{
    if ((measure_task_handle == NULL) || (measure_timer == NULL) ||
        (timer->Instance != measure_timer->Instance))
    {
        return;
    }

    uint8_t next_head = (uint8_t)((ring_head + 1U) % RT_MEASURE_RING_SIZE);

    if (next_head == ring_tail)
    {
        ring_overflow_count++;
    }
    else
    {
        timestamp_ring[ring_head] = DWT->CYCCNT;
        ring_head = next_head;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(measure_task_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void RtMeasure_Task(void *argument)
{
    (void)argument;

    RtMeasureStats_t stats = {
        .min_cycles = UINT32_MAX,
    };

    RtMeasure_EnableDwtCycleCounter();

    Logger_Write("MeasurementTask started: TIM2 ISR to task latency enabled\r\n");

#if RT_MEASURE_LOG_CSV
    Logger_Write("event,latency_us,min_us,avg_us,max_us,jitter_us,missed,overflows,load_level\r\n");
#else
    Logger_Write("measurement_csv,disabled\r\n");
#endif

    if (HAL_TIM_Base_Start_IT(measure_timer) != HAL_OK)
    {
        Logger_Write("ERROR: TIM2 interrupt start failed\r\n");
        vTaskDelete(NULL);
    }

    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint32_t isr_timestamp;

        while (RtMeasure_PopTimestamp(&isr_timestamp))
        {
            uint32_t task_timestamp = DWT->CYCCNT;
            uint32_t latency_cycles = task_timestamp - isr_timestamp;

            RtMeasure_UpdateStats(&stats, latency_cycles);

            if ((RT_MEASURE_LOG_CSV != 0U) &&
                ((stats.event_count % RT_MEASURE_LOG_INTERVAL) == 0U))
            {
                RtMeasure_LogStats(&stats, latency_cycles);
            }
        }
    }
}

static void RtMeasure_EnableDwtCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static bool RtMeasure_PopTimestamp(uint32_t *timestamp)
{
    bool has_sample = false;

    taskENTER_CRITICAL();
    if (ring_tail != ring_head)
    {
        *timestamp = timestamp_ring[ring_tail];
        ring_tail = (uint8_t)((ring_tail + 1U) % RT_MEASURE_RING_SIZE);
        has_sample = true;
    }
    taskEXIT_CRITICAL();

    return has_sample;
}

static uint32_t RtMeasure_CyclesToUs(uint32_t cycles)
{
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    if (cycles_per_us == 0U)
    {
        return 0U;
    }

    return cycles / cycles_per_us;
}

static void RtMeasure_UpdateStats(RtMeasureStats_t *stats, uint32_t latency_cycles)
{
    stats->event_count++;
    stats->total_cycles += latency_cycles;

    if (latency_cycles < stats->min_cycles)
    {
        stats->min_cycles = latency_cycles;
    }

    if (latency_cycles > stats->max_cycles)
    {
        stats->max_cycles = latency_cycles;
    }

    if (stats->event_count > 1U)
    {
        if (latency_cycles > stats->last_cycles)
        {
            stats->last_jitter_cycles = latency_cycles - stats->last_cycles;
        }
        else
        {
            stats->last_jitter_cycles = stats->last_cycles - latency_cycles;
        }
    }

    stats->last_cycles = latency_cycles;

    if (RtMeasure_CyclesToUs(latency_cycles) > RT_MEASURE_DEADLINE_US)
    {
        stats->missed_deadlines++;
    }

    stats->overflow_count = ring_overflow_count;
}

static void RtMeasure_LogStats(const RtMeasureStats_t *stats, uint32_t latency_cycles)
{
    uint32_t avg_cycles = (uint32_t)(stats->total_cycles / stats->event_count);

    Logger_Printf("%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
                  stats->event_count,
                  RtMeasure_CyclesToUs(latency_cycles),
                  RtMeasure_CyclesToUs(stats->min_cycles),
                  RtMeasure_CyclesToUs(avg_cycles),
                  RtMeasure_CyclesToUs(stats->max_cycles),
                  RtMeasure_CyclesToUs(stats->last_jitter_cycles),
                  stats->missed_deadlines,
                  stats->overflow_count,
                  Load_GetLevel());
}
