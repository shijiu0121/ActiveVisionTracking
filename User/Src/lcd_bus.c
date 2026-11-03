#include "lcd_bus.h"
#include "stm32f1xx_hal.h"
/* 战舰 LCD：FSMC_NE4/PG12、A10/PG0，16 位地址线右移，A10 对应 CPU 地址 bit11。 */
#define LCD_COMMAND (*(volatile uint16_t *)0x6c000000UL)
#define LCD_DATA (*(volatile uint16_t *)0x6c000800UL)
void LcdBus_Init(void) {
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOD_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE(); __HAL_RCC_GPIOG_CLK_ENABLE(); __HAL_RCC_FSMC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_AF_PP; gpio.Speed = GPIO_SPEED_FREQ_HIGH; gpio.Pull = GPIO_NOPULL;
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);
    gpio.Pin = 0xff80U; HAL_GPIO_Init(GPIOE, &gpio);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_12; HAL_GPIO_Init(GPIOG, &gpio);
    /* Bank1 NOR/SRAM4，异步 SRAM 16 bit、读写独立。只设置块4，不触碰其他外部存储片选。 */
    FSMC_Bank1->BTCR[6] = FSMC_BCRx_MWID_0 | FSMC_BCRx_WREN | FSMC_BCRx_EXTMOD;
    FSMC_Bank1->BTCR[7] = 5U | (24U << 8); /* 读：地址 6HCLK，数据 25HCLK。 */
    FSMC_Bank1E->BWTR[6] = 3U | (8U << 8); /* 写：地址 4HCLK，数据 9HCLK。 */
    FSMC_Bank1->BTCR[6] |= FSMC_BCRx_MBKEN;
}
void LcdBus_Command(uint16_t value) { LCD_COMMAND = value; }
void LcdBus_Data(uint16_t value) { LCD_DATA = value; }
uint16_t LcdBus_Read(void) { return LCD_DATA; }
