/* ======================== 头文件与配置 ======================== */
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"          // 假设包含硬件初始化函数
#include "iwdg.h"          // 硬件看门狗驱动头文件

/* 软件看门狗监控的任务数量（可配置） */
#define WATCHDOG_TASK_NUM     5

/* 超时阈值（单位：tick，假设系统节拍为1000Hz，则1000 tick = 1秒） */
#define TASK_TIMEOUT_TICKS    (pdMS_TO_TICKS(3000))   // 3秒未更新则判定超时

/* 任务心跳结构体 */
typedef struct {
    TaskHandle_t task_handle;      // 被监控的任务句柄
    const char *task_name;         // 任务名（用于调试）
    TickType_t last_tick;          // 上次上报心跳的时间
    uint8_t active;                // 是否启用监控
} WatchdogTask_t;

/* 软件看门狗任务句柄 */
static TaskHandle_t xSoftWatchdogTask = NULL;

/* 被监控任务列表（需在初始化时填充） */
static WatchdogTask_t s_watchdog_tasks[WATCHDOG_TASK_NUM];

/* ======================== 硬件看门狗初始化 ======================== */
/* 以STM32的IWDG为例，超时时间约2秒（根据实际时钟和分频计算） */
void HardwareWatchdog_Init(void) {
    // 使能 LSI 时钟（若未使能）
    // 配置 IWDG：预分频器、重装载值（根据LSI频率计算超时）
    // 以下为伪代码，需根据实际库函数实现
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_64);      // 预分频64
    IWDG_SetReload(1250);                      // 假设LSI=40kHz，超时≈64*1250/40k=2s
    IWDG_ReloadCounter();                      // 喂狗一次
    IWDG_Enable();
}

/* 硬件喂狗函数 */
static void HardwareWatchdog_Feed(void) {
    IWDG_ReloadCounter();   // 重新加载计数器
}

/* ======================== 空闲任务钩子（硬件喂狗） ======================== */
/* 在 FreeRTOS 的配置文件 FreeRTOSConfig.h 中，需定义 configUSE_IDLE_HOOK 为 1 */
void vApplicationIdleHook(void) {
    HardwareWatchdog_Feed();   // 空闲时喂狗
}

/* ======================== 任务心跳上报函数 ======================== */
/* 被监控的任务定期调用此函数，更新自己的心跳时间 */
void Watchdog_ReportHeartbeat(TaskHandle_t task) {
    for (int i = 0; i < WATCHDOG_TASK_NUM; i++) {
        if (s_watchdog_tasks[i].task_handle == task && s_watchdog_tasks[i].active) {
            s_watchdog_tasks[i].last_tick = xTaskGetTickCount();
            break;
        }
    }
}

/* ======================== 软件看门狗任务 ======================== */
static void vSoftWatchdogTask(void *pvParameters) {
    TickType_t current_tick;
    const TickType_t check_interval = pdMS_TO_TICKS(500);  // 每500ms检查一次

    for (;;) {
        vTaskDelay(check_interval);   // 周期检查
        current_tick = xTaskGetTickCount();

        for (int i = 0; i < WATCHDOG_TASK_NUM; i++) {
            if (!s_watchdog_tasks[i].active) continue;

            TickType_t elapsed = current_tick - s_watchdog_tasks[i].last_tick;
            if (elapsed > TASK_TIMEOUT_TICKS) {
                // 任务超时，记录故障
                const char *task_name = s_watchdog_tasks[i].task_name;
                // 这里可以保存故障信息到 Flash/EEPROM 或通过 CAN 上报
                // 例如：save_fault_log(FAULT_TASK_TIMEOUT, task_name);

                // 根据产品策略选择恢复方式
                // 方式1：重启整个系统（硬件复位）
                NVIC_SystemReset();
                // 方式2：仅重启该任务（需谨慎处理资源释放）
                // 先挂起调度器，删除旧任务，重新创建
                // vTaskSuspendAll();
                // vTaskDelete(s_watchdog_tasks[i].task_handle);
                // xTaskCreate(..., &s_watchdog_tasks[i].task_handle);
                // xTaskResumeAll();
                // 注意：重启任务可能导致资源泄露，仅适用于简单任务
            }
        }
    }
}

/* ======================== 初始化软件看门狗 ======================== */
void SoftWatchdog_Init(void) {
    // 初始化任务列表（示例：假设有5个关键任务，需在创建任务后填充）
    // 实际应用中，应在每个任务创建后，调用 Watchdog_RegisterTask 注册
    for (int i = 0; i < WATCHDOG_TASK_NUM; i++) {
        s_watchdog_tasks[i].active = 0;
    }

    // 创建软件看门狗任务（优先级略低于关键任务，但需保证及时检查）
    xTaskCreate(vSoftWatchdogTask, "SoftWdt", 256, NULL, 2, &xSoftWatchdogTask);
}

/* ======================== 注册被监控任务 ======================== */
/* 在任务创建后调用此函数，将任务加入监控列表 */
void Watchdog_RegisterTask(TaskHandle_t task, const char *name) {
    for (int i = 0; i < WATCHDOG_TASK_NUM; i++) {
        if (!s_watchdog_tasks[i].active) {
            s_watchdog_tasks[i].task_handle = task;
            s_watchdog_tasks[i].task_name = name;
            s_watchdog_tasks[i].last_tick = xTaskGetTickCount();
            s_watchdog_tasks[i].active = 1;
            break;
        }
    }
}

/* ======================== 示例：关键任务实现 ======================== */
/* 被监控的任务需定期调用 Watchdog_ReportHeartbeat */
void vCriticalTask(void *pvParameters) {
    // 注册本任务到软件看门狗
    Watchdog_RegisterTask(xTaskGetCurrentTaskHandle(), "CriticalTask");

    for (;;) {
        // 执行关键操作
        // ...

        // 上报心跳（必须在超时周期内调用）
        Watchdog_ReportHeartbeat(xTaskGetCurrentTaskHandle());

        // 假设任务周期100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ======================== 系统初始化 ======================== */
void Watchdog_SystemInit(void) {
    // 1. 初始化硬件看门狗
    HardwareWatchdog_Init();

    // 2. 初始化软件看门狗任务
    SoftWatchdog_Init();

    // 3. 创建需要监控的关键任务（示例）
    xTaskCreate(vCriticalTask, "Critical", 512, NULL, 3, NULL);
    // 创建其他任务...
}