
#include "main.h"

char TxBuffer[TX_BUFF_SIZE];					//Буфер передачи USART
uint16_t adc_res = 0;
uint16_t adc_max = 0;
float adc_volt = 0;

uint16_t Read_ADC(uint8_t n)
{
	ADC1->SQR3 = n;									//Записываем номер канала в регистр SQR3
	ADC1->CR2 |= ADC_CR2_SWSTART;					//Запускаем преобразование в регулярном канале
	while(!(ADC1->SR & ADC_SR_EOC));					//Ждем окончания преобразования
	return ADC1->DR;								//Читаем результат
}

void TIM2_IRQHandler(void)
{
	TIM2->SR &= ~TIM_SR_UIF;					//Сброс флага переполнения
	uint16_t tmp = Read_ADC(14);
	if(adc_max < tmp)
	{
		adc_max = tmp;
		Execute_Command();
	}
}

void initTIM2(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;			//Включить тактирование TIM6

	//Частота APB1 для таймеров = APB1Clk * 2 = 32МГц * 2 = 64МГц
	TIM2->PSC = 64-1;						//Предделитель частоты (64МГц/64 = 1000кГц)
	TIM2->ARR = 1000-1;							//Модуль счёта таймера (1000кГц/1000 = 1)
	TIM2->DIER |= TIM_DIER_UIE;					//Разрешить прерывание по переполнению таймера
	TIM2->CR1 |= TIM_CR1_CEN;					//Включить таймер

	NVIC_EnableIRQ(TIM2_IRQn);					//Рарзрешить прерывание от TIM2
	NVIC_SetPriority(TIM2_IRQn, 1);				//Выставляем приоритет
}

void initClk(void)
{
	// Enable HSI
	RCC->CR |= RCC_CR_HSION;
	while(!(RCC->CR & RCC_CR_HSIRDY)){};

	// Enable Prefetch Buffer
	FLASH->ACR |= FLASH_ACR_PRFTBE;

	// Flash 2 wait state
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_2;

	// HCLK = SYSCLK
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

	// PCLK2 = HCLK
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

	// PCLK1 = HCLK/2
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

	// PLL configuration: PLLCLK = HSI/2 * 16 = 64 MHz
	RCC->CFGR &= ~RCC_CFGR_PLLSRC;
	RCC->CFGR |= RCC_CFGR_PLLMULL16;

	// Enable PLL
	RCC->CR |= RCC_CR_PLLON;

	// Wait till PLL is ready
	while((RCC->CR & RCC_CR_PLLRDY) == 0) {};

	// Select PLL as system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	// Wait till PLL is used as system clock source
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL){};
}

void initUART1(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;						//включить тактирование альтернативных ф-ций портов
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;					//включить тактирование UART1

	GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);		//PA9 на выход
	GPIOA->CRH |= (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1);

	GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);		//PA10 - вход
	GPIOA->CRH |= GPIO_CRH_CNF10_0;

	/*****************************************
	Скорость передачи данных - 19200
	Частота шины APB2 - 64МГц

	1. USARTDIV = 64'000'000/(16*19200) = 208,3
	2. 208 = 0xD0
	3. 16*0.3 = 5
	4. Итого 0xD05
	*****************************************/
	USART1->BRR = 0xD05;

	USART1->CR1 |= USART_CR1_RE | USART_CR1_TE | USART_CR1_UE;
	USART1->CR1 |= USART_CR1_RXNEIE;						//разрешить прерывание по приему байта данных

	NVIC_EnableIRQ(USART1_IRQn);
}


void initADC1_Regular(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;					//Включить тактирование порта GPIOC

	GPIOC->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);	//PC4 на вход

	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;  				//Включить тактирование АЦП

	//Настройка времени преобразования каналов
	ADC1->SMPR1 |= ADC_SMPR1_SMP14;						//Канал 14 - 239.5 тактов
	//ADC1->SMPR1 |= ADC_SMPR1_SMP16;						//Канал 14 - 239.5 тактов

	ADC1->CR2 |= ADC_CR2_TSVREFE;						//Подключить термодатчик к каналу ADC1_IN16
	ADC1->CR2 |= ADC_CR2_EXTSEL;       					//Выбрать в качестве источника запуска SWSTART
	ADC1->CR2 |= ADC_CR2_EXTTRIG;      					//Разрешить внешний запуск регулярного канала
	ADC1->CR2 |= ADC_CR2_ADON;         					//Включить АЦП

	delay(10);											//Задержка перед калибровкой
	ADC1->CR2 |= ADC_CR2_CAL;							//Запуск калибровки
	while (!(ADC1->CR2 & ADC_CR2_CAL)){};	 			//Ожидание окончания калибровки
}

void txStr(char *str)
{
	uint16_t i;
	for (i = 0; i < strlen(str); i++)
	{
		USART1->DR = str[i];								//передаём байт данных
		while ((USART1->SR & USART_SR_TC)==0) {};			//ждём окончания передачи
	}
}


void Execute_Command (void)
{
	memset(TxBuffer,0,sizeof(TxBuffer));
	adc_res = Read_ADC(14);
	adc_volt = 3.3/4095*adc_max;
	sprintf(TxBuffer, "%d   %1.2fV\r", adc_max, adc_volt);
	txStr(TxBuffer);
}

int main(void)
{
	initClk();
	initUART1();
	initADC1_Regular();
	//initTIM2();
	while(true)
	{
		uint16_t tmp = Read_ADC(14);
			if(adc_max < tmp)
			{
				adc_max = tmp;
				Execute_Command();
			}
			delay(100000);
	};
}

void delay(uint32_t takts)
{
	volatile uint32_t i;
	for (i = 0; i < takts; i++) {};
}
