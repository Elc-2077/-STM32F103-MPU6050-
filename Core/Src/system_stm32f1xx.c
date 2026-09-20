/**
  ******************************************************************************
  * @file    system_stm32f1xx.c
  * @author  MCD Application Team
  * @brief   CMSIS Cortex-M3 Device Peripheral Access Layer System Source File.
  ******************************************************************************
  */

#include "stm32f1xx.h"

#if !defined  (HSE_VALUE)
  #define HSE_VALUE    8000000U  /*!< Default value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined  (HSI_VALUE)
  #define HSI_VALUE    8000000U  /*!< Default value of the Internal oscillator in Hz */
#endif /* HSI_VALUE */

/* Clock prescaler tables */
const uint8_t AHBPrescTable[16U] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8U] =  {0, 0, 0, 0, 1, 2, 3, 4};

/* System Clock */
uint32_t SystemCoreClock = 8000000U;

/**
  * @brief  Setup the microcontroller system
  *         Initialize the Embedded Flash Interface, the PLL and update the
  *         SystemCoreClock variable.
  * @param  None
  * @retval None
  */
void SystemInit(void)
{
  /* Reset the RCC clock configuration to the default reset state */
  /* Set HSION bit */
  RCC->CR |= 0x00000001U;

  /* Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits */
  RCC->CFGR &= 0xF8FF0000U;

  /* Reset HSEON, CSSON and PLLON bits */
  RCC->CR &= 0xFEF6FFFFU;

  /* Reset HSEBYP bit */
  RCC->CR &= 0xFFFBFFFFU;

  /* Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits */
  RCC->CFGR &= 0xFF80FFFFU;

  /* Disable all interrupts and clear pending bits  */
  RCC->CIR = 0x009F0000U;
}

/**
  * @brief  Update SystemCoreClock variable according to Clock Register Values.
  *         The SystemCoreClock variable contains the core clock (HCLK)
  * @param  None
  * @retval None
  */
void SystemCoreClockUpdate(void)
{
  uint32_t tmp = 0U, pllmull = 0U, pllsource = 0U;

  /* Get SYSCLK source */
  tmp = RCC->CFGR & RCC_CFGR_SWS;

  switch (tmp)
  {
    case 0x00U:  /* HSI used as system clock */
      SystemCoreClock = HSI_VALUE;
      break;
    case 0x04U:  /* HSE used as system clock */
      SystemCoreClock = HSE_VALUE;
      break;
    case 0x08U:  /* PLL used as system clock */
      /* Get PLL clock source and multiplication factor */
      pllmull = RCC->CFGR & RCC_CFGR_PLLMULL;
      pllsource = RCC->CFGR & RCC_CFGR_PLLSRC;

      pllmull = ( pllmull >> 18U) + 2U;

      if (pllsource == 0x00U)
      {
        /* HSI oscillator clock divided by 2 selected as PLL clock entry */
        SystemCoreClock = (HSI_VALUE >> 1U) * pllmull;
      }
      else
      {
        /* HSE selected as PLL clock entry */
        if ((RCC->CFGR & RCC_CFGR_PLLXTPRE) != (uint32_t)RESET)
        {/* HSE oscillator clock divided by 2 */
          SystemCoreClock = (HSE_VALUE >> 1U) * pllmull;
        }
        else
        {
          SystemCoreClock = HSE_VALUE * pllmull;
        }
      }
      break;

    default:
      SystemCoreClock = HSI_VALUE;
      break;
  }

  /* Compute HCLK clock frequency */
  /* Get HCLK prescaler */
  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4U)];
  /* HCLK clock frequency */
  SystemCoreClock >>= tmp;
}
