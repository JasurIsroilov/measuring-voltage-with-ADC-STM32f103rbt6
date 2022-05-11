#ifndef __MAIN_H
#define	__MAIN_H

#include "stm32f1xx.h"
#include "stdbool.h"
#include "string.h"
#include "stdio.h"
#include "math.h"
#include "stdlib.h"

/* Размеры буферов приёма и передачи */
#define	RX_BUFF_SIZE	256
#define TX_BUFF_SIZE	256

/* Прототипы функций */
void initADC1_Regular(void);
void Execute_Command (void);
void initUSART1(void);
void txStr(char *str);
void initClk(void);
void initTIM2(void);
void delay(uint32_t takts);
uint16_t Read_ADC(uint8_t n);

#endif
