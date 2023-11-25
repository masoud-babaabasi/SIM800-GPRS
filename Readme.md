# SIM800c GPRS communication library
### STM32 C library to communicate with SIM800c GPRS module
**This Library is tested on stm32H7 series.**

This library communicates with the GPRS SIM800 module via UART. The module communication is based on AT command protocol. For proper operation of the library the NET and STATUS pins of the module should be connected to the microcontroller and these pins must be configured as input. The POWER pin of the module also should be connected and configured as output to be able to turn the module on or off.
In the configurartion the receive interrupt must be enabled and the `SIM800_handle_uart_int` function must be called in the interrupt.
The NET pin interrupt must also be configured and `SIM800_handle_net_light_int` function must be called in the NET pin interrupt.

This library supports receiving and sending SMS messages. It also supports pesian UNICODE SMS.
You can make phone calls with the module also.

I recommend to configure the microcontroller with STM32CubeMX tool. You can use the library like the piece of code below:
```C
#include "sim800.h"
UART_HandleTypeDef huart1;
int main(void)
{
/*
* Start up code 
*/
  HAL_Init();
  SystemClock_Config();
/*
* CubeMX Initilization functions
*/
  MX_TIM6_Init();
  MX_UART1_Init();
/* USER CODE BEGIN 2 */
Sim800_Init(&huart1 , GSM_PWR_GPIO_Port, GSM_PWR_Pin,
  		  GSM_STAT_GPIO_Port, GSM_STAT_Pin , GSM_NET_GPIO_Port , GSM_NET_Pin);
uint8_t sms_index[10];
char number[14] = {0} , msg[300] = {0};
uint16_t msg_persian[700];
uint8_t num_sms = Sim800_Check_New_SMS( sms_index , 10);
printf(" %d New SMS", num_sms);
for( uint8_t i = 0 ; i < num_sms ; i++){
	  uint8_t encoding = Sim800_Read_SMS( sms_index[i] , number , msg);
	  	if( encoding == 1){
	  		printf("%s : %s",number , msg);
	  	}else if(encoding == 2){
	  		ConvertUC2UTF8((uint16_t*)msg, 256, (char *)msg_persian, 700); // implement unicode to UTF-8
        printf("%s : %s",number , msg_persian);
	  	}
  	}
/* USER CODE END 2 */
while (1)
  {
    int8_t network  = Sim800_GetNetworkResisteration();
    switch(network){
    case 0:
  	  printf("Not Connected to any network");
  	  break;
    case 1:
  	  printf("Successfully Connected to Network");
    	  break;
    case 2:
  	  printf("GPRS Connection established");
    	  break;
    default:
  	  
    }

  }
}

#define TIMER_FREQ				10000
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	 SIM800_handle_net_light_int(GPIO_Pin , &htim6 , TIMER_FREQ);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	SIM800_handle_uart_int(huart);
}
```
