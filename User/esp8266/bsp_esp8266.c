#include "bsp_esp8266.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>



#define ESP8266_UART        huart2
#define ESP8266_RX_BUF_SIZE   256

static char esp_rx_buf[ESP8266_RX_BUF_SIZE];

static void ESP8266_ClearBuffer(void)
{
	memset(esp_rx_buf,0,sizeof(esp_rx_buf));
}
uint8_t ESP8266_SendCmd(const char *cmd,const char *expect,uint32_t timeout_ms)
{	
	uint8_t ch;
	uint16_t index=0;
	uint32_t start_tick;
	
	if(cmd==NULL||expect==NULL)
	{
		return 1;
	}
		ESP8266_ClearBuffer();
	
		HAL_UART_Transmit(&ESP8266_UART,
											(uint8_t*)cmd,							
											(uint16_t)strlen(cmd),		
												1000				);
		start_tick=HAL_GetTick();
		
	  while(HAL_GetTick()-start_tick<timeout_ms)
		{
			if(HAL_UART_Receive(&ESP8266_UART,&ch,1,10)==HAL_OK)
			{
				if(index<ESP8266_RX_BUF_SIZE-1)
				{
					esp_rx_buf[index++]=(char)ch;
					esp_rx_buf[index]='\0';
				}
				if(strstr(esp_rx_buf,expect)!=NULL)
				{
					return 0;
				}
			}
		}
		return 1;
	}
uint8_t ESP8266_BasicInit(void)
{
	if(ESP8266_SendCmd("AT\r\n","OK",1000)!=0)
	{
		return 2;
	}
	return 0;
}
uint8_t ESP8266_ConnectWiFi(const char*ssid,const char*password)
{
	char cmd[128];
	if(ssid==NULL||password==NULL)
	{
		return 1;
	}
	snprintf(cmd,
         sizeof(cmd),
         "AT+CWJAP=\"%s\",\"%s\"\r\n",
         ssid,
         password);
	if(ESP8266_SendCmd(cmd,"WIFI GOT IP",15000)==0)
	{
		return 0;
	}
	if(ESP8266_SendCmd("AT+CWJAP?\r\n","OK",2000)==0)
	{
		return 0;
	}
	return 1;
}
uint8_t ESP8266_TCPConnect(const char*ip,uint16_t port)
{
	char cmd[64];
	snprintf(cmd,sizeof(cmd),"AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",ip,port);
	if(ESP8266_SendCmd(cmd,"CONNECT",10000)!=0)
	{
		if(strstr(esp_rx_buf,"ALREADY CONNECTED")!=NULL)
			return 0;
		return 1;
	}
	return 0;
}
uint8_t ESP8266_TCPSend(const uint8_t*data,uint16_t len)
{	
	char cmd[16];
	snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%u\r\n",len);
	if(ESP8266_SendCmd(cmd,">",5000)!=0)
		return 1;
		
	HAL_UART_Transmit(&ESP8266_UART,(uint8_t*)data,len,1000);
	return ESP8266_SendCmd("","SEND OK",5000);
}
