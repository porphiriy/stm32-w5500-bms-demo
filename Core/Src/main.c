/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "wizchip_conf.h"
#include "w5500/w5500.h"
#include "socket.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

uint8_t rxByte;
char uartBuf[128];
volatile uint16_t uartPos = 0;

char uartText[128] = "No UART data";

volatile uint8_t w5500_version = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t W5500_ReadVersion(void)
{
    uint8_t tx[3];
    uint8_t version = 0;

    tx[0] = 0x00;
    tx[1] = 0x39;
    tx[2] = 0x00;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

    HAL_SPI_Transmit(&hspi3, tx, 3, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi3, &version, 1, HAL_MAX_DELAY);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);

    return version;
}

static void W5500_Select(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port,
                      W5500_CS_Pin,
                      GPIO_PIN_RESET);
}

static void W5500_Deselect(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port,
                      W5500_CS_Pin,
                      GPIO_PIN_SET);
}

static uint8_t W5500_ReadByte(void)
{
    uint8_t tx = 0xFF;
    uint8_t rx = 0;

    HAL_SPI_TransmitReceive(&hspi3,
                           &tx,
                           &rx,
                           1,
                           HAL_MAX_DELAY);

    return rx;
}

static void W5500_WriteByte(uint8_t byte)
{
    HAL_SPI_Transmit(&hspi3,
                     &byte,
                     1,
                     HAL_MAX_DELAY);
}

static void W5500_Init(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port,
                      W5500_CS_Pin,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(W5500_RST_GPIO_Port,
                      W5500_RST_Pin,
                      GPIO_PIN_RESET);

    HAL_Delay(10);

    HAL_GPIO_WritePin(W5500_RST_GPIO_Port,
                      W5500_RST_Pin,
                      GPIO_PIN_SET);

    HAL_Delay(100);

    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Deselect);
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);

    uint8_t tx_size[8] = {4,2,2,2,2,2,2,0};
    uint8_t rx_size[8] = {2,2,2,2,2,2,2,2};

    wizchip_init(tx_size, rx_size);

    wiz_NetInfo netInfo = {
        .mac  = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56},
        .ip   = {192, 168, 1, 177},
        .sn   = {255, 255, 255, 0},
        .gw   = {192, 168, 1, 1},
        .dns  = {8, 8, 8, 8},
        .dhcp = NETINFO_STATIC
    };

    wizchip_setnetinfo(&netInfo);
}

static uint8_t rx_buffer[512];

static const char web_page[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>BUREVII BMS</title>"

"<style>"
"body{margin:0;background:#111827;color:#e5e7eb;font-family:Arial,sans-serif;}"
".wrap{max-width:900px;margin:40px auto;padding:20px;}"
"h1{margin-bottom:5px;}"
".sub{color:#9ca3af;margin-bottom:25px;}"
".dot{display:inline-block;width:10px;height:10px;border-radius:50%;"
"background:#22c55e;margin-right:8px;}"

".card{background:#1f2937;border-radius:14px;padding:22px;margin-bottom:16px;"
"box-shadow:0 4px 15px rgba(0,0,0,.25);}"

".label{font-size:14px;color:#9ca3af;}"
".voltage{font-size:48px;font-weight:bold;margin-top:8px;}"
".unit{font-size:24px;color:#9ca3af;}"

"canvas{width:100%;height:300px;display:block;margin-top:20px;}"
"#time{font-size:13px;color:#9ca3af;margin-top:10px;}"
"</style>"
"</head>"

"<body>"
"<div class='wrap'>"

"<h1>BUREVII</h1>"
"<div class='sub'><span class='dot'></span>BMS monitor online</div>"

"<div class='card'>"
"<div class='label'>BATTERY VOLTAGE</div>"
"<div class='voltage'>"
"<span id='voltage'>--.--</span>"
"<span class='unit'> V</span>"
"</div>"
"<div id='time'></div>"
"</div>"

"<div class='card'>"
"<div class='label'>VOLTAGE HISTORY</div>"
"<canvas id='chart'></canvas>"
"</div>"

"</div>"

"<script>"

"const canvas=document.getElementById('chart');"
"const ctx=canvas.getContext('2d');"

"let values=[];"
"const maxPoints=60;"

"function drawChart(){"

" canvas.width=canvas.clientWidth;"
" canvas.height=300;"

" const w=canvas.width;"
" const h=canvas.height;"

" const minV=48;"
" const maxV=54;"

" ctx.clearRect(0,0,w,h);"

" ctx.strokeStyle='#374151';"
" ctx.lineWidth=1;"

" for(let i=0;i<=6;i++){"
"   let y=i*h/6;"
"   ctx.beginPath();"
"   ctx.moveTo(0,y);"
"   ctx.lineTo(w,y);"
"   ctx.stroke();"
" }"

" if(values.length<2)return;"

" ctx.strokeStyle='#22c55e';"
" ctx.lineWidth=3;"
" ctx.beginPath();"

" values.forEach((v,i)=>{"

"   const x=i*(w/(maxPoints-1));"
"   const y=h-(v-minV)/(maxV-minV)*h;"

"   if(i===0)ctx.moveTo(x,y);"
"   else ctx.lineTo(x,y);"

" });"

" ctx.stroke();"
"}"

"async function updateData(){"

" try{"

"   const response=await fetch('/data',{cache:'no-store'});"
"   const text=await response.text();"

"   if(text.startsWith('V:')){"

"     const v=parseFloat(text.substring(2));"

"     if(!isNaN(v)){"

"       document.getElementById('voltage').textContent=v.toFixed(2);"

"       values.push(v);"

"       if(values.length>maxPoints)"
"         values.shift();"

"       drawChart();"

"       document.getElementById('time').textContent="
"         'Updated: '+new Date().toLocaleTimeString();"
"     }"
"   }"

" }catch(e){}"
"}"

"updateData();"
"setInterval(updateData,500);"

"</script>"

"</body>"
"</html>";

static void WebServer_Run(void)
{
    uint8_t sn = 0;

    switch (getSn_SR(sn))
    {
        case SOCK_CLOSED:
            socket(sn, Sn_MR_TCP, 80, 0);
            break;

        case SOCK_INIT:
            listen(sn);
            break;

        case SOCK_ESTABLISHED:
        {
            uint16_t len = getSn_RX_RSR(sn);

            if (len > 0)
            {
                if (len >= sizeof(rx_buffer))
                    len = sizeof(rx_buffer) - 1;

                int32_t received = recv(sn, rx_buffer, len);

                if (received > 0)
                {
                    rx_buffer[received] = '\0';

                    /* запит актуальних UART-даних */
                    if (strncmp((char *)rx_buffer, "GET /data ", 10) == 0)
                    {
                        char response[256];

                        int n = snprintf(
                            response,
                            sizeof(response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Cache-Control: no-cache, no-store\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            uartText
                        );

                        send(sn, (uint8_t *)response, n);
                    }

                    /* основна сторінка */
                    else
                    {
                        static const char header[] =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Cache-Control: no-cache, no-store\r\n"
                            "Connection: close\r\n"
                            "\r\n";

                        send(sn,
                             (uint8_t *)header,
                             strlen(header));

                        send(sn,
                             (uint8_t *)web_page,
                             strlen(web_page));
                    }

                    disconnect(sn);
                }
            }

            break;
        }

        case SOCK_CLOSE_WAIT:
            disconnect(sn);
            break;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  W5500_Init();

  volatile uint8_t version = getVERSIONR();

  HAL_UART_Receive_IT(&huart1, &rxByte, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  WebServer_Run();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, W5500_CS_Pin|W5500_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : W5500_CS_Pin W5500_RST_Pin */
  GPIO_InitStruct.Pin = W5500_CS_Pin|W5500_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        if (rxByte == '\r' || rxByte == '\n')
        {
            if (uartPos > 0)
            {
                uartBuf[uartPos] = '\0';
                strcpy(uartText, uartBuf);
                uartPos = 0;
            }
        }
        else if (uartPos < sizeof(uartBuf) - 1)
        {
            uartBuf[uartPos++] = rxByte;
        }

        HAL_UART_Receive_IT(&huart1, &rxByte, 1);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
