#include "uart.h"

//============================================================================================================================
//Class function implementation
//============================================================================================================================

void CHW_UART_Core::Begin(uint32_t BaudRate)
{
  _hndl.Init.BaudRate = BaudRate;
  if (HAL_UART_Init(&_hndl) != HAL_OK)
  {
    errorMSG("HAL UART Init not success");
  }
}

CHW_Status CHW_UART_Core::Write(const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  return HAL_UART_Transmit(&_hndl, pData, Size, Timeout);
}

CHW_Status CHW_UART_Core::Read(uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  return HAL_UART_Receive(&_hndl, pData, Size, Timeout);
}

CHW_Status CHW_UART_Core::Write_IT(const uint8_t *pData, uint16_t Size)
{
  return HAL_UART_Transmit_IT(&_hndl, pData, Size);
}

CHW_Status CHW_UART_Core::Read_IT(uint8_t *pData, uint16_t Size)
{
  return HAL_UART_Receive_IT(&_hndl, pData, Size);
}

unsigned CHW_UART_Core::_GetID(USART_TypeDef *u)
{
  if (u == USART1) return 0;
  if (u == USART2) return 1;
  if (u == USART3) return 2;
  if (u == UART4)  return 3;
  if (u == UART5)  return 4;
  if (u == USART6) return 5;
  return 6;
}

//============================================================================================================================
//HAL Library functions
//============================================================================================================================

extern "C" void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{  
  //CHW_UART_Core::_ThisPtr[CHW_UART_Core::_GetID(huart->Instance)]->MSP_Init(huart);
}

extern "C" int _write(int file, char *ptr, int len) {
   (void) file; 
   HAL_UART_Transmit(HNDL::uart[PHL_UART_DEBUG], (uint8_t*)ptr, len, 100);
   return len;
}



//============================================================================================================================
//INTERRUPTS
//============================================================================================================================

extern "C" void USART1_IRQHandler(void) 
{
  HAL_UART_IRQHandler(HNDL::uart[PHL::uart1]);
}

extern "C" void USART2_IRQHandler(void) 
{
  HAL_UART_IRQHandler(HNDL::uart[PHL::uart2]);
}

extern "C" void USART3_IRQHandler(void) 
{
  HAL_UART_IRQHandler(HNDL::uart[PHL::uart3]);
}

extern "C" void UART4_IRQHandler(void) 
{
  HAL_UART_IRQHandler(HNDL::uart[PHL::uart4]);
}

extern "C" void UART5_IRQHandler(void) 
{
  HAL_UART_IRQHandler(HNDL::uart[PHL::uart5]);
}

extern "C" void USART6_IRQHandler(void) 
{
  HAL_UART_IRQHandler(HNDL::uart[PHL::uart6]);
}