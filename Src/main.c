/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2018 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f7xx_hal.h"
#include "dma.h"
#include "i2c.h"
#include "sai.h"
#include "spi.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "Si468x/Si468x.h"
#include "Si468x/Si468x_FM.h"
#include "Si468x/Si468x_DAB.h"
#include "AR1010.h"
#include "SST25V_flash.h"
#include "core_cm7.h"
#include "string.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
#define DWT_CTRL        (*(volatile uint32_t *)0xE0001000)
#define CYCCNTENA       (1<<0)
#define DWT_CYCCNT      ((volatile uint32_t *)0xE0001004)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
static uint8_t I2C_TxDMAComplete = 0;
static uint8_t I2C_RxDMAComplete = 0;

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	I2C_TxDMAComplete = 1;
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	I2C_RxDMAComplete = 1;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	Error_Handler();
}

void I2C_write(uint8_t address, uint8_t *data, uint16_t size)
{
	SCB_CleanDCache_by_Addr((void *) data, size);
	I2C_TxDMAComplete = 0;
	if (HAL_I2C_Master_Transmit_DMA(&hi2c1, address << 1, data, size) != HAL_OK)
		Error_Handler();
	while (!I2C_TxDMAComplete);
}

void I2C_read(uint8_t address, uint8_t *read_buffer, uint16_t size)
{
	SCB_CleanInvalidateDCache_by_Addr((void *) read_buffer, size);
	I2C_RxDMAComplete = 0;
	if (HAL_I2C_Master_Receive_DMA(&hi2c1, address << 1, read_buffer, size) != HAL_OK)
		Error_Handler();
	while (!I2C_RxDMAComplete);
}

void flash_SPI_write(uint8_t *data, uint16_t size)
{
	HAL_SPI_Transmit(&hspi5, data, size, 1000);
}

void flash_SPI_read(uint8_t *read_buffer, uint16_t size)
{
	HAL_SPI_Receive(&hspi5, read_buffer, size, 1000);
}

uint8_t flash_SPI_read_write_byte(uint8_t data)
{
	uint8_t received;
	HAL_SPI_TransmitReceive(&hspi5, &data, &received, 1, 1000);
	return received;
}

void flash_CS_pin(uint8_t set)
{
	HAL_GPIO_WritePin(FLASH_SS_GPIO_Port, FLASH_SS_Pin, set);
}

uint8_t dab_change_service = 1;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == SI_INT_Pin)
    	si468x_interrupt();
    if (GPIO_Pin == USER_Btn_Pin)
    	dab_change_service = 1;
}

uint8_t response_buffer[5][200];
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* Enable I-Cache-------------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache-------------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable DWT */
  DWT->LAR = 0xC5ACCE55; // Unlock register access
  *DWT_CYCCNT = 0;
  DWT_CTRL |= CYCCNTENA; // Enable CPU cycle counter
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_SAI2_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();

  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(ESP32_SS_GPIO_Port, ESP32_SS_Pin, GPIO_PIN_RESET); // Disable ESP32 SPI listening

  si468x_init(Si468x_MODE_DAB);

  uint8_t *current_uuid = (uint8_t *) "9f38baeb-223d-4ed3-b4d1-b427a454487a";

  uint8_t read_uuid[36];
  //SST25_read(0, read_uuid, 36);

  //if (strncmp((char *) current_uuid, (char *) read_uuid, 36)) // If flash data either not written or wrong version
  {
	  //SST25_sector_erase_4K(0);
	  //SST25_write(0, current_uuid, 36);

	  si468x_DAB_band_scan();
  }

  uint16_t num_services = 0, current_service_id = 0;
  //SST25_read(4096, (uint8_t *) &num_services, 2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint8_t text_index = 0;
  while (1)
  {
	  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
	  HAL_Delay(100);

	  if (dab_change_service)
	  {
		  si468x_DAB_tune_service(current_service_id++);
		  dab_change_service = 0;
	  }
	  if (current_service_id >= num_services)
		  current_service_id = 0;

	  if (Interrupt_Status.DSRVINT)
	  {
		  uint16_t response_size = 0;
		  si468x_DAB_get_digital_service_data(response_buffer[text_index++], &response_size, 0);
		  Interrupt_Status.DSRVINT = 0;

		  if (text_index >= 5)
			  text_index = 0;
	  }

//	  si468x_FM_tune(90.3); // BBC R3
//	  si468x_FM_tune(92.52); // BBC R4
//	  while (1)
//		  si468x_FM_RDS_status();
//	  si468x_FM_tune(96.4); // EAGLE
//	  si468x_FM_tune(97.7); // Radio 1
//	  si468x_FM_tune(88.1); // BBC R2

  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    /**Configure the main internal regulator output voltage 
    */
  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Activate the Over-Drive mode 
    */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI2|RCC_PERIPHCLK_I2C1;
  PeriphClkInitStruct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PIN;
  PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
	  HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
 	  HAL_Delay(100);
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

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
