/* ======================== 头文件与类型定义 ======================== */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* 故障类型枚举 */
typedef enum {
    FAULT_NONE = 0,
    FAULT_OPEN,                 // 开路（MSD拔出/连接器脱落）
    FAULT_SHORT_TO_VCC,         // 短路到电源
    FAULT_SHORT_TO_GND,         // 短路到地
    FAULT_HIGH_RESISTANCE       // 接触不良（回路电阻增大）
} InterlockFault_t;

/* 消息队列传递的采样数据结构 */
typedef struct {
    uint16_t adc_value;         // ADC采样的回波电压值（0~4095，12位ADC）
    uint32_t timestamp;         // 采样时间戳（可选，用于诊断）
} InterlockSample_t;

/* 监控任务状态机结构 */
typedef struct {
    InterlockFault_t current_fault;   // 当前确认的故障
    uint8_t fault_counter;            // 连续故障计数（防抖）
    uint16_t last_adc;                // 上次ADC值
    uint16_t adc_normal_low;          // 正常区间下限（标定值）
    uint16_t adc_normal_high;         // 正常区间上限
    uint16_t adc_short_gnd_th;        // 短路到地阈值（低于此值）
    uint16_t adc_short_vcc_th;        // 短路到电源阈值（高于此值）
    uint16_t adc_high_res_th;         // 接触不良阈值（低于正常下限但高于短路到地）
} InterlockState_t;

/* ======================== 全局变量 ======================== */
static QueueHandle_t xInterlockQueue = NULL;          // 消息队列句柄
static TaskHandle_t xInterlockMonitorTask = NULL;    // 监控任务句柄
static TaskHandle_t xInterlockSafetyTask = NULL;     // 安全响应任务句柄
static InterlockState_t g_state;                     // 状态机

/* ======================== 硬件抽象函数（需用户实现） ======================== */
extern void hv_disable(void);               // 切断高压继电器
extern void can_send_interlock_fault(InterlockFault_t fault);   // 发送故障CAN报文
extern void set_warning_led(InterlockFault_t fault);           // 设置警告灯

/* ======================== 故障诊断核心函数 ======================== */
static InterlockFault_t diagnose_fault(uint16_t adc_value) {
    // 短路到地：ADC值极低
    if (adc_value <= g_state.adc_short_gnd_th) {
        return FAULT_SHORT_TO_GND;
    }
    // 短路到电源：ADC值极高
    if (adc_value >= g_state.adc_short_vcc_th) {
        return FAULT_SHORT_TO_VCC;
    }
    // 接触不良：ADC值低于正常下限但高于短路阈值
    if (adc_value < g_state.adc_normal_low) {
        return FAULT_HIGH_RESISTANCE;
    }
    // 正常：ADC值在正常区间内
    if (adc_value >= g_state.adc_normal_low && adc_value <= g_state.adc_normal_high) {
        return FAULT_NONE;
    }
    // 其他异常（如超出正常上限但未到短路VCC），可作为开路预警
    // 实际工程中开路通常通过超时检测，这里返回NONE
    return FAULT_NONE;
}

/* ======================== 安全响应任务 ======================== */
static void vInterlockSafetyTask(void *pvParameters) {
    for (;;) {
        // 阻塞等待监控任务的通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // 接收到通知，根据当前故障等级执行安全措施
        switch (g_state.current_fault) {
            case FAULT_OPEN:
            case FAULT_SHORT_TO_VCC:
                // 高危故障：立即切断高压，并发送故障报文
                hv_disable();
                can_send_interlock_fault(g_state.current_fault);
                set_warning_led(g_state.current_fault);
                // 可挂起所有其他任务，系统进入安全状态（或复位）
                // vTaskSuspendAll();
                break;
            case FAULT_SHORT_TO_GND:
                // 二级故障：限功率运行，发送警告
                can_send_interlock_fault(g_state.current_fault);
                set_warning_led(g_state.current_fault);
                // 可调用限功率函数（略）
                break;
            case FAULT_HIGH_RESISTANCE:
                // 三级故障：仅记录预警，点亮保养灯
                can_send_interlock_fault(g_state.current_fault);
                set_warning_led(g_state.current_fault);
                break;
            default:
                break;
        }
    }
}

/* ======================== 监控任务（周期50ms） ======================== */
static void vInterlockMonitorTask(void *pvParameters) {
    InterlockSample_t sample;
    BaseType_t xResult;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(50);   // 50ms周期

    // 初始化状态机阈值（需根据实际电路标定）
    g_state.adc_normal_low    = 800;    // 正常区间下限（标定值）
    g_state.adc_normal_high   = 3200;   // 正常区间上限
    g_state.adc_short_gnd_th  = 50;     // 短路到地阈值
    g_state.adc_short_vcc_th  = 4000;   // 短路到电源阈值
    g_state.adc_high_res_th   = 600;    // 接触不良阈值（低于此值但高于短路到地）
    g_state.current_fault     = FAULT_NONE;
    g_state.fault_counter     = 0;
    g_state.last_adc          = 0;

    for (;;) {
        // 从消息队列接收采样数据，阻塞等待最多一个周期（50ms）
        xResult = xQueueReceive(xInterlockQueue, &sample, xPeriod);
        if (xResult == pdTRUE) {
            // 收到新采样数据
            g_state.last_adc = sample.adc_value;
            InterlockFault_t new_fault = diagnose_fault(sample.adc_value);

            // 防抖逻辑：连续3次相同故障才确认
            if (new_fault != FAULT_NONE) {
                if (new_fault == g_state.current_fault) {
                    g_state.fault_counter++;
                } else {
                    g_state.current_fault = new_fault;
                    g_state.fault_counter = 1;
                }
            } else {
                // 无故障时，若当前故障不是高危故障，则清除计数器
                // 高危故障（开路/短路到电源）需锁存，不自动清除
                if (g_state.current_fault != FAULT_OPEN &&
                    g_state.current_fault != FAULT_SHORT_TO_VCC) {
                    g_state.fault_counter = 0;
                }
            }

            // 故障确认（连续3次）后，唤醒安全响应任务
            if (g_state.fault_counter >= 3) {
                // 通过任务通知唤醒安全任务
                xTaskNotifyGive(xInterlockSafetyTask);
            }
        } else {
            // 队列超时（未收到新采样），可能ADC任务异常或PWM信号完全丢失
            // 视为开路故障
            if (++g_state.fault_counter >= 3) {
                g_state.current_fault = FAULT_OPEN;
                xTaskNotifyGive(xInterlockSafetyTask);
            }
        }

        // 精确延时，确保50ms周期运行（即使有队列超时，也保证周期）
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ======================== 初始化函数 ======================== */
void Interlock_Init(void) {
    // 创建消息队列（缓存5个采样值）
    xInterlockQueue = xQueueCreate(5, sizeof(InterlockSample_t));
    configASSERT(xInterlockQueue != NULL);

    // 创建监控任务（高优先级，例如3）
    xTaskCreate(vInterlockMonitorTask, "InterlockMon", 256, NULL, 3, &xInterlockMonitorTask);
    configASSERT(xInterlockMonitorTask != NULL);

    // 创建安全响应任务（优先级略低，例如2）
    xTaskCreate(vInterlockSafetyTask, "InterlockSafe", 256, NULL, 2, &xInterlockSafetyTask);
    configASSERT(xInterlockSafetyTask != NULL);
}

/* ======================== 外部接口：由ADC采样任务调用，放入采样数据 ======================== */
void Interlock_FeedSample(uint16_t adc_value) {
    InterlockSample_t sample;
    sample.adc_value = adc_value;
    sample.timestamp = xTaskGetTickCount();
    // 如果队列满，丢弃本次采样（可记录错误）
    xQueueSend(xInterlockQueue, &sample, 0);
}