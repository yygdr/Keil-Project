#include "can_driver.h"
#include "stm32f1xx_hal.h"   // 以 STM32 为例，根据实际芯片替换
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ======================== 硬件相关定义 ======================== */
extern CAN_HandleTypeDef hcan;   // 假设已由 CubeMX 初始化

/* ======================== RTOS 对象 ======================== */
static QueueHandle_t xCanRxQueue = NULL;     // 接收队列（中断 -> 解析任务）
static QueueHandle_t xCanTxQueue = NULL;     // 发送队列（业务任务 -> 发送任务）
static SemaphoreHandle_t xCanTxMutex = NULL; // 保护硬件发送寄存器的互斥量

/* ======================== 任务句柄 ======================== */
static TaskHandle_t xCanParseTaskHandle = NULL;
static TaskHandle_t xCanSendTaskHandle = NULL;

/* ======================== 接收队列配置 ======================== */
#define CAN_RX_QUEUE_LEN   16   // 接收队列深度
#define CAN_TX_QUEUE_LEN   32   // 发送队列深度

/* ======================== CAN 接收中断回调 ======================== */
/* 在 HAL_CAN_RxFifo0MsgPendingCallback 中调用此函数 */
static void CAN_RxCallback(void) {
    CanFrame_t rxFrame;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 从硬件 FIFO 读取一帧（使用 HAL 库）
    if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, (CAN_RxHeaderTypeDef*)&rxFrame, rxFrame.data) == HAL_OK) {
        // 转换为内部帧格式
        rxFrame.id = rxFrame.id;  // 实际需从 header 提取
        rxFrame.dlc = rxFrame.dlc;
        rxFrame.is_extended = (rxFrame.id >> 31) & 0x01; // 简化处理

        // 发送到接收队列
        xQueueSendFromISR(xCanRxQueue, &rxFrame, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ======================== CAN 解析任务 ======================== */
/* 统一处理接收到的 CAN 帧，根据 ID 分发到不同业务模块 */
static void vCanParseTask(void *pvParameters) {
    CanFrame_t rxFrame;
    for (;;) {
        if (xQueueReceive(xCanRxQueue, &rxFrame, portMAX_DELAY) == pdTRUE) {
            /* 根据 CAN ID 分发 */
            switch (rxFrame.id) {
                case 0x100:   // BMS 状态上报请求
                    // 调用 BMS 状态处理函数
                    break;
                case 0x200:   // 诊断请求
                    // 调用诊断处理模块
                    break;
                case 0x300:   // Bootloader 指令
                    // 调用 Bootloader 处理函数
                    break;
                default:
                    // 忽略或记录
                    break;
            }
        }
    }
}

/* ======================== CAN 发送任务 ======================== */
/* 从发送队列取帧，使用互斥量保护硬件，实际发送 */
static void vCanSendTask(void *pvParameters) {
    CanFrame_t txFrame;
    for (;;) {
        if (xQueueReceive(xCanTxQueue, &txFrame, portMAX_DELAY) == pdTRUE) {
            // 获取互斥量，防止多任务同时操作发送寄存器
            xSemaphoreTake(xCanTxMutex, portMAX_DELAY);

            // 配置发送 header（标准/扩展帧、ID、DLC）
            CAN_TxHeaderTypeDef txHeader;
            txHeader.ExtId = txFrame.id;
            txHeader.IDE = txFrame.is_extended ? CAN_ID_EXT : CAN_ID_STD;
            txHeader.RTR = CAN_RTR_DATA;
            txHeader.DLC = txFrame.dlc;
            txHeader.TransmitGlobalTime = DISABLE;

            uint32_t txMailbox;
            HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(&hcan, &txHeader, txFrame.data, &txMailbox);
            if (status != HAL_OK) {
                // 发送失败处理（重试或记录错误）
            }

            xSemaphoreGive(xCanTxMutex);
        }
    }
}

/* ======================== 对外 API ======================== */
void CAN_Init(void) {
    // 创建 RTOS 对象
    xCanRxQueue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(CanFrame_t));
    xCanTxQueue = xQueueCreate(CAN_TX_QUEUE_LEN, sizeof(CanFrame_t));
    xCanTxMutex = xSemaphoreCreateMutex();

    configASSERT(xCanRxQueue != NULL && xCanTxQueue != NULL && xCanTxMutex != NULL);

    // 启动 CAN 硬件（HAL 层）
    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

    // 注册接收回调（在 HAL_CAN_RxFifo0MsgPendingCallback 中调用 CAN_RxCallback）
    // 通常需要修改 HAL 库回调函数，或使用弱函数覆盖
    // 这里假设在 stm32f1xx_it.c 中已调用 CAN_RxCallback()

    // 创建解析任务和发送任务（优先级可调）
    xTaskCreate(vCanParseTask, "CanParse", 256, NULL, 3, &xCanParseTaskHandle);
    xTaskCreate(vCanSendTask,  "CanSend",  256, NULL, 2, &xCanSendTaskHandle);
}

BaseType_t CAN_SendFrame(const CanFrame_t *pFrame, TickType_t xTicksToWait) {
    // 业务任务调用，将帧放入发送队列
    return xQueueSend(xCanTxQueue, pFrame, xTicksToWait);
}

QueueHandle_t CAN_GetRxQueueHandle(void) {
    return xCanRxQueue;
}