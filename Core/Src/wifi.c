#include "wifi.h"
#include "usart.h"
#include "TW_TTS.h"
#include "actuator.h"
#include "rtc.h"
#include <string.h>
#include <stdio.h>

#define WIFI_DEBUG_ENABLE 0

#if WIFI_DEBUG_ENABLE
#define WIFI_LOG(fmt, ...) do { \
    char _dbg_buf[128]; \
    int _dbg_len = snprintf(_dbg_buf, sizeof(_dbg_buf), "[WIFI] " fmt "\r\n", ##__VA_ARGS__); \
    if (_dbg_len > 0) HAL_UART_Transmit(&huart2, (uint8_t*)_dbg_buf, _dbg_len, 100); \
} while(0)
#else
#define WIFI_LOG(fmt, ...)
#endif

volatile uint8_t wifi_buffer[512];
volatile uint16_t wifi_buffer_index = 0;

static WiFi_State wifi_state = {0, "", ""};
uint8_t wifi_rx_byte;
static char wifi_cmd_buf[256];
static uint16_t wifi_cmd_len = 0;
static Device_Status device_status_cache = {0};

static char full_response_buf[512];
static char api_json_buf[256];

/**
 * @brief 发送 WiFi 命令并等待响应
 * @details 该函数通过 UART1 向 WiFi 模块发送 AT 命令，并等待模块的响应，
 *          在超时时间内检查是否收到期望的响应字符串
 * @param cmd 要发送的 AT 命令字符串
 * @param expect 期望从 WiFi 模块收到的响应字符串，如果为 NULL 则不检查响应
 * @param timeout_ms 等待响应的超时时间（单位：毫秒）
 * @return 1 - 收到期望响应，0 - 超时或未收到期望响应
 * @note 函数使用中断禁用/启用机制保护缓冲区访问，防止数据竞争
 */
static uint8_t WiFi_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    uint8_t resp[256];  
    uint32_t tick_start = HAL_GetTick();  

    __disable_irq();
    wifi_buffer_index = 0;
    __enable_irq();

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 200);
    HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 200);

     WIFI_LOG("TX: %s", cmd);

    while ((HAL_GetTick() - tick_start) < timeout_ms)
    {
        __disable_irq();
        uint16_t idx = wifi_buffer_index;
        __enable_irq();

        if (idx > 0)
        {
            HAL_Delay(5);

            __disable_irq();
            idx = wifi_buffer_index;
            __enable_irq();

            if (idx > 0 && idx < sizeof(resp))
            {
                __disable_irq();
                memcpy(resp, (const void *)wifi_buffer, idx);
                __enable_irq();

                resp[idx] = '\0';
                if (expect && strstr((char *)resp, expect))
                {
                    WIFI_LOG("RX OK: '%s'", expect);
                    __disable_irq();
                    wifi_buffer_index = 0;
                    __enable_irq();
                    return 1;
                }
            }
        }
        HAL_Delay(2);
    }
    WIFI_LOG("TIMEOUT: expect '%s'", expect ? expect : "NULL");
    __disable_irq();
    wifi_buffer_index = 0;
    __enable_irq();
    return 0;
}

/**
 * @brief 查询 WiFi 模块的 IP 地址
 * @details 该函数通过发送 AT+CIFSR 命令查询 WiFi 模块的 IP 地址，
 *          最多尝试 5 次，成功后将 IP 地址保存到 wifi_state.ip 中
 * @param 无
 * @return 1 - 成功获取 IP 地址，0 - 获取失败
 * @note IP 地址格式为 "192.168.1.100"，最大长度 15 字符
 */
static uint8_t WiFi_QueryIP(void)
{
    uint8_t retry; 
    for (retry = 0; retry < 5; retry++)
    {
        WIFI_LOG("Querying IP (try %u)...", retry + 1);

        __disable_irq();
        wifi_buffer_index = 0;
        __enable_irq();

        HAL_UART_Transmit(&huart1, (uint8_t *)"AT+CIFSR\r\n", 10, 200);
        HAL_Delay(500);

        __disable_irq();
        uint16_t idx = wifi_buffer_index;
        __enable_irq();

        if (idx > 0 && idx < 510)
        {
            __disable_irq();
            char *buf = (char *)wifi_buffer;
            buf[idx] = '\0';
            __enable_irq();

            WIFI_LOG("CIFSR resp: %s", buf);

            char *p = strstr(buf, "STAIP,\"");
            if (p)
            {
                p += 7;
                uint8_t i = 0;
                while (*p != '"' && *p != '\0' && *p != '\r' && *p != '\n' && i < 15)
                {
                    wifi_state.ip[i++] = *p++;
                }
                wifi_state.ip[i] = '\0';

                if (wifi_state.ip[0] != '\0')
                {
                    WIFI_LOG("Got IP: %s", wifi_state.ip);
                    __disable_irq();
                    wifi_buffer_index = 0;
                    __enable_irq();
                    return 1;
                }
            }
        }

        __disable_irq();
        wifi_buffer_index = 0;
        __enable_irq();

        HAL_Delay(500);
    }

    WIFI_LOG("IP query FAILED after %u tries", retry);
    return 0;
}
 
/**
 * @brief 初始化 ESP8266 WiFi 模块
 * @details 该函数初始化 ESP8266 WiFi 模块，包括以下步骤：
 *          1. 清空 WiFi 状态变量和接收缓冲区
 *          2. 启动 UART1 接收中断，用于接收模块响应
 *          3. 测试 AT 命令通信，最多重试 3 次，验证模块是否正常工作
 *          4. 关闭命令回显（ATE0），减少响应数据量
 *          5. 设置为 Station 模式（AT+CWMODE=1），使模块可以连接到 WiFi 网络
 *          6. 设置 TCP 连接超时时间为 30 秒（AT+CIPSTO=30）
 * @param 无
 * @return 无
 * @note 初始化成功后，模块处于 Station 模式，可以调用 WiFi_Connect 连接到 WiFi 网络
 *       如果 AT 命令测试失败（3 次重试均无响应），函数会提前返回，初始化失败
 *       使用 UART1 接口与 ESP8266 通信，波特率需要在 usart.c 中配置
 */
void WiFi_Init(void)
{
    wifi_state.connected = 0;
    wifi_state.ssid[0] = '\0';
    wifi_state.ip[0] = '\0';
    wifi_buffer_index = 0;

    HAL_UART_Receive_IT(&huart1, &wifi_rx_byte, 1);

    HAL_Delay(500);
    WIFI_LOG("ESP8266 init...");

    uint8_t init_retry;
    for (init_retry = 0; init_retry < 3; init_retry++)
    {
        if (WiFi_SendCmd("AT", "OK", 2000))
        {
            WIFI_LOG("ESP8266 OK (try %u)", init_retry + 1);
            break;
        }
        WIFI_LOG("ESP8266 NOT responding (try %u), retrying...", init_retry + 1);
        HAL_Delay(500);
    }

    if (init_retry >= 3)
    {
        WIFI_LOG("ESP8266 FAILED after %u attempts!", init_retry);
        return;
    }

    HAL_Delay(100);

    if (!WiFi_SendCmd("ATE0", "OK", 1000))
    {
        WIFI_LOG("ATE0 failed, continuing...");
    }
    HAL_Delay(100);

    if (!WiFi_SendCmd("AT+CWMODE=1", "OK", 2000))
    {
        WIFI_LOG("CWMODE failed, retrying...");
        HAL_Delay(200);
        WiFi_SendCmd("AT+CWMODE=1", "OK", 2000);
    }
    WIFI_LOG("Mode: STA (connect to phone hotspot)");
    HAL_Delay(100);

    WiFi_SendCmd("AT+CIPSTO=300", "OK", 2000);
    HAL_Delay(100);

    WiFi_SendCmd("AT+SLEEP=0", "OK", 1000);
    HAL_Delay(100);

    WIFI_LOG("ESP8266 init done");
}

/**
 * @brief 连接到 WiFi 网络
 * @details 该函数使用 DHCP 方式连接到指定的 WiFi 网络，配置多连接模式和 TCP 服务器，
 *          最多尝试 WIFI_CONNECT_RETRY 次连接
 * @param ssid 要连接的 WiFi 网络名称（SSID）
 * @param pwd WiFi 网络密码
 * @return 1 - 连接成功，0 - 连接失败
 * @note 连接成功后会启动 TCP 服务器，监听端口 80
 */
uint8_t WiFi_Connect(const char *ssid, const char *pwd)
{
    char cmd[128]; 
    uint8_t retry;  

    WIFI_LOG("Starting WiFi connect to '%s'...", ssid);   

    for (retry = 0; retry < WIFI_CONNECT_RETRY; retry++)    
    {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
        WIFI_LOG("Connecting to '%s' (try %u/%u)...", ssid, retry + 1, WIFI_CONNECT_RETRY);
       
        if (WiFi_SendCmd(cmd, "OK", WIFI_CONNECT_TIMEOUT_MS)) 
        {
            strncpy(wifi_state.ssid, ssid, sizeof(wifi_state.ssid) - 1);
            wifi_state.ssid[sizeof(wifi_state.ssid) - 1] = '\0'; 
            wifi_state.connected = 1;
            WIFI_LOG("Connected to '%s', querying IP...", ssid);
            HAL_Delay(500);

            if (WiFi_QueryIP())  
            {
                WIFI_LOG("IP obtained: %s", wifi_state.ip); 
            }
            else
            {
                WIFI_LOG("WARNING: IP query failed, but connected");
            }
            HAL_Delay(200);
        
            if (WiFi_SendCmd("AT+CIPMUX=1", "OK", 2000))   
            {
                WIFI_LOG("CIPMUX=1 OK (multi-connection mode)");   
            }
            else
            {
                WIFI_LOG("WARNING: CIPMUX failed, retrying..."); 
                HAL_Delay(200);
                WiFi_SendCmd("AT+CIPMUX=1", "OK", 2000);
            }
            HAL_Delay(100);

            if (WiFi_SendCmd("AT+CIPSERVER=1,80", "OK", 2000)) 
            {
                WIFI_LOG("TCP Server started on port 80"); 
            }
            else
            {
                WIFI_LOG("WARNING: CIPSERVER failed");
            }
            HAL_Delay(100);
  
            WIFI_LOG("READY! SSID=%s IP=%s Port=80", wifi_state.ssid, wifi_state.ip);          
            return 1;    
        }

        WIFI_LOG("Connect try %u FAILED, retrying...", retry + 1);       

        if (retry < WIFI_CONNECT_RETRY - 1) 
        {
            HAL_Delay(1000);
        }
    }

    wifi_state.connected = 0;    
    wifi_state.ip[0] = '\0';
    WIFI_LOG("All %u connect attempts FAILED!", WIFI_CONNECT_RETRY);
    return 0;
}

 
/**
 * @brief 向指定连接发送数据
 * @details 该函数通过指定的连接 ID 向 WiFi 模块发送数据，最多尝试 3 次，
 *          发送成功后返回 1，失败后返回 0
 * @param link_id 连接 ID（0-4）
 * @param data 要发送的数据指针
 * @param data_len 要发送的数据长度
 * @return 1 - 发送成功，0 - 发送失败
 * @note 发送数据前需要先发送 AT+CIPSEND 命令，等待 ">" 响应后再发送数据
 */
uint8_t WiFi_SendDataToLink(uint8_t link_id, const char *data, uint16_t data_len)
{
    char cmd[32];  
    uint8_t retry;  
    for (retry = 0; retry < 3; retry++)
    {
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u,%u", link_id, data_len);
        if (WiFi_SendCmd(cmd, ">", 2000))
        {
            HAL_UART_Transmit(&huart1, (uint8_t *)data, data_len, 500);

            uint32_t wait_start = HAL_GetTick();
            while ((HAL_GetTick() - wait_start) < 500)
            {
                __disable_irq();
                uint16_t idx = wifi_buffer_index;
                __enable_irq();
                if (idx > 4)
                {
                    HAL_Delay(5);
                    __disable_irq();
                    idx = wifi_buffer_index;
                    __enable_irq();
                    if (idx > 0 && idx < 510)
                    {
                        __disable_irq();
                        char *buf = (char *)wifi_buffer;
                        buf[idx] = '\0';
                        __enable_irq();
                        if (strstr(buf, "SEND OK") || strstr(buf, "OK"))
                        {
                            WIFI_LOG("Link%u sent %u bytes OK", link_id, data_len);
                            __disable_irq();
                            wifi_buffer_index = 0;
                            __enable_irq();
                            return 1;
                        }
                    }
                }
                HAL_Delay(2);
            }

            WIFI_LOG("Link%u send no SEND OK, assuming success", link_id);
            __disable_irq();
            wifi_buffer_index = 0;
            __enable_irq();
            return 1;
        }
        HAL_Delay(20);
    }
    WIFI_LOG("Link%u send FAILED after 3 tries", link_id);
    return 0;
}

/**
 * @brief 发送完整的 HTTP 响应
 * @details 该函数构造 HTTP 响应头和 JSON 响应体，并通过指定的连接 ID 发送
 * @param link_id 连接 ID（0-4）
 * @param json_body JSON 格式的响应体字符串
 * @return 无
 * @note HTTP 响应包含 CORS 头，允许跨域访问
 */
static void WiFi_SendFullHTTPResponse(uint8_t link_id, const char *json_body)
{
    uint16_t body_len = strlen(json_body);

    int header_len = snprintf(full_response_buf, sizeof(full_response_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %u\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        body_len);

    if (header_len < 0 || header_len >= (int)sizeof(full_response_buf))
    {
        WIFI_LOG("Header build FAILED");
        return;
    }

    uint16_t total_len = (uint16_t)header_len + body_len;
    if (total_len >= sizeof(full_response_buf))
    {
        WIFI_LOG("Response too large: %u bytes", total_len);
        return;
    }

    memcpy(full_response_buf + header_len, json_body, body_len);
    full_response_buf[total_len] = '\0';

    WIFI_LOG("Sending full response: %u bytes (hdr=%u body=%u)", total_len, header_len, body_len);
    WiFi_SendDataToLink(link_id, full_response_buf, total_len);
}

/**
 * @brief 解析无符号整数
 * @details 该函数从字符串中解析无符号 8 位整数，遇到非数字字符时停止
 * @param str 要解析的字符串指针
 * @return 解析出的无符号 8 位整数
 * @note 如果字符串不以数字开头，则返回 0
 */
static uint8_t WiFi_ParseUint(const char *str)
{
    uint8_t val = 0;  
    while (*str >= '0' && *str <= '9')
    {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val;
}

/**
 * @brief 解析有符号整数
 * @details 该函数从字符串中解析有符号 16 位整数，支持负数，遇到非数字字符时停止
 * @param str 要解析的字符串指针
 * @return 解析出的有符号 16 位整数
 * @note 如果字符串以 '-' 开头，则返回负数
 */
static int16_t WiFi_ParseInt(const char *str)
{
    int16_t val = 0;  
    uint8_t neg = 0; 
    if (*str == '-') { neg = 1; str++; }
    while (*str >= '0' && *str <= '9')
    {
        val = val * 10 + (*str - '0');
        str++;
    }
    return neg ? -val : val;
}

/**
 * @brief 处理 API 请求
 * @details 该函数解析 HTTP 请求中的 URI，根据不同的 API 端点执行相应的操作，
 *          包括状态查询、药盒控制、音量控制、提醒设置、阈值设置等
 * @param link_id 连接 ID（0-4）
 * @param uri HTTP 请求的 URI 字符串
 * @return 无
 * @note 支持的 API 端点：status、open、close、volume、reminder、threshold、led、fan、heater
 */
static void WiFi_HandleAPIRequest(uint8_t link_id, const char *uri)
{
    const char *api_uri = uri;
    if (uri[0] == '/') api_uri = uri + 1;

    WIFI_LOG("API: %s (from %s)", api_uri, uri);

    if (strcmp(api_uri, "api/status") == 0 || strcmp(api_uri, "status") == 0)
    {
        Actuator_Thresholds th;
        Actuator_GetThresholds(&th);

        snprintf(api_json_buf, sizeof(api_json_buf),
            "{\"code\":0,\"msg\":\"ok\",\"data\":{"
            "\"temp\":%u,"
            "\"hum\":%u,"
            "\"medicine\":%u,"
            "\"light\":%u,"
            "\"volume\":%u,"
            "\"battery\":%u,"
            "\"voltage\":%u,"
            "\"led\":%u,"
            "\"fan\":%u,"
            "\"heater\":%u,"
            "\"box_open\":%u,"
            "\"rtc_synced\":%u,"
            "\"temp_high\":%u,"
            "\"temp_low\":%u,"
            "\"light_on\":%u,"
            "\"light_off\":%u"
            "}}",
            device_status_cache.temp,
            device_status_cache.hum,
            device_status_cache.medicine_percent,
            device_status_cache.light_percent,
            device_status_cache.volume,
            device_status_cache.battery_soc,
            device_status_cache.battery_voltage,
            device_status_cache.led_state,
            device_status_cache.fan_state,
            device_status_cache.heater_state,
            device_status_cache.box_open,
            device_status_cache.rtc_synced,
            th.temp_high,
            th.temp_low,
            th.light_on,
            th.light_off);
        WiFi_SendFullHTTPResponse(link_id, api_json_buf);
    }
    else if (strcmp(api_uri, "api/open") == 0 || strcmp(api_uri, "open") == 0)
    {
        wifi_cmd_buf[0] = 'O';
        wifi_cmd_buf[1] = '\0';
        wifi_cmd_len = 1;
        WIFI_LOG("CMD: Open pillbox");
        WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"open\"}");
    }
    else if (strcmp(api_uri, "api/close") == 0 || strcmp(api_uri, "close") == 0)
    {
        wifi_cmd_buf[0] = 'C';
        wifi_cmd_buf[1] = '\0';
        wifi_cmd_len = 1;
        WIFI_LOG("CMD: Close pillbox");
        WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"close\"}");
    }
    else if (strcmp(api_uri, "api/volume/up") == 0 || strcmp(api_uri, "volume/up") == 0)
    {
        wifi_cmd_buf[0] = 'U';
        wifi_cmd_buf[1] = '\0';
        wifi_cmd_len = 1;
        WIFI_LOG("CMD: Volume up");
        WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"volume_up\"}");
    }
    else if (strcmp(api_uri, "api/volume/down") == 0 || strcmp(api_uri, "volume/down") == 0)
    {
        wifi_cmd_buf[0] = 'D';
        wifi_cmd_buf[1] = '\0';
        wifi_cmd_len = 1;
        WIFI_LOG("CMD: Volume down");
        WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"volume_down\"}");
    }
    else if (strncmp(api_uri, "api/volume?set=", 15) == 0 || strncmp(api_uri, "volume?set=", 13) == 0)
    {
        const char *val_str = (api_uri[0] == 'a') ? api_uri + 15 : api_uri + 13;
        uint8_t vol = WiFi_ParseUint(val_str);
        wifi_cmd_buf[0] = 'V';
        wifi_cmd_buf[1] = vol;
        wifi_cmd_buf[2] = '\0';
        wifi_cmd_len = 2;
        WIFI_LOG("CMD: Set volume=%u", vol);
        snprintf(api_json_buf, sizeof(api_json_buf),
            "{\"code\":0,\"msg\":\"ok\",\"volume\":%u}", vol);
        WiFi_SendFullHTTPResponse(link_id, api_json_buf);
    }
    else if (strncmp(api_uri, "api/reminder?set=", 17) == 0 || strncmp(api_uri, "reminder?set=", 14) == 0)
    {
        const char *params = (api_uri[0] == 'a') ? api_uri + 17 : api_uri + 14;
        uint8_t idx = 0, h = 0, m = 0;

        if (params[0] >= '0' && params[0] <= '2')
        {
            idx = params[0] - '0';
            params++;
            if (*params == '&') params++;
        }

        if (strncmp(params, "h=", 2) == 0)
        {
            h = WiFi_ParseUint(params + 2);
            params = strchr(params, '&');
            if (params) params++;
        }

        if (params && strncmp(params, "m=", 2) == 0)
        {
            m = WiFi_ParseUint(params + 2);
        }

        if (idx < TTS_REMINDER_COUNT)
        {
            TTS_SetReminder(idx, h, m);
            WIFI_LOG("CMD: Set reminder[%u]=%02u:%02u", idx, h, m);
            snprintf(api_json_buf, sizeof(api_json_buf),
                "{\"code\":0,\"msg\":\"ok\",\"reminder\":%u,\"hour\":%u,\"minute\":%u}",
                idx, h, m);
        }
        else
        {
            WIFI_LOG("CMD: Invalid reminder index=%u", idx);
            snprintf(api_json_buf, sizeof(api_json_buf),
                "{\"code\":400,\"msg\":\"invalid index\"}");
        }
        WiFi_SendFullHTTPResponse(link_id, api_json_buf);
    }
    else if (strncmp(api_uri, "api/threshold?", 14) == 0 || strncmp(api_uri, "threshold?", 12) == 0)
    {
        int16_t temp_high = TEMP_HIGH_THRESHOLD;
        const char *ph = strstr(api_uri, "temp_high=");
        int16_t temp_low = TEMP_LOW_THRESHOLD;
        int16_t light_on = LIGHT_ON_THRESHOLD;
        int16_t light_off = LIGHT_OFF_THRESHOLD;

        const char *ph_high = strstr(uri, "temp_high=");
        if (ph_high) temp_high = WiFi_ParseInt(ph_high + 10);

        const char *pl_low = strstr(uri, "temp_low=");
        if (pl_low) temp_low = WiFi_ParseInt(pl_low + 9);

        const char *lo = strstr(uri, "light_on=");
        if (lo) light_on = WiFi_ParseInt(lo + 9);

        const char *lf = strstr(uri, "light_off=");
        if (lf) light_off = WiFi_ParseInt(lf + 10);

        WIFI_LOG("THRESHOLD parsed: temp=%d/%d light=%d/%d",
                 temp_high, temp_low, light_on, light_off);
        Actuator_Thresholds th;
        th.temp_high = (uint8_t)temp_high;
        th.temp_low = (uint8_t)temp_low;
        th.light_on = (uint8_t)light_on;
        th.light_off = (uint8_t)light_off;
        Actuator_SetThresholds(&th);

        snprintf(api_json_buf, sizeof(api_json_buf),
            "{\"code\":0,\"msg\":\"ok\",\"temp_high\":%u,\"temp_low\":%u,\"light_on\":%u,\"light_off\":%u}",
            th.temp_high, th.temp_low, th.light_on, th.light_off);
        WiFi_SendFullHTTPResponse(link_id, api_json_buf);
    }
    else if (strncmp(api_uri, "api/settime?", 12) == 0 || strncmp(api_uri, "settime?", 9) == 0)
    {
        uint8_t y = 0, mo = 0, d = 0, h = 0, m = 0, s = 0;

        const char *py = strstr(uri, "y=");
        if (py) y = WiFi_ParseUint(py + 2);
        const char *pmo = strstr(uri, "mo=");
        if (pmo) mo = WiFi_ParseUint(pmo + 3);
        const char *pd = strstr(uri, "d=");
        if (pd) d = WiFi_ParseUint(pd + 2);
        const char *ph = strstr(uri, "h=");
        if (ph) h = WiFi_ParseUint(ph + 2);
        const char *pm = strstr(uri, "m=");
        if (pm) m = WiFi_ParseUint(pm + 2);
        const char *ps = strstr(uri, "s=");
        if (ps) s = WiFi_ParseUint(ps + 2);

        RTC_SetDateTime(y, mo, d, h, m, s);
        WIFI_LOG("TIME SET: %02u-%02u-%02u %02u:%02u:%02u", y, mo, d, h, m, s);

        snprintf(api_json_buf, sizeof(api_json_buf),
            "{\"code\":0,\"msg\":\"ok\",\"time\":\"%02u-%02u-%02u %02u:%02u:%02u\"}",
            y, mo, d, h, m, s);
        WiFi_SendFullHTTPResponse(link_id, api_json_buf);
    }
    else
    {
        WIFI_LOG("API 404: %s", uri);
        WiFi_SendFullHTTPResponse(link_id, "{\"code\":404,\"msg\":\"not found\"}");
    }
}

/**
 * @brief 处理 WiFi 接收缓冲区
 * @details 该函数在主循环中定期调用，处理 WiFi 模块接收到的数据，
 *          包括 HTTP 请求解析、API 请求处理、WiFi 事件处理等
 * @param 无
 * @return 无
 * @note 函数会清空接收缓冲区，为下次接收做准备
 */
void WiFi_ProcessBuffer(void)
{
    __disable_irq();
    uint16_t idx = wifi_buffer_index;
    __enable_irq();

    if (idx == 0) return;

    HAL_Delay(5);

    __disable_irq();
    idx = wifi_buffer_index;
    if (idx == 0)
    {
        __enable_irq();
        return;
    }
    if (idx >= sizeof(wifi_buffer) - 1)
    {
        WIFI_LOG("Buffer overflow! idx=%u, clearing", idx);
        wifi_buffer_index = 0;
        __enable_irq();
        return;
    }
    ((char *)wifi_buffer)[idx] = '\0';
    wifi_buffer_index = 0;
    __enable_irq();

    char *buf = (char *)wifi_buffer;

    char *http_get = strstr(buf, "+IPD,");
    if (http_get)
    {
        uint8_t link_id = 0;
        char *comma = strchr(http_get + 5, ',');
        if (comma)
        {
            link_id = (uint8_t)(*(http_get + 5) - '0');
            if (link_id > 4) link_id = 0;
        }

        WIFI_LOG("HTTP from Link%u", link_id);

        char *req_pos = strstr(http_get, "GET /");
        if (!req_pos)
        {
            req_pos = strstr(http_get, "POST /");
        }

        if (req_pos)
        {
            char uri[64] = {0};
            char *sp = strchr(req_pos + 5, ' ');
            if (sp)
            {
                uint16_t uri_len = sp - (req_pos + 5);
                if (uri_len >= sizeof(uri)) uri_len = sizeof(uri) - 1;
                memcpy(uri, req_pos + 5, uri_len);
                uri[uri_len] = '\0';
            }

            WIFI_LOG("Link%u URI: %s", link_id, uri);

            const char *api_uri = uri;
            if (uri[0] == '/') api_uri = uri + 1;

            if (strncmp(api_uri, "api/", 4) == 0)
            {
                WiFi_HandleAPIRequest(link_id, api_uri);
            }
            else
            {
                wifi_cmd_len = 0;
                if (strcmp(api_uri, "open") == 0)
                {
                    wifi_cmd_buf[0] = 'O';
                    wifi_cmd_buf[1] = '\0';
                    wifi_cmd_len = 1;
                    WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"open\"}");
                }
                else if (strcmp(api_uri, "close") == 0)
                {
                    wifi_cmd_buf[0] = 'C';
                    wifi_cmd_buf[1] = '\0';
                    wifi_cmd_len = 1;
                    WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"close\"}");
                }
                else if (strcmp(api_uri, "volup") == 0)
                {
                    wifi_cmd_buf[0] = 'U';
                    wifi_cmd_buf[1] = '\0';
                    wifi_cmd_len = 1;
                    WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"volume_up\"}");
                }
                else if (strcmp(api_uri, "voldown") == 0)
                {
                    wifi_cmd_buf[0] = 'D';
                    wifi_cmd_buf[1] = '\0';
                    wifi_cmd_len = 1;
                    WiFi_SendFullHTTPResponse(link_id, "{\"code\":0,\"msg\":\"ok\",\"action\":\"volume_down\"}");
                }
                else if (strncmp(api_uri, "settime?", 9) == 0)
                {
                    uint8_t y = 0, mo = 0, d = 0, h = 0, m = 0, ss = 0;
                    const char *py = strstr(uri, "y=");
                    if (py) y = WiFi_ParseUint(py + 2);
                    const char *pmo = strstr(uri, "mo=");
                    if (pmo) mo = WiFi_ParseUint(pmo + 3);
                    const char *pd = strstr(uri, "d=");
                    if (pd) d = WiFi_ParseUint(pd + 2);
                    const char *ph = strstr(uri, "h=");
                    if (ph) h = WiFi_ParseUint(ph + 2);
                    const char *pm = strstr(uri, "m=");
                    if (pm) m = WiFi_ParseUint(pm + 2);
                    const char *ps = strstr(uri, "s=");
                    if (ps) ss = WiFi_ParseUint(ps + 2);

                    RTC_SetDateTime(y, mo, d, h, m, ss);
                    WIFI_LOG("TIME SET: %02u-%02u-%02u %02u:%02u:%02u", y, mo, d, h, m, ss);

                    snprintf(api_json_buf, sizeof(api_json_buf),
                        "{\"code\":0,\"msg\":\"ok\",\"time\":\"%02u-%02u-%02u %02u:%02u:%02u\"}",
                        y, mo, d, h, m, ss);
                    WiFi_SendFullHTTPResponse(link_id, api_json_buf);
                }
                else
                {
                    WIFI_LOG("Unknown URI: %s", uri);
                    WiFi_SendFullHTTPResponse(link_id, "{\"code\":404,\"msg\":\"unknown command\"}");
                }
            }
        }
        else
        {
            WIFI_LOG("Link%u: No HTTP method found", link_id);
        }
    }
    else if (strstr(buf, "WIFI CONNECTED"))
    {
        WIFI_LOG("Event: WIFI CONNECTED");
    }
    else if (strstr(buf, "WIFI GOT IP"))
    {
        WIFI_LOG("Event: WIFI GOT IP");
    }
    else if (strstr(buf, "WIFI DISCONNECT"))
    {
        WIFI_LOG("Event: WIFI DISCONNECT");
        wifi_state.connected = 0;
        wifi_state.ip[0] = '\0';
    }
    else if (strstr(buf, "ERROR"))
    {
        WIFI_LOG("Event: ESP8266 ERROR");
    }

    {
        char *p = buf;
        while ((p = strstr(p, ",CLOSED")) != NULL)
        {
            if (p > buf && *(p - 1) >= '0' && *(p - 1) <= '4')
            {
                WIFI_LOG("Link%u CLOSED", *(p - 1) - '0');
            }
            p += 7;
        }
    }
}

/**
 * @brief 获取 WiFi 状态
 * @details 该函数返回 WiFi 模块的当前状态结构体指针，
 *          包含连接状态、SSID 和 IP 地址等信息
 * @param 无
 * @return 指向 WiFi_State 结构体的指针
 * @note 返回的指针指向静态变量，调用者不应修改其内容
 */
WiFi_State* WiFi_GetState(void)
{
    // 返回 WiFi 状态结构体指针
    return &wifi_state;
}

/**
 * @brief 获取 WiFi 命令
 * @details 该函数从 WiFi 命令缓冲区获取待处理的命令，
 *          命令由 HTTP 请求解析后存入缓冲区
 * @param cmd 指向命令缓冲区的指针，用于存储获取的命令
 * @return 命令长度，如果没有命令则返回 0
 * @note 函数会清空命令长度标志，表示命令已被处理
 */
uint8_t WiFi_GetCommand(char *cmd)
{
    if (wifi_cmd_len > 0)
    {
        memcpy(cmd, wifi_cmd_buf, wifi_cmd_len + 1);
        uint16_t len = wifi_cmd_len;
        wifi_cmd_len = 0;
        return len;
    }
    return 0;
}

/**
 * @brief 更新设备状态缓存
 * @details 该函数将当前的设备状态更新到 WiFi 模块的状态缓存中，
 *          用于 API 请求时返回最新的设备状态
 * @param status 指向 Device_Status 结构体的指针，包含当前的设备状态
 * @return 无
 * @note 状态缓存用于在 API 请求时快速返回设备状态，避免频繁查询
 */
void WiFi_UpdateStatus(Device_Status *status)
{
    if (!status) return;
    device_status_cache.temp = status->temp;
    device_status_cache.hum = status->hum;
    device_status_cache.medicine_percent = status->medicine_percent;
    device_status_cache.light_percent = status->light_percent;
    device_status_cache.volume = status->volume;
    device_status_cache.battery_soc = status->battery_soc;
    device_status_cache.battery_voltage = status->battery_voltage;
    device_status_cache.led_state = status->led_state;
    device_status_cache.fan_state = status->fan_state;
    device_status_cache.heater_state = status->heater_state;
    device_status_cache.box_open = status->box_open;
    device_status_cache.rtc_synced = status->rtc_synced;
}
