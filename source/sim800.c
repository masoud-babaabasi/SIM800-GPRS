/*
* file: sim800.h
* Author: Masoud Babaabasi
* date: August 2023
*/
#include "sim800.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

static char sim800_RX_buffer[sim800_RX_BUFF_SIZE];
static uint8_t buff_gsm;
//char sim800_TX_buffer[sim800_TX_BUFF_SIZE];
static const char* SIM800_responseInfo[responseInfoSize] ={"ERROR",
													"NOT READY",
													"READY",
													"CONNECT OK",
													"CONNECT FAIL",
													"ALREADY CONNECT",
													"SEND OK",
													"SEND FAIL",
													"DATA ACCEPT",
													"CLOSED",
													"> ",
													"OK"};

static const char* SIM800_call_response[call_response_size] ={"+CME ERROR:",
														"NO DIALTONE",
														"BUSY",
														"NO CARRIER",
														"NO ANSWER",
														"OK"};
#if sim800_RX_BUFF_SIZE > 256
static uint16_t rx_end_ptr , rx_start_ptr;
#else
static uint8_t rx_end_ptr , rx_start_ptr;
#endif

static UART_HandleTypeDef *GSM_UART;
static GPIO_TypeDef *PWR_GPIO, *STAT_GPIO,*NET_GPIO;
static uint16_t PWR_PIN ,STAT_PIN , NET_PIN;
static uint16_t last_SMS_index;
static uint16_t net_light_dur = 0;

static void convert_persian_copy(uint16_t *str_dest , char *str_src){
	uint16_t size_str = strlen(str_src);
	if( size_str % 4 == 0){
		for( uint16_t i = 0 ; i < size_str / 4 ; i+=1){
			str_dest[i] = 0;
			for(uint8_t j = 0 ; j < 4 ; j++){
				uint8_t a;
				if( str_src[i * 4 + j] >= '0' && str_src[i * 4 + j] <= '9')
					a = str_src[i * 4 + j] - '0';
				else a = str_src[i * 4 + j] - 'A' + 10;
				str_dest[i] |= a << (12 - j * 4);
			}
		}
	}

}
/*
* 	@brief: removes all the "\n\r" from a string
*	@param: input string
*/
void _strip_string_CR_LR(char *response){
	for(int i = 0 ; i < strlen(response) ; i++){
		if( response[i] == '\r' || response[i] == '\n'){
			memmove(&response[i] , &response[i+1] , strlen(&response[i+1]));
		}
	}
}
/*
*	@brief: sends a string over UART
*	@param: input string bytes
*/
static void Sim800_transmit(char *cmd){
	HAL_UART_Transmit(GSM_UART,(uint8_t*)cmd , strlen(cmd) , 1000);
}
/*
*	@brief: Initialize the library , POWER pin must be output to be able to turn the module on or off
*			STATUS and NET pins must configure to be input to be able read the module status
*	@param: pointer to UART_HandleTypeDef, microcontroller's UART connected to sim800 module
*	@param: pointer GPIO_TypeDef for POWER pin of sim800 connected to microcontroller
*	@param: pin number of POWER pin of sim800 connected to microcontroller
*	@param: pointer GPIO_TypeDef for STATUS pin of sim800 connected to microcontroller
*	@param: pin number of STATUS pin of sim800 connected to microcontroller
*	@param: pointer GPIO_TypeDef for NET pin of sim800 connected to microcontroller
*	@param: pin number of NET pin of sim800 connected to microcontroller
*/
void Sim800_Init(UART_HandleTypeDef *_GSM_UART, GPIO_TypeDef *_PWR_GPIO, uint16_t _PWR_PIN ,
											    GPIO_TypeDef *_STAT_GPIO, uint16_t _STAT_PIN ,
												GPIO_TypeDef *_NET_GPIO, uint16_t _NET_PIN){
	GSM_UART = _GSM_UART;
	PWR_GPIO = _PWR_GPIO;
	PWR_PIN = _PWR_PIN;
	STAT_GPIO = _STAT_GPIO;
	STAT_PIN = _STAT_PIN;
	NET_GPIO = _NET_GPIO;
	NET_PIN = _NET_PIN;
	HAL_UART_Receive_IT(GSM_UART,&buff_gsm, 1 );
	if(HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) == 1 ){
		Sim800_transmit("ATE0\r");
		HAL_Delay(1000);
	}	
	memset((char*) sim800_RX_buffer , 0 , sim800_RX_BUFF_SIZE);
	rx_end_ptr = rx_start_ptr = 0;
	net_light_dur = 0;
	if(HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) == 1 ){
		Sim800_transmit("AT+CMGF=1\r");
		HAL_Delay(100);
		Sim800_transmit("AT+CSMP=17,167,0,0\r");
		while(_checkResponse(1000) >= 0);	}
}
/*
*	@brief: this function must be written in the interrupt function of the UART
			this function fills up the RX bufer when an receive interrupt occurs 
*	@param: pointer to UART_HandleTypeDef
*/
void SIM800_handle_uart_int(UART_HandleTypeDef *huart){
	if( huart == GSM_UART){
		Sim800_RX_Fill(buff_gsm);
		HAL_UART_Receive_IT(GSM_UART,&buff_gsm, 1 );
	}
}
/*
*	@brief: this function must be written in the interrupt function of the GPIO interrupts
			measures the duration of NET pin high time in ms
*	@param: NET pin number
*	@param: pointer to a TIM_HandleTypeDef for timing measurements
*	@param: used timer ferquency
*/
void SIM800_handle_net_light_int(uint16_t GPIO_Pin , TIM_HandleTypeDef *timer , uint32_t timer_freq){
	if(GPIO_Pin == NET_PIN){
		 if(HAL_GPIO_ReadPin(NET_GPIO, NET_PIN) == 0){
			 HAL_TIM_Base_Start(timer);
		 }else{
			 HAL_TIM_Base_Stop(timer);
			 net_light_dur = timer->Instance->CNT;
			 net_light_dur = ((uint32_t)net_light_dur * 1000 ) / timer_freq; // milli seconds
			 timer->Instance->CNT = 0;
		 }
	 }
}
/*
*	@brief: turn ON or OFF the module
*	@param: 1 means ON and 0 means OFF
*	@return: 1 if success, 0 on fail
*/
uint8_t Sim800_Power( uint8_t ON_OFF){
	uint32_t time_start;
	if( ON_OFF && HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) == 0){
		time_start = HAL_GetTick();
		net_light_dur = 0;
		HAL_GPIO_WritePin(PWR_GPIO,PWR_PIN,GPIO_PIN_RESET);
		while( HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) == 0  && (HAL_GetTick() - time_start) < 4000);
		HAL_GPIO_WritePin(PWR_GPIO,PWR_PIN,GPIO_PIN_SET);
		HAL_Delay(2000);
		Sim800_transmit("ATE0\r");
		HAL_Delay(1500);
		memset((char*) sim800_RX_buffer , 0 , sim800_RX_BUFF_SIZE);
		rx_end_ptr = rx_start_ptr = 0;
		Sim800_transmit("AT+CMGF=1\r");
		HAL_Delay(100);
		Sim800_transmit("AT+CSMP=17,167,0,0\r");
		while(_checkResponse(1000) >= 0);
		return HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN );
	}
	if( ON_OFF == 0 && HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) == 1){
		time_start = HAL_GetTick();
		HAL_GPIO_WritePin(PWR_GPIO,PWR_PIN,GPIO_PIN_RESET);
		while( HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) == 1  && (HAL_GetTick() - time_start) < 5000);
		HAL_GPIO_WritePin(PWR_GPIO,PWR_PIN,GPIO_PIN_SET);
		net_light_dur = 0;
		return ~( HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN ) );
	}
	return HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN );
}
/*
*	@brief: write one byte to RX buffer
*	@param: input byte
*/
void Sim800_RX_Fill(uint8_t input_byte){
	sim800_RX_buffer[rx_end_ptr] = (char) input_byte;
	rx_end_ptr = ( rx_end_ptr + 1 ) % sim800_RX_BUFF_SIZE;
}
/*
*	@brief: check to see if there are any unread bytes in the RX buffer
*	@return: number of bytes available to read in the RX buffer
*/
uint16_t Sim800_Input_Available(){
	if( rx_end_ptr >= rx_start_ptr ){
		return rx_end_ptr - rx_start_ptr;
	}
	return (sim800_RX_BUFF_SIZE - rx_start_ptr) + rx_end_ptr;
}
/*
*	@brief: read a string from input buffer until the delimitater
*	@param: buffer to copy the read string
*	@param: delimitater character 
*	@param: timout in milliseconds
*/
uint16_t Sim800_Read_String_Until(char *read_string, char input_char, uint16_t time_out){
	uint16_t read_number = 0;
	uint32_t start_time = HAL_GetTick();
	
	while( ( HAL_GetTick() - start_time ) <= time_out){
		if(Sim800_Input_Available()){
			*(read_string + read_number) = sim800_RX_buffer[rx_start_ptr];
				rx_start_ptr = (rx_start_ptr + 1 ) % sim800_RX_BUFF_SIZE;
				read_number++;
			if( sim800_RX_buffer[rx_start_ptr - 1] == input_char) return read_number;
		}
	}
	// timeout occured
	if( rx_start_ptr > read_number ) rx_start_ptr -= read_number;
	else rx_start_ptr = sim800_RX_BUFF_SIZE - ( read_number - rx_start_ptr);
	return 0;
}
/*
*	@brief: after a command read a response string from RX buffer 
*	@param: timout in milliseconds
*	@return: the Index of the response
*/
int8_t _checkResponse(uint16_t time_out){
	char response[32];
	if(  Sim800_Read_String_Until(response, '\n' , time_out) ){
		Sim800_Read_String_Until(response, '\n' , time_out);
		_strip_string_CR_LR(response);
		for( int8_t i = 0 ; i < responseInfoSize ; i++){
			if( strnstr( response , SIM800_responseInfo[i] , 32) != NULL ){
				return i;
			}
		}
		return responseInfoSize;
	}
	return -1;
}
/*
*	@brief: after a call procedure read a response string from RX buffer 
*	@param: timout in milliseconds
*	@return: the Index of the response
*/
int8_t _checkCallResponse(uint16_t time_out){
	char response[32];
	if(  Sim800_Read_String_Until(response, '\n' , time_out) ){
		Sim800_Read_String_Until(response, '\n' , time_out);
		_strip_string_CR_LR(response);
		for( int8_t i = 0 ; i < call_response_size ; i++){
			if( strstr( response , SIM800_call_response[i]) != NULL ){
				return i;
			}
		}
	}
	return -1;
}
/*
*	@brief: send a "AT" string to module to see if the module is ready to response
*	@return: 1 if module is ready and responed OK
*/
uint8_t	Sim800_IsAttached(){
	Sim800_transmit("AT\r");
	if( _checkResponse(2000) != SIM800_OK )
		return 0;
	else 
		return 1;
}
/*
*	@brief: check the STATUS pin status
*	@return: STATUS pin value
*/
uint8_t Sim800_IsOn(){
	return HAL_GPIO_ReadPin(STAT_GPIO , STAT_PIN );
}
/*
*	@brief: send a SMS message
*	@param: string containing the number to be sent to
*	@param: message text
*	@return: 1 on success, 0 on fail
*/
uint8_t Sim800_Send_SMS(char *number, char *msg){
	uint8_t uart_size;
	char str[150];
	Sim800_transmit("AT+CSMP=49,167,0,0\r");
	while(_checkResponse(1000) >= 0);
	uart_size = sprintf(str,"AT+CMGS=\"%s\"\r" , number);
	Sim800_transmit(str);
	Sim800_Read_String_Until(str, ' ', 2000);
	if( strstr(str,">") != NULL ){
		uart_size = snprintf(str , 140 , "%s\r" , msg ); 
		Sim800_transmit(str);
		uint8_t end_SMS[2] = {26 , 0}; //CTRL+Z
		Sim800_transmit((char *)end_SMS);
		HAL_Delay(1500);
		char res[32];
		_checkResponse(10000);
		if( _checkResponse(5000) != SIM800_OK ) return 0;
		return 1;
	}
	return 0;
}
/*
*	@brief: send a unicode Persian SMS message
*	@param: string containing the number to be sent to
*	@param: message text
*	@return: 1 on success, 0 on fail
*/
uint8_t Sim800_Send_SMS_Persian(char *number, uint16_t *msg){
	char str[32];
	uint16_t len_msg = 0;
	while(msg[len_msg] != 0) len_msg++;
	char str_hex[6] = {0};
	Sim800_transmit("AT+CSCS=\"HEX\"\r");
	while(_checkResponse(1000) >= 0);
	Sim800_transmit("AT+CSMP=17,167,0,8\r");
	while(_checkResponse(1000) >= 0);
	sprintf(str,"AT+CMGS=\"%s\"\r" , number);
	Sim800_transmit(str);
	Sim800_Read_String_Until(str, ' ', 2000);
	if( strstr(str,">") != NULL ){

		for(uint16_t i = 0 ; i < len_msg ; i++){
			for( uint8_t j = 0 ; j < 4 ; j++){
				char a = (msg[i] >> (12 - j * 4)) & 0x0F;
				if( a >= 0 && a <= 9) str_hex[j] = '0' + a;
				else str_hex[j] = 'A' + ( a - 10 );
			}
			Sim800_transmit(str_hex);
		}

		sprintf(str  ,"\r" );
		Sim800_transmit(str);
		uint8_t end_SMS[2] = {26 , 0}; //CTRL+Z
		Sim800_transmit((char *)end_SMS);
		HAL_Delay(1500);
		_checkResponse(10000); //
		if( _checkResponse(5000) != SIM800_OK ) return 0;
		Sim800_transmit("AT+CSCS=\"GSM\"\r");
		while(_checkResponse(1000) >= 0);
		Sim800_transmit("AT+CSMP=49,167,0,0\r");
		while(_checkResponse(1000) >= 0);
		return 1;
	}
	return 0;
}
/*
*	@brief: check to see if there are new messages received
*	@param: pointer to an array which the Unread SMS indexes is written
*	@param: length of indexes array
*	@return: number of unread SMS
*/
uint8_t Sim800_Check_New_SMS(uint8_t * SMS_index , uint8_t size_insdex){
	uint8_t uart_size , unread_sms = 0;
	char response[1024];
	Sim800_transmit("AT+CMGL=\"REC UNREAD\",1\r");
	while( Sim800_Read_String_Until(response, '\n' , 3000) ){
		if( unread_sms >= size_insdex) break;
		if( response[0] == '\r' && response[1] == '\n') continue;
		if( strnstr(response , "+CMGL:"  , 1024) != NULL){
			*(SMS_index + unread_sms) = atoi( response + strlen("+CMGL: ") );
			unread_sms ++ ;
		}
		if( strnstr(response , "+CMTI: \"SM\"" , 1024 ) != NULL){
			*(SMS_index + unread_sms) = atoi( response + strlen("+CMTI: \"SM\",") );
			unread_sms ++ ;
		}
		if( response[0] == 'O' && response[1] == 'K') {
			Sim800_Read_String_Until(response, '\n' , 3000);
			break;
		}
	}
	return unread_sms;
}
/*
*	@brief: read a SMS from module memory
*	@param: index of SMS in the memory
*	@param: string that the sender number is copied to
*	@param: string for SMS text 
*	@return: 0 on fail, else success
*/
uint8_t Sim800_Read_SMS(uint8_t index , char *number ,char *msg){
	uint8_t uart_size;
	char	*ptr1 , *ptr2 , row = 0;
	char str[512];
	uint8_t persian_coding = 0;
	Sim800_transmit("AT+CMGF=1\r");
	if( _checkResponse(1000) != SIM800_OK ) return 0;

	Sim800_transmit("AT+CSDH=1\r");
	if( _checkResponse(1000) != SIM800_OK ) return 0;

	uart_size = sprintf(str,"AT+CMGR=%d\r",index);
	Sim800_transmit(str);
	uint16_t read_size = Sim800_Read_String_Until(str, '\n' , 3000);
	while( read_size ){
		if( str[0] == '\r' && str[1] == '\n') {
			//continue;
		}else if( str[0] == 'O' && str[1] == 'K') {
			Sim800_Read_String_Until(str, '\n' , 3000);
			return (1 + persian_coding);
		}else if( strstr(str , "+CMGR:" ) != NULL){
			for(uint8_t i = 0 ; i < read_size ; i++){
				if( str[i] == ',' ) {
					row++;
					if( row == 1) ptr1 = &str[i+2];
					else if( row == 2) str[i-1] = 0;
					else if( row == 9){
						if( str[i-1] == '8' ) persian_coding = 1;
						break;
					}
				}
			}
			strcpy( number , ptr1 );
		}else{
			str[read_size - 2] = '\0';
			if( persian_coding ){
				convert_persian_copy((uint16_t*)msg , str);
			}else{
				strcpy( msg , str);
			}
		}
		read_size = Sim800_Read_String_Until(str, '\n' , 3000);
	}
	return 0;
}
#if USE_RTC != 0
/*
*	@brief: get GMT time from the operator
*	@param: pointer to RTC_DateTypeDef (defined in HAL rtc header file. must initialize the rtc)
*	@param: pointer to RTC_TimeTypeDef
*	@return: 1 on succes
*/
uint8_t Sim800_Get_GMT_time(RTC_DateTypeDef* GMT_date , RTC_TimeTypeDef *GMT_time){
	uint8_t uart_size;
	char	*ptr1 , *ptr2;
	char str[150];
	char temp[5];
	Sim800_transmit("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r");
	if( _checkResponse(5000) != SIM800_OK ) return 0;

	Sim800_transmit("AT+SAPBR=3,1,\"APN\",\"mtnirancell\"\r");
	if( _checkResponse(5000) != SIM800_OK ) return 0;
	
	Sim800_transmit("AT+SAPBR=1,1\r");
	if( _checkResponse(7000) != SIM800_OK ) return 0;
	
	Sim800_transmit("AT+CIPGSMLOC=1,1\r");
	while( Sim800_Read_String_Until(str, '\n' , 7000) ){
		if( str[0] == '\r' && str[1] == '\n') continue;
		if( strstr(str , "+CIPGSMLOC:" ) != NULL){
			ptr1 = strstr( str , ",");
			ptr1 = strstr( (char*)( ptr1 + 1) , ",");
			ptr1 = strstr( (char*)( ptr1 + 1) , ",");
			ptr2 = strstr( (char*)( ptr1 + 1) , "/");
			*(ptr2) = '\0';
			strcpy( temp , ptr1 + 1 );
			GMT_date->Year = atoi(temp) % 100;
			
			ptr1 = ptr2;
			ptr2 = strstr( (char*)( ptr1 + 1) , "/");
			*(ptr2) = '\0';
			strcpy( temp , ptr1 + 1 );
			GMT_date->Month = atoi(temp);
			
			ptr1 = ptr2;
			ptr2 = strstr( (char*)( ptr1 + 1) , ",");
			*(ptr2) = '\0';
			strcpy( temp , ptr1 + 1 );
			GMT_date->Date = atoi(temp);
			
			ptr1 = ptr2;
			ptr2 = strstr( (char*)( ptr1 + 1) , ":");
			*(ptr2) = '\0';
			strcpy( temp , ptr1 + 1 );
			GMT_time->Hours = atoi(temp);
			
			ptr1 = ptr2;
			ptr2 = strstr( (char*)( ptr1 + 1) , ":");
			*(ptr2) = '\0';
			strcpy( temp , ptr1 + 1 );
			GMT_time->Minutes = atoi(temp);
			
			strcpy( temp , ptr2 + 1 );
			GMT_time->Seconds = atoi(temp);
		}
		if( str[0] == 'O' && str[1] == 'K') {
			Sim800_Read_String_Until(str, '\n' , 5000);
			//return 1;
		}
	}
	Sim800_transmit("AT+SAPBR=0,1\r\n");
	if( _checkResponse(5000) != SIM800_OK ) return 0;
	return 1;
}
#endif
/*
*	@brief: make a phone call
*	@param: srtring containing the number
*	@param: timeout in milliseconds
*	@return: response index( must check if it is equal SIM800_OK)
*/
int8_t Sim800_Make_call(char *number , int timeout){
	char str_temp[32];
	uint8_t size = sprintf(str_temp , "ATD%s;\r" , number);
	Sim800_transmit(str_temp);
	return _checkCallResponse(timeout);
}
/*
*	@brief: terminate current phone call
*	@return: 1 on success
*/
int8_t Sim800_Hangup(){
	Sim800_transmit("ATH\r");
	if( _checkResponse(5000) != SIM800_OK ) return 0;
	return 1;
}
/*
*	@brief: this function works correnctly when SIM800_handle_net_light_int is called in the 
*			NET pin intterupt to measure the net pin HIGH duration
*	@return: ststuse of registration of module in the network
*			0 : not registered
*			1 : registered successfully
*			2 : GPRS communication is established
*			-1 : NET duration calculated is invalid
*/
int8_t Sim800_GetNetworkResisteration(){
	if( net_light_dur > 780 && net_light_dur < 820){
		return 0; //not registered
	 }
	if( net_light_dur > 2950 && net_light_dur < 3050){
		  return 1; // registered successfully
	 }
	if( net_light_dur > 290 && net_light_dur < 310){
		return 2; //GPRS communication is established
	}
	return -1;
}
/*******************************END OF FILE*****************************************/
