#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "modbus.h"
#include "flash.h"
#include "ota.h"
#include "modbus_slave.h"

#define DE_PORT         GPIOB
#define DE_PIN          GPIO_PIN_8
#define DE_TX()         HAL_GPIO_WritePin(DE_PORT, DE_PIN, GPIO_PIN_SET)
#define DE_RX()         HAL_GPIO_WritePin(DE_PORT, DE_PIN, GPIO_PIN_RESET)

#define FRAME_MAX       32
#define FRAME_GAP_MS    5               /* 3.5 chars @9600 = 4 ms */

/* register map exposed to the PLC */
enum { R_TEMP, R_HUMI, R_LUX_H, R_LUX_L, R_NET, R_NUM };

static UART_HandleTypeDef huart6;
static uint8_t  rx_byte;
static uint8_t  rx_buf[FRAME_MAX];
static uint8_t  tx_buf[FRAME_MAX];
static volatile uint8_t  rx_len;
static volatile uint32_t rx_tick;
static uint16_t regs[R_NUM];

/* non-blocking: a blocking send would starve the master's polling RX on USART2 */
static void Send(uint8_t len)
{
    if (huart6.gState != HAL_UART_STATE_READY) return;
    DE_TX();
    if (HAL_UART_Transmit_IT(&huart6, tx_buf, len) != HAL_OK) DE_RX();
}

static void ReplyException(uint8_t fc, uint8_t code)
{
    uint16_t crc;

    tx_buf[0] = MB_SLAVE_ADDR;
    tx_buf[1] = fc | 0x80;
    tx_buf[2] = code;
    crc = CRC_16_MODBUS(tx_buf, 3, 0);
    tx_buf[3] = crc & 0xFF;
    tx_buf[4] = crc >> 8;
    Send(5);
}

static void HandleFrame(uint8_t *f, uint8_t n)
{
    uint8_t  fc, i, p;
    uint16_t crc, start, cnt;

    if (n < 8 || f[0] != MB_SLAVE_ADDR) return;

    crc = CRC_16_MODBUS(f, n - 2, 1);
    if (f[n - 2] != (crc & 0xFF) || f[n - 1] != (crc >> 8)) return;

    fc = f[1];
    if (fc == 0x06) {                       /* write single register: only 0x00FF/0x5A5A is used */
        uint16_t reg = ((uint16_t)f[2] << 8) | f[3];
        uint16_t val = ((uint16_t)f[4] << 8) | f[5];
        memcpy(tx_buf, f, n);
        Send(n);
        if (reg == 0x00FF && val == 0x5A5A) {
            vTaskDelay(pdMS_TO_TICKS(100));
            OTA_Trigger();
        }
        return;
    }
    if (fc != 0x03 && fc != 0x04) { ReplyException(fc, 0x01); return; }

    start = ((uint16_t)f[2] << 8) | f[3];
    cnt   = ((uint16_t)f[4] << 8) | f[5];
    if (cnt == 0 || cnt > R_NUM || start + cnt > R_NUM) { ReplyException(fc, 0x02); return; }

    p = 0;
    tx_buf[p++] = MB_SLAVE_ADDR;
    tx_buf[p++] = fc;
    tx_buf[p++] = (uint8_t)(cnt * 2);
    for (i = 0; i < cnt; i++) {
        tx_buf[p++] = (uint8_t)(regs[start + i] >> 8);
        tx_buf[p++] = (uint8_t)(regs[start + i] & 0xFF);
    }
    crc = CRC_16_MODBUS(tx_buf, p, 0);
    tx_buf[p++] = crc & 0xFF;
    tx_buf[p++] = crc >> 8;
    Send(p);
}

static void UpdateRegs(void)
{
    uint32_t lux = (dev_cnt > 1) ? (uint32_t)dev_list[1].val1 : 0;

    regs[R_TEMP]  = (uint16_t)(dev_list[0].val1 * 10.0f);
    regs[R_HUMI]  = (uint16_t)(dev_list[0].val2 * 10.0f);
    regs[R_LUX_H] = (uint16_t)(lux >> 16);
    regs[R_LUX_L] = (uint16_t)(lux & 0xFFFF);
    regs[R_NET]   = g_mqtt_connected;
}

static void SlaveInit(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();

    g.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin       = DE_PIN;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = 0;
    HAL_GPIO_Init(DE_PORT, &g);
    DE_RX();

    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = 9600;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);

    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
}

void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart6);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h->Instance != USART6) return;
    if (rx_len < FRAME_MAX) rx_buf[rx_len++] = rx_byte;
    rx_tick = HAL_GetTick();
    HAL_UART_Receive_IT(h, &rx_byte, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART6) DE_RX();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h->Instance != USART6) return;
    __HAL_UART_CLEAR_OREFLAG(h);
    rx_len = 0;
    HAL_UART_Receive_IT(h, &rx_byte, 1);
}

void ModbusSlave_Task(void *arg)
{
    uint8_t f[FRAME_MAX], n;

    (void)arg;
    SlaveInit();

    for (;;) {
        if (rx_len && (HAL_GetTick() - rx_tick) >= FRAME_GAP_MS) {
            __HAL_UART_DISABLE_IT(&huart6, UART_IT_RXNE);
            n = rx_len;
            memcpy(f, rx_buf, n);
            rx_len = 0;
            __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
            HandleFrame(f, n);
        }
        UpdateRegs();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
