//6QSecurity
//Copyright(C) Yantai Beautiful Words Co,.Ltd.
//author:zhangshaoyan(shell.albert@gmail.com).
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l4xx_hal.h"
#include "adc.h"
#include "i2c.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include "common.h"
uint8_t gFmtBuffer[128];
uint8_t gWaktupByTamper = 0;

//define the Security RAM Table.
#define TABLE_SIZE_UUID   (100*24)
#define TABLE_DEVICE_DATA   (6480)
#define TABLE_DEVICE_STATUS (5*sizeof(uint32_t))
#define TABLE_SIZE_SSL_PRIVATE_KEY (1024*3) //3KB.
//the global RAM Table.
uint32_t gTableStatusFlag[TABLE_DEVICE_STATUS];

//Task mask bits fields.
#define MASK_TABLE_BITS           (0x1<<30|0x1<<29|0x1<<28)//111
#define MASK_ADDR_BITS            (0x1<<15|0x1<<14|0x1<<13|0x1<<12|0x1<<11|0x1<<10|0x1<<9|0x1<<8|0x1<<7|0x1<<6|0x1<<5|0x1<<4|0x1<<3|0x1<<2|0x1<<1|0x1<<0)


//Table Mask Bits.
#define MASK_TABLE_UUID           (0x0<<30|0x0<<29|0x0<<28)//000
#define MASK_TABLE_DEVICE         (0x0<<30|0x0<<29|0x1<<28)//001
#define MASK_TABLE_DEVTYPES       (0x0<<30|0x1<<29|0x0<<28)//010
#define MASK_TABLE_STATUS_FLAG    (0x0<<30|0x1<<29|0x1<<28)//011
#define MASK_TABLE_SSL_SECURITY   (0x1<<30|0x0<<29|0x0<<28)//100
//Status Flag Table.
#define OFFSET_STATUS_FLAG_TABLE_RTC_DATE   0x00000000
#define OFFSET_STATUS_FLAG_TABLE_RTC_TIME   0x00000004
#define OFFSET_STATUS_FLAG_TABLE_TAMPER     0x00000008
#define OFFSET_STATUS_FLAG_TABLE_BATTERY    0x0000000C
#define OFFSET_STATUS_FLAG_TABLE_PHYTEST    0x00000010
#define OFFSET_STATUS_FLAG_TABLE_SOFTRST    0x00000014
/* Private variables ---------------------------------------------------------*/
volatile SPIFSMState fsm = FSM_Sleep;
volatile SPITxState gTxState = Tx_Wait;
volatile uint8_t gConfigFlag = 0;
/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void gSetGPIO2SleepMode(void);

void gTable_StatusFlag_Cmd_Wr_RTCDate(uint8_t year, uint8_t month, uint8_t day)
{
	RTC_DateTypeDef sDate;
	sDate.Year = year;
	sDate.Month = month;
	sDate.Date = day;
	HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD);
#ifdef UART1_MSG
	sprintf((char*)gFmtBuffer, "setDate:%02x-%02x-%02x\r\n", year, month, day);
	HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
}
void gTable_StatusFlag_Cmd_Rd_RTCDate(uint8_t *year, uint8_t *month, uint8_t *day)
{
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	//You must call HAL_RTC_GetDate() after HAL_RTC_GetTime() to unlock the values.
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BCD);
	*year = sDate.Year;
	*month = sDate.Month;
	*day = sDate.Date;
#ifdef UART1_MSG
	sprintf((char*)gFmtBuffer, "getDate:%02x-%02x-%02x\r\n", *year, *month, *day);
	HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
}
void gTable_StatusFlag_Cmd_Wr_RTCTime(uint8_t hour, uint8_t minute, uint8_t second)
{
	RTC_TimeTypeDef sTime;
	sTime.Hours = hour;
	sTime.Minutes = minute;
	sTime.Seconds = second;
	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD);
#ifdef UART1_MSG
	sprintf((char*)gFmtBuffer, "setTime:%02x:%02x:%02x\r\n", hour, minute, second);
	HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
	//here we set config flag.
	gConfigFlag = 1;
}
void gTable_StatusFlag_Cmd_Rd_RTCTime(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	//You must call HAL_RTC_GetDate() after HAL_RTC_GetTime() to unlock the values.
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BCD);
	*hour = sTime.Hours;
	*minute = sTime.Minutes;
	*second = sTime.Seconds;
#ifdef UART1_MSG
	sprintf((char*)gFmtBuffer, "getTime:%02x:%02x:%02x\r\n", *hour, *minute, *second);
	HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
}
void gTable_StatusFlag_Cmd_Wr_Tamper(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	uint32_t tData = (uint32_t)d0 << 24 | (uint32_t)d1 << 16 | (uint32_t)d2 << 8 | (uint32_t)d3 << 0;
	uint32_t tAddr = OFFSET_STATUS_FLAG_TABLE_TAMPER / sizeof(uint32_t);
	gTableStatusFlag[tAddr] = tData;
}
void gTable_StatusFlag_Cmd_Rd_Tamper(uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3)
{
	uint32_t tAddr = OFFSET_STATUS_FLAG_TABLE_TAMPER / sizeof(uint32_t);
	uint32_t tData = gTableStatusFlag[tAddr];
	*d0 = (uint8_t)((tData >> 24) & 0xFF);
	*d1 = (uint8_t)((tData >> 16) & 0xFF);
	*d2 = (uint8_t)((tData >> 8) & 0xFF);
	*d3 = (uint8_t)((tData >> 0) & 0xFF);
}
void gTable_StatusFlag_Cmd_Wr_Battery(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	uint32_t tData = (uint32_t)d0 << 24 | (uint32_t)d1 << 16 | (uint32_t)d2 << 8 | (uint32_t)d3 << 0;
	uint32_t tAddr = OFFSET_STATUS_FLAG_TABLE_BATTERY / sizeof(uint32_t);
	gTableStatusFlag[tAddr] = tData;
}
void gTable_StatusFlag_Cmd_Rd_Battery(uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3)
{
	uint32_t tAddr = OFFSET_STATUS_FLAG_TABLE_BATTERY / sizeof(uint32_t);
	uint32_t tData = gTableStatusFlag[tAddr];
	*d0 = (uint8_t)((tData >> 24) & 0xFF);
	*d1 = (uint8_t)((tData >> 16) & 0xFF);
	*d2 = (uint8_t)((tData >> 8) & 0xFF);
	*d3 = (uint8_t)((tData >> 0) & 0xFF);
}
void gTable_StatusFlag_Cmd_Wr_PhySelf(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	uint32_t tData = (uint32_t)d0 << 24 | (uint32_t)d1 << 16 | (uint32_t)d2 << 8 | (uint32_t)d3 << 0;
	uint32_t tAddr = OFFSET_STATUS_FLAG_TABLE_PHYTEST / sizeof(uint32_t);
	gTableStatusFlag[tAddr] = tData;
}
void gTable_StatusFlag_Cmd_Wr_SoftRst(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	if (d0 == 0x55 && d1 == 0xAA && d2 == 0x55 && d3 == 0xAA)
	{
		__set_FAULTMASK(1);
		NVIC_SystemReset();
	}
}
void gTable_StatusFlag_Cmd_Rd_PhySelf(uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3)
{
	uint32_t tAddr = OFFSET_STATUS_FLAG_TABLE_PHYTEST / sizeof(uint32_t);
	uint32_t tData = gTableStatusFlag[tAddr];
	*d0 = (uint8_t)((tData >> 24) & 0xFF);
	*d1 = (uint8_t)((tData >> 16) & 0xFF);
	*d2 = (uint8_t)((tData >> 8) & 0xFF);
	*d3 = (uint8_t)((tData >> 0) & 0xFF);
}
void gTable_RrWr_Error(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3)
{
#ifdef UART1_MSG
	sprintf((char*)gFmtBuffer, "read/write error,address:%02x%02x%02x%02x\r\n", a0, a1, a2, a3);
	HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
	// if (!gSleepFlag)
	// {
	// 	//if the MCU is not configured by manual then we reset it.
	// 	__set_FAULTMASK(1);
	// 	NVIC_SystemReset();
	// }
}
/* Private function prototypes -----------------------------------------------*/
int gWriteTable(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	int ret = 0;
	uint32_t address = (uint32_t)a0 << 24 | (uint32_t)a1 << 16 | (uint32_t)a2 << 8 | (uint32_t)a3 << 0;
	uint32_t destTable = address & MASK_TABLE_BITS;
	uint32_t destAddr = address & MASK_ADDR_BITS;
	switch (destTable)
	{
	case MASK_TABLE_UUID:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	case MASK_TABLE_DEVICE:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	case MASK_TABLE_DEVTYPES:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	case MASK_TABLE_STATUS_FLAG:
		switch (destAddr)
		{
		case OFFSET_STATUS_FLAG_TABLE_RTC_DATE:
			gTable_StatusFlag_Cmd_Wr_RTCDate(d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_RTC_TIME:
			gTable_StatusFlag_Cmd_Wr_RTCTime(d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_TAMPER:
			gTable_StatusFlag_Cmd_Wr_Tamper(d0, d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_BATTERY:
			gTable_StatusFlag_Cmd_Wr_Battery(d0, d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_PHYTEST:
			gTable_StatusFlag_Cmd_Wr_PhySelf(d0, d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_SOFTRST:
			gTable_StatusFlag_Cmd_Wr_SoftRst(d0, d1, d2, d3);
			break;
		default:
			gTable_RrWr_Error(a0, a1, a2, a3);
			ret = -1;
			break;
		}
		break;
	case MASK_TABLE_SSL_SECURITY:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	default:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	}
	return ret;
}
int gReadTable(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3)
{
	int ret = 0;
	uint32_t address = (uint32_t)a0 << 24 | (uint32_t)a1 << 16 | (uint32_t)a2 << 8 | (uint32_t)a3 << 0;
	uint32_t destTable = address & MASK_TABLE_BITS;
	uint32_t destAddr = address & MASK_ADDR_BITS;
	switch (destTable)
	{
	case MASK_TABLE_UUID:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	case MASK_TABLE_DEVICE:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	case MASK_TABLE_DEVTYPES:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	case MASK_TABLE_STATUS_FLAG:
		switch (destAddr)
		{
		case OFFSET_STATUS_FLAG_TABLE_RTC_DATE:
			*d0 = 0;
			gTable_StatusFlag_Cmd_Rd_RTCDate(d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_RTC_TIME:
			*d0 = 0;
			gTable_StatusFlag_Cmd_Rd_RTCTime(d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_TAMPER:
			gTable_StatusFlag_Cmd_Rd_Tamper(d0, d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_BATTERY:
			gTable_StatusFlag_Cmd_Rd_Battery(d0, d1, d2, d3);
			break;
		case OFFSET_STATUS_FLAG_TABLE_PHYTEST:
			gTable_StatusFlag_Cmd_Rd_PhySelf(d0, d1, d2, d3);
			break;
		default:
			gTable_RrWr_Error(a0, a1, a2, a3);
			ret = -1;
			break;
		}
		break;
	case MASK_TABLE_SSL_SECURITY:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	default:
		gTable_RrWr_Error(a0, a1, a2, a3);
		ret = -1;
		break;
	}
	return ret;
}

int main(void)
{
	uint8_t txBuffer[4] = {0x19, 0x86, 0x10, 0x14};
	uint8_t rxBuffer[4];
	uint8_t regAddress[4];
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	uint64_t nSecurityFlag;
	uint8_t i;
	/* USER CODE END 1 */

	/* MCU Configuration----------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();
	MX_GPIO_Init();
	MX_RTC_Init();
	MX_SPI1_Init();
	MX_ADC1_Init();
	MX_USART1_UART_Init();
	//power on led flash.
	for (i = 0; i < 5; i++)
	{
		HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_SET);
		HAL_Delay(100);
		HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_RESET);
		HAL_Delay(100);
	}
#ifdef UART1_MSG
	//output version.
	sprintf((char*)gFmtBuffer, "\r\n\r\n6QSM V%s\r\nBuilt on %s %s\r\n", SM_VERSION, __DATE__, __TIME__);
	HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
	// while (1)
	// {
	//     //we must set all GPIOs to AnalogInput to save power.
	//     gSetGPIO2SleepMode();
	//     HAL_SuspendTick();
	//     HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
	//     HAL_ResumeTick();
	//     //after wakeup,we must re-initial GPIOs.
	// }

	//initial RTC to default value.
	sDate.Year = 0x17;
	sDate.Month = 0x09;
	sDate.Date = 0x01;
	sTime.Hours = 0x0;
	sTime.Minutes = 0x0;
	sTime.Seconds = 0x0;
	HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD);
	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD);

	//check the security flag in Flash.
	// nSecurityFlag = *(__IO uint64_t*)(FLASH_PAGE_127_2K);
	// if (0x1987090119861014 != nSecurityFlag)
	// {
	//       //flash the LED till power is used off.
	//       while (1)
	//       {
	//         HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_SET);
	//         HAL_Delay(100);
	//         HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_RESET);
	//         HAL_Delay(100);
	//       }
	// }

	/* Infinite loop */
	while (1)
	{
		if (!gConfigFlag)
		{
			//the MCU is not configured,so turn on LED1.
			HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_SET);
		} else {
			//the MCU is configured,so turn off LED1.
			HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_RESET);
		}
		if (fsm == FSM_Sleep)
		{
			//reset FSM & then sleep.
			fsm = FSM_Initial;
			//enter sleep mode or not.
			if (1/*gConfigFlag*/)
			{
#ifdef UART1_MSG
				sprintf((char*)gFmtBuffer, "%s\r\n", "Enter Stop2 Mode.");
				HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
				//close all GPIOs to analog input to reduce power.
				//except TamperDetectPin & STM_RSVPin (to wakeup STM32 from Stop2Mode)
				HAL_SPI_DeInit(&hspi1);
				HAL_ADC_DeInit(&hadc1);
				HAL_UART_DeInit(&huart1);
				gSetGPIO2SleepMode();
				HAL_SuspendTick();
				HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
				HAL_ResumeTick();

				SystemClock_Config();
				MX_GPIO_Init();
				MX_ADC1_Init();
				MX_RTC_Init();
				MX_SPI1_Init();
				MX_USART1_UART_Init();
			}

			//if I'm waken up by Tamper.
			if (gWaktupByTamper)
			{
				//reset flag.
				gWaktupByTamper = 0;
				//clear RAM Tables.
				//clear Security Flag in Flash & reset.
				// __set_FAULTMASK(1);
				// NVIC_SystemReset();
				//flash the LED till power is used off.
				while (1)
				{
					HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_SET);
					HAL_Delay(100);
					HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_RESET);
					HAL_Delay(100);
				}
			}
		}

		//led on to indicate power on.
		HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_SET);

		//reset state flag.
		gTxState = Tx_Wait;

		//switch (HAL_SPI_TransmitReceive(&hspi1, txBuffer, rxBuffer, sizeof(rxBuffer), 1000 * 60))
		if (HAL_SPI_TransmitReceive_IT(&hspi1, txBuffer, rxBuffer, sizeof(rxBuffer)) != HAL_OK)
		{
#ifdef UART1_MSG
			sprintf((char*)gFmtBuffer, "%s\r\n", "SPI TxRx Error,Reset.");
			HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
			fsm = FSM_Sleep;//reset fsm.
			continue;
		}
		//wait for tx finished.
		while (gTxState == Tx_Wait);

		//prase out the tx result.
		switch (gTxState)
		{
		case Tx_Finish:
#ifdef UART1_MSG
			sprintf((char*)gFmtBuffer, "R:%02x%02x%02x%02x,FSM=%d\r\n", rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3], fsm);
			HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
			switch (fsm)
			{
			case FSM_Initial:
				if (rxBuffer[0] & 0x80)
				{
					//write.
					regAddress[0] = rxBuffer[0] & (~0x80);
					regAddress[1] = rxBuffer[1];
					regAddress[2] = rxBuffer[2];
					regAddress[3] = rxBuffer[3];
					//change to next state.
					fsm = FSM_Write;
				} else {
					//for read operation,we prepare data here.
					//wait for Master next clock to shift out.
					regAddress[0] = rxBuffer[0] & (~0x80);
					regAddress[1] = rxBuffer[1];
					regAddress[2] = rxBuffer[2];
					regAddress[3] = rxBuffer[3];
					//the MSB indicates read/write,so we dispass it.
					if (gReadTable(regAddress[0], regAddress[1], regAddress[2], regAddress[3], &txBuffer[0], &txBuffer[1], &txBuffer[2], &txBuffer[3]) < 0)
					{
						fsm = FSM_Sleep;
					} else {
						//change to next state.
						fsm = FSM_Read;
					}

#ifdef UART1_MSG
					sprintf((char*)gFmtBuffer, "R:%02x%02x%02x%02x=%02x%02x%02x%02x\r\n", ///<
					        regAddress[0], regAddress[1], regAddress[2], regAddress[3], txBuffer[0], txBuffer[1], txBuffer[2], txBuffer[3]);
					HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
				}
				break;
			case FSM_Write:
				gWriteTable(regAddress[0], regAddress[1], regAddress[2], regAddress[3], rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3]);
				//change to next state.
				fsm = FSM_Sleep;
#ifdef UART1_MSG
				sprintf((char*)gFmtBuffer, "W:%02x%02x%02x%02x=%02x%02x%02x%02x\r\n", ///<
				        regAddress[0], regAddress[1], regAddress[2], regAddress[3], rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3]);
				HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
#endif
				break;
			case FSM_Read:
				//change to next state.
				fsm = FSM_Sleep;
				break;
			default:
				break;
			}
			break;
		case Tx_Error:
// #ifdef UART1_MSG
//       sprintf((char*)gFmtBuffer, "%s\r\n", "SPI TxRx Error,Reset.");
//       HAL_UART_Transmit(&huart1, (uint8_t*)gFmtBuffer, strlen((const char*)gFmtBuffer), USART1_TX_TIMEOUT);
// #endif
//       //reset fsm.
//       gTxState = Tx_Wait;
//       fsm = FSM_Initial;
			break;
		default:
			break;
		}

		HAL_GPIO_WritePin(GPIOB, STM_LED1_Pin, GPIO_PIN_RESET);
	}
	/* USER CODE END 3 */
}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInit;

	/**Configure LSE Drive Capability
	*/
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
	/**Initializes the CPU, AHB and APB busses clocks
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	/**Initializes the CPU, AHB and APB busses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
	                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC | RCC_PERIPHCLK_USART1
	                                     | RCC_PERIPHCLK_ADC;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
	PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
	PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_MSI;
	PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
	PeriphClkInit.PLLSAI1.PLLSAI1N = 16;
	PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
	PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
	PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
	PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_ADC1CLK;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	/**Configure the main internal regulator output voltage
	*/
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	/**Configure the Systick interrupt time
	*/
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

	/**Configure the Systick
	*/
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

	/**Enable MSI Auto calibration
	*/
	HAL_RCCEx_EnableMSIPLLMode();
	/* SysTick_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */
/**
  * @brief  TxRx Transfer completed callback.
  * @param  hspi: SPI handle
  * @note   This example shows a simple way to report end of Interrupt TxRx transfer, and
  *         you can add your own implementation.
  * @retval None
  */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	gTxState = Tx_Finish;
}
/**
  * @brief  SPI error callbacks.
  * @param  hspi: SPI handle
  * @note   This example shows a simple way to report transfer error, and you can
  *         add your own implementation.
  * @retval None
  */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	gTxState = Tx_Error;
}
/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	  ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */

}

#endif


void gSetGPIO2SleepMode(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pins : PC13 PC14 PC15 PC0
	                         PC1 PC2 PC3 PC4
	                         PC5 PC6 PC7 PC8
	                         PC9 PC10 PC11 PC12 */
	GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0
	                      | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4
	                      | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8
	                      | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA0 **PA1** PA2 PA3
	                         PA4 PA5 PA6 PA7
	                         PA8 PA9 PA10 PA11
	                         PA12 PA13 PA14 PA15 */
	//PA1 is used to wakeup STM32 from iMX6Q.
	GPIO_InitStruct.Pin = GPIO_PIN_0 | /*GPIO_PIN_1 |*/ GPIO_PIN_2 | GPIO_PIN_3
	                      | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7
	                      | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
	                      | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB0 PB1 PB2 PB10
	                         PB11 PB12 PB13 PB14
	                         PB15 PB3 PB4 PB5
	                         PB6 **PB7** PB8 PB9 */ //PB7 is tamper detect,so remove it here.
	//PB12 is for STM_LED1.
	if (!gConfigFlag)
	{
		//the MCU is not configured,so keep LED1 on.
		GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10
		                      | GPIO_PIN_11 | /*GPIO_PIN_12 |*/ GPIO_PIN_13 | GPIO_PIN_14
		                      | GPIO_PIN_15 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5
		                      | GPIO_PIN_6 | /*GPIO_PIN_7 |*/ GPIO_PIN_8 | GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	} else {
		//the MCU is configured,so keep LED1 off.
		GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10
		                      | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
		                      | GPIO_PIN_15 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5
		                      | GPIO_PIN_6 | /*GPIO_PIN_7 |*/ GPIO_PIN_8 | GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	}

	/*Configure GPIO pin : PD2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pin : PH3 */
	GPIO_InitStruct.Pin = GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	__HAL_RCC_GPIOC_CLK_DISABLE();
	__HAL_RCC_GPIOH_CLK_DISABLE();
	__HAL_RCC_GPIOA_CLK_DISABLE();
	__HAL_RCC_GPIOB_CLK_DISABLE();
	__HAL_RCC_GPIOD_CLK_DISABLE();

	HAL_ADC_DeInit(&hadc1);
	HAL_SPI_DeInit(&hspi1);
	HAL_UART_DeInit(&huart1);
	//DeInit RTC will set all data&time to zero,so here disable it.
	//HAL_RTC_DeInit(&hrtc);
}
/**
  * @}
  */
/**
  * @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
