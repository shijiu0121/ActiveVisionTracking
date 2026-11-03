#ifndef AVT_TEST_HAL_H
#define AVT_TEST_HAL_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);
void HAL_GPIO_TogglePin(void *port, uint16_t pin);
#endif
