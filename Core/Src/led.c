
#include "led.h"
#include "main.h"

/* DS0：低电平点亮，高电平熄灭 */

void LED_On(void)
{
    HAL_GPIO_WritePin(DS0_GPIO_Port, DS0_Pin, GPIO_PIN_RESET);
}

void LED_Off(void)
{
    HAL_GPIO_WritePin(DS0_GPIO_Port, DS0_Pin, GPIO_PIN_SET);
}

void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(DS0_GPIO_Port, DS0_Pin);
}
