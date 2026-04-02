/* ======================== 头文件与类型定义 ======================== */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "stm32f1xx_hal.h"   // 以 STM32F1 为例，根据实际芯片替换

/* 定义采样参数 */
#define ADC_CHANNELS        3          // 采样通道数
#define BUFFER_SIZE         256        // 每个缓冲区的采样点数（每个点包含所有通道）
#define ADC_BUFFER_HALF     (BUFFER_SIZE / 2)

/* 定义消息队列传递的数据结构（每个采样点转换后的物理量） */
typedef struct {
    float voltage[ADC_CHANNELS];   // 各通道转换后的电压值（或电流/温度等）
    uint32_t timestamp;            // 采样时间戳（可选）
} AdcData_t;

/* 全局句柄 */
static SemaphoreHandle_t xAdcDataReadySemaphore = NULL;   // 二值信号量，用于唤醒数据处理任务
static QueueHandle_t xAdcDataQueue = NULL;               // 消息队列，向其他任务分发数据

/* DMA 双缓冲区（位于 SRAM） */
static uint16_t adc_buffer_0[ADC_CHANNELS * BUFFER_SIZE];   // 缓冲区0
static uint16_t adc_buffer_1[ADC_CHANNELS * BUFFER_SIZE];   // 缓冲区1
static uint8_t current_buffer = 0;   // 当前由 DMA 写入的缓冲区索引 (0或1)

/* ADC 句柄（以 ADC1 为例，需根据实际配置） */
extern ADC_HandleTypeDef hadc1;

/* ======================== 硬件初始化函数（简化） ======================== */
void MX_ADC_DMA_Init(void) {
    // 假设 hadc1 已配置为多通道扫描、连续转换模式
    // 使能 DMA 双缓冲模式
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer_0, ADC_CHANNELS * BUFFER_SIZE);
    // 设置双缓冲（注意：不同芯片 API 可能不同，此处示意）
    // STM32 HAL 库中 DMA 双缓冲配置通常需要先停止 DMA 再设置
    // 以下为通用逻辑（以 STM32F1 为例）
    __HAL_DMA_DISABLE(hadc1.DMA_Handle);
    hadc1.DMA_Handle->Instance->CNDTR = ADC_CHANNELS * BUFFER_SIZE;
    hadc1.DMA_Handle->Instance->CMAR = (uint32_t)adc_buffer_0;
    hadc1.DMA_Handle->Instance->CNDTR = ADC_CHANNELS * BUFFER_SIZE;
    hadc1.DMA_Handle->Instance->CNDTR = ADC_CHANNELS * BUFFER_SIZE; // 重复设置？
    // 更可靠的方式：使用 DMA 双缓冲模式，通过 HAL_DMAEx_MultiBufferStart 或类似函数
    // 此处简化：直接调用 HAL 库双缓冲启动函数（需芯片支持）
    HAL_DMAEx_MultiBufferStart(hadc1.DMA_Handle, (uint32_t)ADC1->DR,
                               (uint32_t)adc_buffer_0, (uint32_t)adc_buffer_1,
                               ADC_CHANNELS * BUFFER_SIZE);
    __HAL_DMA_ENABLE(hadc1.DMA_Handle);
    // 使能 DMA 传输完成中断和半传输完成中断
    __HAL_DMA_ENABLE_IT(hadc1.DMA_Handle, DMA_IT_TC | DMA_IT_HT);
    HAL_ADC_Start(&hadc1);
}

/* DMA 传输半完成中断回调（由 HAL 库调用） */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // 释放信号量，通知数据处理任务处理前半缓冲区
    xSemaphoreGiveFromISR(xAdcDataReadySemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* DMA 传输完成中断回调（由 HAL 库调用） */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // 释放信号量，通知数据处理任务处理后半缓冲区
    xSemaphoreGiveFromISR(xAdcDataReadySemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ======================== 数据处理任务 ======================== */
/* 滑动平均滤波（简单示例，针对单通道） */
static float sliding_average(float *buffer, uint16_t len, uint16_t *index, float new_val) {
    static float sum = 0;
    sum -= buffer[*index];
    buffer[*index] = new_val;
    sum += new_val;
    *index = (*index + 1) % len;
    return sum / len;
}

void vAdcDataProcessTask(void *pvParameters) {
    uint16_t *buffer_to_process = NULL;
    uint16_t half_size = ADC_CHANNELS * ADC_BUFFER_HALF;
    uint16_t full_size = ADC_CHANNELS * BUFFER_SIZE;
    AdcData_t adc_data;
    // 为每个通道准备滑动平均缓冲区（此处假设滑动窗口大小为 16）
    #define AVG_WINDOW 16
    static float avg_buffer[ADC_CHANNELS][AVG_WINDOW];
    static uint16_t avg_index[ADC_CHANNELS] = {0};
    float raw_value;   // 原始 ADC 值（0~4095）
    float voltage;     // 转换后电压（示例：假设参考电压 3.3V，12位 ADC）

    for (;;) {
        // 等待信号量（阻塞直到有缓冲区可用）
        if (xSemaphoreTake(xAdcDataReadySemaphore, portMAX_DELAY) == pdTRUE) {
            // 判断当前应该处理哪个缓冲区（由 DMA 中断触发顺序决定）
            // 实际应通过全局标志或寄存器判断，这里简化：根据 current_buffer 切换
            // 注意：需保证在中断中切换缓冲区索引时互斥，此处用临界区保护
            taskENTER_CRITICAL();
            if (current_buffer == 0) {
                buffer_to_process = adc_buffer_0;
                current_buffer = 1;   // 下次中断将使用 buffer_1
            } else {
                buffer_to_process = adc_buffer_1;
                current_buffer = 0;
            }
            taskEXIT_CRITICAL();

            // 处理当前缓冲区（按通道顺序）
            // 假设 ADC 采样顺序为 CH1, CH2, CH3, CH1, CH2, CH3, ...
            for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
                for (uint8_t ch = 0; ch < ADC_CHANNELS; ch++) {
                    uint16_t raw = buffer_to_process[i * ADC_CHANNELS + ch];
                    // 转换为物理量（电压）
                    raw_value = (float)raw;
                    voltage = raw_value * 3.3f / 4095.0f;   // 参考电压 3.3V
                    // 滑动平均滤波
                    float filtered = sliding_average(avg_buffer[ch], AVG_WINDOW, &avg_index[ch], voltage);
                    adc_data.voltage[ch] = filtered;   // 存储滤波后的值
                }
                adc_data.timestamp = xTaskGetTickCount();
                // 将处理后的数据发送到消息队列（非阻塞发送，队列满则丢弃）
                xQueueSend(xAdcDataQueue, &adc_data, 0);
            }
        }
    }
}

/* ======================== 其他任务（示例：SOC估算任务） ======================== */
void vSocEstimationTask(void *pvParameters) {
    AdcData_t received_data;
    for (;;) {
        // 等待从队列接收数据
        if (xQueueReceive(xAdcDataQueue, &received_data, portMAX_DELAY) == pdTRUE) {
            // 处理接收到的数据，例如使用电压计算 SOC
            // 这里仅示例
            // process_soc(received_data.voltage[0]);
        }
    }
}

/* ======================== 系统初始化（任务创建） ======================== */
void AdcDmaDemo_Init(void) {
    // 创建二值信号量（初始为0）
    xAdcDataReadySemaphore = xSemaphoreCreateBinary();
    configASSERT(xAdcDataReadySemaphore != NULL);

    // 创建消息队列（容纳 32 个数据包）
    xAdcDataQueue = xQueueCreate(32, sizeof(AdcData_t));
    configASSERT(xAdcDataQueue != NULL);

    // 创建数据处理任务（高优先级，高于普通业务任务）
    xTaskCreate(vAdcDataProcessTask, "AdcProc", 512, NULL, 4, NULL);
    // 创建 SOC 估算任务（中等优先级）
    xTaskCreate(vSocEstimationTask, "SocEst", 512, NULL, 2, NULL);
    // 其他任务...
}

/* 主函数（在启动调度器前调用） */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_DMA_Init();          // 假设有 DMA 初始化函数

    AdcDmaDemo_Init();      // 创建 RTOS 对象和任务

    // 启动 ADC DMA 采样
    MX_ADC_DMA_Init();

    // 启动调度器
    vTaskStartScheduler();

    while (1) {
        // 不会到达
    }
}




/* ======================== 头文件与类型定义（同上） ======================== */
// 相同部分省略...

/* DMA 软件乒乓缓冲区 */
static uint16_t adc_buffer_0[ADC_CHANNELS * BUFFER_SIZE];
static uint16_t adc_buffer_1[ADC_CHANNELS * BUFFER_SIZE];
static uint8_t active_buffer = 0;      // 当前 DMA 正在填充的缓冲区索引
static uint8_t pending_buffer = 1;     // 等待处理的缓冲区索引

/* DMA 传输完成中断回调（无硬件双缓冲时，只产生一个完成中断） */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // 切换 DMA 目标地址到另一个缓冲区
    if (active_buffer == 0) {
        // 当前 DMA 刚填满 buffer_0，切换 DMA 到 buffer_1
        HAL_DMA_Start_IT(hadc1.DMA_Handle, (uint32_t)&hadc1.Instance->DR,
                         (uint32_t)adc_buffer_1, ADC_CHANNELS * BUFFER_SIZE);
        active_buffer = 1;
        pending_buffer = 0;   // buffer_0 变为待处理
    } else {
        HAL_DMA_Start_IT(hadc1.DMA_Handle, (uint32_t)&hadc1.Instance->DR,
                         (uint32_t)adc_buffer_0, ADC_CHANNELS * BUFFER_SIZE);
        active_buffer = 0;
        pending_buffer = 1;
    }
    // 释放信号量，通知数据处理任务处理已填满的缓冲区
    xSemaphoreGiveFromISR(xAdcDataReadySemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* 数据处理任务（需根据 pending_buffer 选择缓冲区） */
static void vAdcDataProcessTask(void *pvParameters) {
    uint16_t *buffer_to_process = NULL;
    // ... 其他变量同上

    for (;;) {
        xSemaphoreTake(xAdcDataReadySemaphore, portMAX_DELAY);

        // 获取当前待处理的缓冲区（需临界区保护，避免与中断冲突）
        taskENTER_CRITICAL();
        if (pending_buffer == 0) {
            buffer_to_process = adc_buffer_0;
            // 注意：这里不能立即修改 pending_buffer，因为 DMA 可能还在使用另一个缓冲区
            // 实际使用中，应确保数据处理完成后才允许 DMA 再次使用该缓冲区
            // 本例简化，实际工程中可使用双缓冲标志或环形队列。
        } else {
            buffer_to_process = adc_buffer_1;
        }
        taskEXIT_CRITICAL();

        // 处理缓冲区数据（同上）...
        // 注意：数据处理完成后，应将处理过的缓冲区标记为可用，以便 DMA 下次使用。
        // 这里省略了详细同步机制，实际开发中需要更精细的状态机。
    }
}