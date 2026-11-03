#ifndef BOARD_IO_H
#define BOARD_IO_H
#include <stdint.h>
#include <stddef.h>
typedef struct { volatile uint32_t timer_irqs, rx_overflow, uart_errors, tx_frames, tx_dropped; } BoardDiagnostics;
extern BoardDiagnostics board_diag;
void Board_Init(void);
void Board_Poll(void);
int Board_ReadByte(void);
int Board_Send(const uint8_t *data, size_t n);
#endif
