#ifndef __COMMON_H__
#define __COMMON_H__
#define SM_VERSION  "0.1"  //2017/07/26.
#define USART1_TX_TIMEOUT 200
#define UART1_MSG   1
#define FLASH_PAGE_127_2K   0x0803F800

typedef enum {
  FSM_Initial = 0,
  FSM_Write = 1,
  FSM_Read = 2,
  FSM_Sleep = 3,
} SPIFSMState;

typedef enum {
  Tx_Wait = 0,
  Tx_Finish = 1,
  Tx_Error = 2,
} SPITxState;

extern uint8_t gWaktupByTamper;
extern uint8_t gFmtBuffer[128];
#endif //__COMMON_H__
