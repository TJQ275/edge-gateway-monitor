/* OTA bootloader: 0x08000000 (32K) | meta 0x08008000 (16K) | app 0x0800C000 (464K)
 * Download: XMODEM-CRC on USART6 (PA11 TX / PA12 RX / PB8 DE), 115200 8N1.
 * Only the bootloader writes flash, so a failed update never bricks the board. */

#include "stm32f4xx_hal.h"

#include "ota_layout.h"

#define APP_ADDR      OTA_APP_ADDR
#define APP_MAX       OTA_APP_SIZE
#define META_ADDR     OTA_META_ADDR
#define META_SECTOR   OTA_META_SECTOR
#define META_MAGIC    OTA_META_MAGIC
#define META_UPD      OTA_UPD_MAGIC
#define META_BLANK    0xFFFFFFFFU

#define SOH   0x01
#define STX   0x02
#define EOT   0x04
#define ACK   0x06
#define NAK   0x15
#define CAN   0x18
#define XC    0x43

typedef struct { uint32_t magic, len, crc, inv; } meta_t;
typedef void (*entry_t)(void);

static UART_HandleTypeDef huart;
static uint32_t erased_end = APP_ADDR;

/* ---------- crc ---------- */
static const uint32_t crc_tab[16] = {
    0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
    0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
    0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
    0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
};

static uint32_t crc32_update(uint32_t c, const uint8_t *p, uint32_t len)
{
    while (len--) {
        c ^= *p++;
        c = (c >> 4) ^ crc_tab[c & 0x0FU];
        c = (c >> 4) ^ crc_tab[c & 0x0FU];
    }
    return c;
}

static uint32_t crc32_buf(uint32_t addr, uint32_t len)
{
    return ~crc32_update(0xFFFFFFFFU, (const uint8_t *)addr, len);
}

static uint16_t crc16_ccitt(const uint8_t *p, uint32_t len)
{
    uint16_t c = 0;
    uint8_t  i;

    while (len--) {
        c ^= (uint16_t)(*p++) << 8;
        for (i = 0; i < 8U; i++)
            c = (c & 0x8000U) ? (uint16_t)((c << 1) ^ 0x1021U) : (uint16_t)(c << 1);
    }
    return c;
}

/* ---------- flash ---------- */
static uint32_t sector_of(uint32_t a, uint32_t *size)
{
    if (a < 0x08010000U) { *size = 0x4000U;  return (a - 0x08000000U) >> 14; }
    if (a < 0x08020000U) { *size = 0x10000U; return 4U; }
    if (a < 0x08040000U) { *size = 0x20000U; return 5U; }
    if (a < 0x08060000U) { *size = 0x20000U; return 6U; }
    *size = 0x20000U; return 7U;
}

static int flash_erase(uint32_t sector)
{
    FLASH_EraseInitTypeDef e = {0};
    uint32_t err = 0;
    int r;

    e.TypeErase    = FLASH_TYPEERASE_SECTORS;
    e.Sector       = sector;
    e.NbSectors    = 1;
    e.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    r = (HAL_FLASHEx_Erase(&e, &err) == HAL_OK) ? 0 : -1;
    HAL_FLASH_Lock();
    return r;
}

static int flash_write(uint32_t addr, const uint8_t *p, uint32_t len)
{
    int r = 0;

    HAL_FLASH_Unlock();
    while (len >= 4U) {
        uint32_t w = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, w) != HAL_OK) { r = -1; break; }
        addr += 4U; p += 4U; len -= 4U;
    }
    while (r == 0 && len--) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, *p++) != HAL_OK) { r = -1; break; }
        addr++;
    }
    HAL_FLASH_Lock();
    return r;
}

static int ensure_erased(uint32_t addr, uint32_t len)
{
    while (addr + len > erased_end) {
        uint32_t sz, sec = sector_of(erased_end, &sz);
        if (flash_erase(sec)) return -1;
        erased_end += sz;
    }
    return 0;
}

/* ---------- meta ---------- */
static uint32_t meta_state(void)
{
    return *(const volatile uint32_t *)META_ADDR;
}

static int meta_valid(void)
{
    const meta_t *m = (const meta_t *)META_ADDR;

    if (m->magic != META_MAGIC || m->inv != ~META_MAGIC) return 0;
    if (m->len < 0x100U || m->len > APP_MAX) return 0;
    return crc32_buf(APP_ADDR, m->len) == m->crc;
}

/* Mark "update requested" before touching the app, so a power cut keeps us in
 * download mode instead of booting a half written image. */
static int meta_mark_upd(void)
{
    uint32_t v[2];

    v[0] = META_UPD;
    v[1] = ~META_UPD;
    if (flash_erase(META_SECTOR)) return -1;
    return flash_write(META_ADDR, (const uint8_t *)v, sizeof(v));
}

static int meta_save(uint32_t len, uint32_t crc)
{
    meta_t m;

    m.magic = META_MAGIC;
    m.len   = len;
    m.crc   = crc;
    m.inv   = ~META_MAGIC;
    if (flash_erase(META_SECTOR)) return -1;
    return flash_write(META_ADDR, (const uint8_t *)&m, sizeof(m));
}

/* ---------- jump ---------- */
static int app_ok(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_ADDR;
    uint32_t pc = *(volatile uint32_t *)(APP_ADDR + 4U);

    if (sp < 0x20000000U || sp > 0x20020000U) return 0;
    if (pc < APP_ADDR + 8U || pc >= APP_ADDR + APP_MAX) return 0;
    return 1;
}

static void jump_app(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_ADDR;
    uint32_t pc = *(volatile uint32_t *)(APP_ADDR + 4U);
    uint32_t i;

    for (i = 0; i < 8U; i++) NVIC->ICER[i] = 0xFFFFFFFFU;
    for (i = 0; i < 8U; i++) NVIC->ICPR[i] = 0xFFFFFFFFU;
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    SCB->VTOR = APP_ADDR;
    __set_MSP(sp);
    ((entry_t)pc)();
    for (;;) { }
}

/* ---------- uart ---------- */
static void uart_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();

    g.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin       = GPIO_PIN_8;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);

    huart.Instance          = USART6;
    huart.Init.BaudRate     = 115200;
    huart.Init.WordLength   = UART_WORDLENGTH_8B;
    huart.Init.StopBits     = UART_STOPBITS_1;
    huart.Init.Parity       = UART_PARITY_NONE;
    huart.Init.Mode         = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart);
}

static void tx(uint8_t c)
{
    uint32_t t = HAL_GetTick();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_UART_Transmit(&huart, &c, 1, 100);
    while (__HAL_UART_GET_FLAG(&huart, UART_FLAG_TC) == RESET && (HAL_GetTick() - t) < 10U) { }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
}

static int rx(uint8_t *p, uint16_t n, uint32_t ms)
{
    return (HAL_UART_Receive(&huart, p, n, ms) == HAL_OK) ? 0 : -1;
}

/* ---------- xmodem ---------- */
static int xmodem(uint32_t *out_len, uint32_t *out_crc)
{
    uint8_t  c, hdr[2], data[1024], crcb[2], blk = 0, synced = 0;
    uint16_t len;
    uint32_t addr = APP_ADDR, total = 0, crc = 0xFFFFFFFFU;

    for (;;) {
        if (!synced) tx(XC);
        if (rx(&c, 1, 1000)) { synced = 0; continue; }

        if (c == EOT) { tx(ACK); break; }
        if (c == CAN) { tx(ACK); return -1; }
        if (c != SOH && c != STX) { continue; }

        len = (c == SOH) ? 128U : 1024U;
        if (rx(hdr, 2, 1000) || (uint8_t)(hdr[0] + hdr[1]) != 0xFFU) { tx(NAK); continue; }
        if (rx(data, len, 1000) || rx(crcb, 2, 1000)) { tx(NAK); continue; }
        if (((uint16_t)crcb[0] << 8 | crcb[1]) != crc16_ccitt(data, len)) { tx(NAK); synced = 1; continue; }

        if (total > 0U && hdr[0] == blk) { tx(ACK); synced = 1; continue; }
        if (hdr[0] != (uint8_t)(blk + 1U)) { tx(NAK); synced = 1; continue; }
        if (total + len > APP_MAX) { tx(CAN); return -1; }
        if (ensure_erased(addr, len) || flash_write(addr, data, len)) { tx(CAN); return -1; }

        crc = crc32_update(crc, data, len);
        addr  += len;
        total += len;
        blk = hdr[0];
        synced = 1;
        tx(ACK);
    }

    *out_len = total;
    *out_crc = ~crc;
    return (total == 0U) ? -1 : 0;
}

/* ---------- clock ---------- */
static void clock_init(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    o.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    o.HSEState       = RCC_HSE_ON;
    o.PLL.PLLState   = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) return;      /* fall back to HSI */

    c.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource   = RCC_SYSCLKSOURCE_HSE;
    c.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV1;
    c.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&c, FLASH_LATENCY_0);
}

int main(void)
{
    uint32_t len = 0, crc = 0;

    SCB->VTOR = 0x08000000U;      /* before HAL_Init: SysTick would use a stale table */
    HAL_Init();
    clock_init();
    uart_init();

    if (meta_state() != META_UPD) {
        if (meta_valid() && app_ok()) jump_app();
        /* factory image flashed by ST-Link: no metadata yet, but the app is runnable */
        if (meta_state() == META_BLANK && app_ok()) jump_app();
        if (meta_mark_upd()) for (;;) { }
    }

    if (xmodem(&len, &crc) == 0 && meta_save(len, crc) == 0 && meta_valid() && app_ok())
        NVIC_SystemReset();

    for (;;) { }
}
