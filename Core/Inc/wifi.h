#ifndef __WIFI_H
#define __WIFI_H

#include "main.h"
#include "hardware_config.h"

typedef struct {
    uint8_t connected;
    char ssid[32];
    char ip[16];
} WiFi_State;

typedef struct {
    uint8_t temp;
    uint8_t hum;
    uint8_t medicine_percent;
    uint8_t light_percent;
    uint8_t volume;
    uint8_t battery_soc;
    uint16_t battery_voltage;
    uint8_t led_state;
    uint8_t fan_state;
    uint8_t heater_state;
    uint8_t box_open;
    uint8_t rtc_synced;
} Device_Status;

extern volatile uint8_t wifi_buffer[512];
extern volatile uint16_t wifi_buffer_index;
extern uint8_t wifi_rx_byte;

void WiFi_Init(void);
uint8_t WiFi_Connect(const char *ssid, const char *pwd);
uint8_t WiFi_ConnectStatic(const char *ssid, const char *pwd,
                           const char *ip, const char *gateway,
                           const char *netmask, const char *dns1, const char *dns2);
uint8_t WiFi_SendDataToLink(uint8_t link_id, const char *data, uint16_t data_len);
void WiFi_ProcessBuffer(void);
WiFi_State* WiFi_GetState(void);
uint8_t WiFi_GetCommand(char *cmd);
void WiFi_UpdateStatus(Device_Status *status);

#endif
