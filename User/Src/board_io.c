#include "board_io.h"
#include "stm32f1xx_hal.h"
#include <string.h>
BoardDiagnostics board_diag;
static volatile uint16_t rx_head, rx_tail, tx_head, tx_tail;
static uint8_t rx_ring[1024], tx_ring[512];
void Board_Init(void) {
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_USART1_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_TIM3_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_9; gpio.Mode = GPIO_MODE_AF_PP; gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_10; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLUP; HAL_GPIO_Init(GPIOA, &gpio);
    /* 72 MHz APB2 / 115200 = 625，8N1；不占用 SWD。 */
    USART1->BRR = HAL_RCC_GetPCLK2Freq() / 115200U;
    USART1->CR2 = 0; USART1->CR3 = USART_CR3_EIE;
    USART1->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0); HAL_NVIC_EnableIRQ(USART1_IRQn);
    /* APB1=36 MHz，定时器时钟倍频到 72 MHz；1 MHz / 10000 = 100 Hz。 */
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0) timer_clock *= 2;
    TIM3->PSC = timer_clock / 1000000U - 1; TIM3->ARR = 9999;
    TIM3->EGR = TIM_EGR_UG; TIM3->SR = 0; TIM3->DIER = TIM_DIER_UIE;
    HAL_NVIC_SetPriority(TIM3_IRQn, 3, 0); HAL_NVIC_EnableIRQ(TIM3_IRQn); TIM3->CR1 = TIM_CR1_CEN;
}
void TIM3_IRQHandler(void) {
    if (TIM3->SR & TIM_SR_UIF) { TIM3->SR = (uint16_t)~TIM_SR_UIF; board_diag.timer_irqs++; }
}
void USART1_IRQHandler(void) {
    uint32_t sr = USART1->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        uint8_t b = (uint8_t)USART1->DR; /* SR/DR 顺序读取清除错误。 */
        if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) board_diag.uart_errors++;
        else {
            uint16_t next = (rx_head + 1U) & 1023U;
            if (next == rx_tail) board_diag.rx_overflow++;
            else { rx_ring[rx_head] = b; rx_head = next; }
        }
    }
    if ((sr & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if (tx_tail == tx_head) USART1->CR1 &= ~USART_CR1_TXEIE;
        else { USART1->DR = tx_ring[tx_tail]; tx_tail = (tx_tail + 1U) & 511U; }
    }
}
void Board_Poll(void) { /* 工作在中断环形缓冲区；主循环不等待串口。 */ }
int Board_ReadByte(void) {
    if (rx_head == rx_tail) return -1;
    uint8_t b = rx_ring[rx_tail]; rx_tail = (rx_tail + 1U) & 1023U; return b;
}
int Board_Send(const uint8_t *data, size_t n) {
    uint32_t primask = __get_PRIMASK(); __disable_irq();
    if (n > (size_t)((tx_tail - tx_head - 1U) & 511U)) { board_diag.tx_dropped++; __set_PRIMASK(primask); return 0; }
    for (size_t i = 0; i < n; i++) { tx_ring[tx_head] = data[i]; tx_head = (tx_head + 1U) & 511U; }
    board_diag.tx_frames++; USART1->CR1 |= USART_CR1_TXEIE; __set_PRIMASK(primask); return 1;
}
