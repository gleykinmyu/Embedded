#include "STMboard.h"
#include "Debug.h"

  void CHW_Core::SystemClock_Config()
  {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      errorMSG("OSC Config error");
    }
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
      errorMSG("RCC Config error");
    }
    /**
     * HAL_RCC_ClockConfig уже вызвал HAL_InitTick (1 ms, источник HCLK).
     * Не вызывать повторно HAL_SYSTICK_Config: SysTick_Config снова ставит
     * приоритет SysTick = 15 (минимальный); UART и др. с приоритетом 5
     * тогда вытесняют тик — HAL_GetTick/HAL_Delay «плывут».
     */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
  }

extern "C"
{
  void HAL_MspInit(void)
  {
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* System interrupt init*/
    HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
    HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
    HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
    HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
    HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
    HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
  }

  //==============================================================================================================================


  void SysTick_Handler(void)
  {
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler(); /* weak HAL_SYSTICK_Callback; не дублирует IncTick */
  }

  void NMI_Handler(void)
  {
  }

  void HardFault_Handler(void)
  {
    while (1)
    {
    }
  }

  void MemManage_Handler(void)
  {

    while (1)
    {
    }
  }

  void BusFault_Handler(void)
  {
    while (1)
    {
    }
  }

  void UsageFault_Handler(void)
  {

    while (1)
    {
    }
  }

  void SVC_Handler(void)
  {
  }

  void DebugMon_Handler(void)
  {
  }

  void PendSV_Handler(void)
  {
  }
}
