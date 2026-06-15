#ifndef __BSP_ESP8266_H
#define __BSP_ESP8266_H

#include "main.h"

uint8_t ESP8266_SendCmd(const char *cmd,const char *expect,uint32_t timeout_ms);
uint8_t ESP8266_BasicInit(void);
uint8_t ESP8266_ConnectWiFi(const char*ssid,const char*password);
uint8_t ESP8266_TCPConnect(const char*ip,uint16_t port);
uint8_t ESP8266_TCPSend(const uint8_t*data,uint16_t len);

#endif
