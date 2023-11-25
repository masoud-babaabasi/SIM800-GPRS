/*
* file: sim800.h
* Author: Masoud Babaabasi
* date: August 2023
*/
#ifndef __SIM800
#define __SIM800
#include "main.h"

#define USE_RTC 0

#define sim800_TX_BUFF_SIZE 	128
#define sim800_RX_BUFF_SIZE 	1024
#define responseInfoSize 		12
#define call_response_size		6

#define SIM800_TIMEOUT 			-1
#define SIM800_ERROR 			0
#define SIM800_NOT_READY 		1
#define SIM800_READY 			2
#define SIM800_CONNECT_OK 		3
#define SIM800_CONNECT_FAIL 	4
#define SIM800_ALREADY_CONNECT 	5
#define SIM800_SEND_OK 			6
#define SIM800_SEND_FAIL 		7
#define SIM800_DATA_ACCEPT 		8
#define SIM800_CLOSED 			9
#define SIM800_READY_TO_RECEIVE 10 // basically SMSGOOD means >
#define SIM800_OK 				11

#define SIM800_CALL_ERROR		0
#define SIM800_CALL_NO_DIALTONE	1
#define SIM800_CALL_BUSY		2
#define SIM800_CALL_NO_CARRIER	3
#define SIM800_CALL_NO_ANSWER	4
#define SIM800_CALL_OK			5

void Sim800_Init(UART_HandleTypeDef *_GSM_UART, GPIO_TypeDef *_PWR_GPIO, uint16_t _PWR_PIN , GPIO_TypeDef *_STAT_GPIO, uint16_t _STAT_PIN,GPIO_TypeDef *_NET_GPIO, uint16_t _NET_PIN);
void SIM800_handle_uart_int(UART_HandleTypeDef *huart);
void SIM800_handle_net_light_int(uint16_t GPIO_Pin , TIM_HandleTypeDef *timer , uint32_t timer_freq);
uint8_t Sim800_Power( uint8_t ON_OFF); // turns off/on the module if module return 1 if the action was successful
void Sim800_RX_Fill(uint8_t input_byte); // call in the HAL_UART_RxCpltCallback interupt function.
uint16_t Sim800_Input_Available(void); // show number of btyes available in sim800_RX_buff
uint16_t Sim800_Read_String_Until(char *read_string, char input_char, uint16_t time_out);
int8_t _checkResponse(uint16_t time_out);
uint8_t	Sim800_IsAttached(void);
uint8_t Sim800_IsOn(void);
int8_t  Sim800_GetNetworkResisteration();
uint8_t Sim800_Send_SMS(char *number, char *msg);
uint8_t Sim800_Send_SMS_Persian(char *number, uint16_t *msg);// unicode input
uint8_t Sim800_Check_New_SMS(uint8_t * SMS_index , uint8_t size_insdex); // return the number of unread reciced messages and array of index of unread messages
uint8_t Sim800_Read_SMS(uint8_t index , char *number ,char *msg);
#if USE_RTC != 0
uint8_t Sim800_Get_GMT_time(RTC_DateTypeDef* GMT_date , RTC_TimeTypeDef *GMT_time);
#endif
uint8_t Sim800_Get_Location(float *Lat , float *Alt);
int8_t	Sim800_Make_call(char *number , int timeout);
int8_t	Sim800_Hangup();
#endif

/*******************************END OF FILE*****************************************/
