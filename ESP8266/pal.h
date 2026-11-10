#ifndef PAL_H
#define PAL_H

#include "main.h"
#include <string.h>
#include "usart.h"
#include "stm32f4xx.h"
#include "stdio.h"




/* credentials are kept in secrets.h (git-ignored) */
#include "secrets.h"



typedef struct {
    uint8_t client_id[50];
    uint8_t username[24];
    uint8_t password[24];
    int socket;
    uint32_t keepalive_interval;
    uint8_t clean_session;
    
    // 关键：函数指针成员，用于发送和接收数据
    int (*send)(void* sock, const void *buf, unsigned int len);
    int (*recv)(int sock, uint8_t *buf, int buf_len, int timeout);
    
    void *socket_info; 
} mqtt_broker_handle_t;



int pal_tcp_recv_raw(int sock, uint8_t *buf, int len, int timeout_ms);

void MQTT_SendTempHumi_NoWait(float temp, float humi,float light);

void MQTT_Init(void);
void RE_MQTT_Init();

int MQTT_SendTempHumi(float temp, float humi);
void MQTT_SendPing(void);

void MQTT_ReConnect();
#endif
