#include "load.h"

#include "FreeRTOS.h"
#include "task.h"
#include "app_config.h"
#include "logger.h"

#define LOAD_TASK_STACK_WORDS 128U
#if RT_MEASURE_LOW_PRIORITY_TEST
#define LOAD_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U)
#define LOAD_TASK_STARTUP_LOG "LoadTask started: same-priority CPU load test enabled\r\n"
#else
#define LOAD_TASK_PRIORITY    tskIDLE_PRIORITY
#define LOAD_TASK_STARTUP_LOG "LoadTask started: safe low-priority CPU load enabled\r\n"
#endif
#define LOAD_WINDOW_MS        10U
#define LOAD_PHASE_MS         3000U

static TaskHandle_t load_task_handle;
static volatile uint32_t load_level;

static void Load_Task(void *argument);
static void Load_EnableDwtCycleCounter(void);
static void Load_BusyWaitMs(uint32_t duration_ms);

bool Load_Init(void)
{
    BaseType_t created = xTaskCreate(Load_Task,
                                     "LoadTask",
                                     LOAD_TASK_STACK_WORDS,
                                     NULL,
                                     LOAD_TASK_PRIORITY,
                                     &load_task_handle);

    return created == pdPASS;
}

uint32_t Load_GetLevel(void)
{
    return load_level;
}

static void Load_Task(void *argument)
{
    (void)argument;

    static const uint32_t levels[] = {0U, 10U, 25U, 50U};
    uint32_t level_index = 0U;
    TickType_t phase_start = xTaskGetTickCount();

    Load_EnableDwtCycleCounter();
    load_level = levels[level_index];

    Logger_Write(LOAD_TASK_STARTUP_LOG);
    Logger_Printf("load_level,%lu\r\n", load_level);

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        if ((now - phase_start) >= pdMS_TO_TICKS(LOAD_PHASE_MS))
        {
            phase_start = now;
            level_index = (level_index + 1U) %
                          (sizeof(levels) / sizeof(levels[0]));
            load_level = levels[level_index];
            Logger_Printf("load_level,%lu\r\n", load_level);
        }

        uint32_t busy_ms = (LOAD_WINDOW_MS * load_level) / 100U;
        uint32_t idle_ms = LOAD_WINDOW_MS - busy_ms;

        Load_BusyWaitMs(busy_ms);

        if (idle_ms > 0U)
        {
            vTaskDelay(pdMS_TO_TICKS(idle_ms));
        }
        else
        {
            taskYIELD();
        }
    }
}

static void Load_EnableDwtCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void Load_BusyWaitMs(uint32_t duration_ms)
{
    if (duration_ms == 0U)
    {
        return;
    }

    uint32_t cycles_per_ms = SystemCoreClock / 1000U;
    uint32_t target_cycles = cycles_per_ms * duration_ms;
    uint32_t start_cycles = DWT->CYCCNT;

    while ((DWT->CYCCNT - start_cycles) < target_cycles)
    {
        __NOP();
    }
}
