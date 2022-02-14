/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "general.h"
#include "gdb_if.h"
#include "version.h"

#include "gdb_packet.h"
#include "gdb_main.h"
#include "target.h"
#include "exception.h"
#include "gdb_packet.h"
#include "morse.h"
#include "platform.h"
#include "CBUF.h"

#include "tinyprintf.h"

#include <assert.h>
#include <sys/time.h>
#include <sys/unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "dhcpserver/dhcpserver.h"
//#include <sysparam.h>
#include "http.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"
#include "lwip/tcp.h"

#include "esp32/rom/ets_sys.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "soc/uart_reg.h"
#include "driver/gpio.h"
// #include "esp8266/uart_register.h"

#include <lwip/sockets.h>

#include "ota-tftp.h"
#include "wifi.h"

void platform_max_frequency_set(uint32_t freq)
{
    if (freq < 100)
    {
        return;
    }
    if (freq > 48 * 1000 * 1000)
    {
        return;
    }
    int swdptap_set_frequency(uint32_t frequency);
    int actual_frequency = swdptap_set_frequency(freq);
    ESP_LOGI(__func__, "freq:%u", actual_frequency);
}

uint32_t platform_max_frequency_get(void)
{
    int swdptap_get_frequency(void);
    return swdptap_get_frequency();
}

nvs_handle h_nvs_conf;

uint32_t uart_overrun_cnt;
uint32_t uart_errors;
uint32_t uart_queue_full_cnt;
uint32_t uart_rx_count;
uint32_t uart_tx_count;

int tcp_serv_sock;
int udp_serv_sock;
int tcp_client_sock = 0;

static xQueueHandle uart_event_queue;
static struct sockaddr_in udp_peer_addr;

struct
{
    volatile uint8_t m_get_idx;
    volatile uint8_t m_put_idx;
    uint8_t m_entry[256];
    SemaphoreHandle_t sem;
} dbg_msg_queue;

void platform_init(void)
{
    {
        gpio_config_t gpio_conf = {
            .pin_bit_mask = BIT64(CONFIG_TMS_SWDIO_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = 0,
            .pull_down_en = 0,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&gpio_conf);
        gpio_set_level(CONFIG_TMS_SWDIO_GPIO, 1);
    }
    {
        gpio_config_t gpio_conf = {
            .pin_bit_mask = BIT64(CONFIG_TCK_SWCLK_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = 0,
            .pull_down_en = 0,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&gpio_conf);
        gpio_set_level(CONFIG_TMS_SWDIO_GPIO, 1);
    }
    {
        gpio_config_t gpio_conf = {
            .pin_bit_mask = BIT64(CONFIG_SRST_GPIO),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = 0,
            .pull_down_en = 0,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&gpio_conf);
    }
}

void platform_buffer_flush(void)
{
    ;
}

void platform_srst_set_val(bool assert)
{
    if (assert)
    {
        gpio_set_direction(CONFIG_SRST_GPIO, GPIO_OUTPUT);
        gpio_set_level(CONFIG_SRST_GPIO, 0);
    }
    else
    {
        gpio_set_level(CONFIG_SRST_GPIO, 1);
        gpio_set_direction(CONFIG_SRST_GPIO, GPIO_INPUT);
    }
}

bool platform_srst_get_val(void)
{
    return !gpio_get_level(CONFIG_SRST_GPIO);
}

const char *
platform_target_voltage(void)
{
    static char voltage[16];
    extern uint16_t rom_phy_get_vdd33(void);
    int vdd = rom_phy_get_vdd33();
    sprintf(voltage, "%dmV", vdd);
    return voltage;
}

uint32_t platform_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

#define vTaskDelayMs(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

void platform_delay(uint32_t ms)
{
    vTaskDelayMs(ms);
}

int platform_hwversion(void)
{
    return 0;
}

void net_uart_task(void *params)
{
    tcp_serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    udp_serv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    tcp_client_sock = 0;

    int ret;

    struct sockaddr_in saddr;
    saddr.sin_addr.s_addr = 0;
    saddr.sin_port = ntohs(23);
    saddr.sin_family = AF_INET;

    bind(tcp_serv_sock, (struct sockaddr *)&saddr, sizeof(saddr));

    saddr.sin_addr.s_addr = 0;
    saddr.sin_port = ntohs(2323);
    saddr.sin_family = AF_INET;
    bind(udp_serv_sock, (struct sockaddr *)&saddr, sizeof(saddr));
    listen(tcp_serv_sock, 1);

    while (1)
    {
        fd_set fds;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(tcp_serv_sock, &fds);
        FD_SET(udp_serv_sock, &fds);
        if (tcp_client_sock)
            FD_SET(tcp_client_sock, &fds);

        int maxfd = MAX(tcp_serv_sock, MAX(udp_serv_sock, tcp_client_sock));

        if ((ret = select(maxfd + 1, &fds, NULL, NULL, &tv) > 0))
        {
            if (FD_ISSET(tcp_serv_sock, &fds))
            {
                tcp_client_sock = accept(tcp_serv_sock, 0, 0);
                if (tcp_client_sock < 0)
                {
                    ESP_LOGE(__func__, "accept() failed");
                    tcp_client_sock = 0;
                }
                else
                {
                    ESP_LOGI(__func__, "accepted tcp connection");

                    int opt = 1; /* SO_KEEPALIVE */
                    setsockopt(tcp_client_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&opt, sizeof(opt));
                    opt = 3; /* s TCP_KEEPIDLE */
                    setsockopt(tcp_client_sock, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&opt, sizeof(opt));
                    opt = 1; /* s TCP_KEEPINTVL */
                    setsockopt(tcp_client_sock, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&opt, sizeof(opt));
                    opt = 3; /* TCP_KEEPCNT */
                    setsockopt(tcp_client_sock, IPPROTO_TCP, TCP_KEEPCNT, (void *)&opt, sizeof(opt));
                    opt = 1;
                    setsockopt(tcp_client_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&opt, sizeof(opt));
                }
            }
            uint8_t buf[128];

            if (FD_ISSET(udp_serv_sock, &fds))
            {
                socklen_t slen = sizeof(udp_peer_addr);
                ret = recvfrom(udp_serv_sock, buf, sizeof(buf), 0, (struct sockaddr *)&udp_peer_addr, &slen);
                if (ret > 0)
                {
                    uart_write_bytes(TARGET_UART_IDX, (const char *)buf, ret);
                    uart_tx_count += ret;
                }
                else
                {
                    ESP_LOGE(__func__, "udp recvfrom() failed");
                }
            }

            if (tcp_client_sock && FD_ISSET(tcp_client_sock, &fds))
            {
                ret = recv(tcp_client_sock, buf, sizeof(buf), MSG_DONTWAIT);
                if (ret > 0)
                {
                    uart_write_bytes(TARGET_UART_IDX, (const char *)buf, ret);
                    uart_tx_count += ret;
                }
                else
                {
                    ESP_LOGE(__func__, "tcp client recv() failed (%s)", strerror(errno));
                    close(tcp_client_sock);
                    tcp_client_sock = 0;
                }
            }
        }
    }
}

void uart_rx_task(void *parameters)
{
    static uint8_t buf[TCP_MSS];
    int bufpos = 0;
    int ret;

    while (1)
    {
        uart_event_t evt;

        if (xQueueReceive(uart_event_queue, (void *)&evt, 100))
        {

            if (evt.type == UART_FIFO_OVF)
            {
                uart_overrun_cnt++;
            }
            if (evt.type == UART_FRAME_ERR)
            {
                uart_errors++;
            }
            if (evt.type == UART_BUFFER_FULL)
            {
                uart_queue_full_cnt++;
            }

            bufpos = uart_read_bytes(TARGET_UART_IDX, &buf[bufpos], sizeof(buf) - bufpos, 0);

            if (bufpos > 0)
            {
                ESP_LOGD(__func__, "uart has rx bytes: %d", bufpos);
                uart_rx_count += bufpos;
                http_term_broadcast_data(buf, bufpos);

                if (tcp_client_sock)
                {
                    ret = send(tcp_client_sock, buf, bufpos, 0);
                    if (ret < 0)
                    {
                        ESP_LOGE(__func__, "tcp send() failed (%s)", strerror(errno));
                        close(tcp_client_sock);
                        tcp_client_sock = 0;
                    }
                }

                if (udp_peer_addr.sin_addr.s_addr)
                {
                    ret = sendto(udp_serv_sock, buf, bufpos, MSG_DONTWAIT, (struct sockaddr *)&udp_peer_addr, sizeof(udp_peer_addr));
                    if (ret < 0)
                    {
                        ESP_LOGE(__func__, "udp send() failed (%s)", strerror(errno));
                        udp_peer_addr.sin_addr.s_addr = 0;
                    }
                }

                bufpos = 0;
            } // if(bufpos)
        }
    }
}

void dbg_task(void *parameters)
{
    // struct netconn* nc = netconn_new(NETCONN_UDP);
    // ip_addr_t ip;
    // IP_ADDR4(&ip, 192, 168, 4, 255);

    while (1)
    {
        xSemaphoreTake(dbg_msg_queue.sem, -1);

        while (!CBUF_IsEmpty(dbg_msg_queue))
        {
            char c = CBUF_Pop(dbg_msg_queue);
            http_debug_putc(c, c == '\n' ? 1 : 0);
        }
    }
}

void debug_putc(char c, int flush)
{
    CBUF_Push(dbg_msg_queue, c);
    if (flush)
    {
        xSemaphoreGive(dbg_msg_queue.sem);
    }
}

void platform_set_baud(uint32_t baud)
{
    uart_set_baudrate(TARGET_UART_IDX, baud);
    nvs_set_u32(h_nvs_conf, "uartbaud", baud);
}

bool cmd_setbaud(target *t, int argc, const char **argv)
{
    if (argc == 1)
    {
        uint32_t baud;
        uart_get_baudrate(TARGET_UART_IDX, &baud);
        gdb_outf("Current baud: %d\n", baud);
    }
    if (argc == 2)
    {
        int baud = atoi(argv[1]);
        gdb_outf("Setting baud: %d\n", baud);

        platform_set_baud(baud);
    }

    return 1;
}

// wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, 0)
void uart_send_break()
{
    uart_wait_tx_done(TARGET_UART_IDX, 10);

    uint32_t baud;
    uart_get_baudrate(TARGET_UART_IDX, &baud);    // save current baudrate
    uart_set_baudrate(TARGET_UART_IDX, baud / 2); // set half the baudrate
    char b = 0x00;
    uart_write_bytes(TARGET_UART_IDX, &b, 1);
    uart_wait_tx_done(TARGET_UART_IDX, 10);
    uart_set_baudrate(TARGET_UART_IDX, baud); // restore baudrate
}

int vprintf_noop(const char *s, va_list va)
{
    return 1;
}

static vprintf_like_t vprintf_orig = NULL;
static void putc_remote(void *ignored, char c)
{
    (void)ignored;
    if (c == '\n')
    {
        debug_putc('\r', 0);
    }

    debug_putc(c, c == '\n' ? 1 : 0);
}

int vprintf_remote(const char *fmt, va_list va)
{
    tfp_format(NULL, putc_remote, fmt, va);

    if (vprintf_orig)
    {
        vprintf_orig(fmt, va);
    }
    return 1;
}

extern void gdb_net_task();

void app_main(void)
{

    // Initialize debugging early, since the handover happens first.
    CBUF_Init(dbg_msg_queue);
    dbg_msg_queue.sem = xSemaphoreCreateBinary();

#ifdef CONFIG_DEBUG_UART
    vprintf_orig = esp_log_set_vprintf(vprintf_remote);
#else /* !CONFIG_DEBUG_UART */
    ESP_LOGI(__func__, "deactivating debug uart");
    esp_log_set_vprintf(vprintf_noop);
#endif

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_start();

    ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &h_nvs_conf));
    httpd_start();

    esp_wifi_set_ps(WIFI_PS_NONE);

#if defined(CONFIG_TARGET_UART)
    ESP_LOGI(__func__, "configuring UART%d for target", TARGET_UART_IDX);

    uint32_t baud = 115200;
    nvs_get_u32(h_nvs_conf, "uartbaud", &baud);

    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_driver_install(TARGET_UART_IDX, 4096, 256, 16, &uart_event_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(TARGET_UART_IDX, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(TARGET_UART_IDX, CONFIG_TARGET_UART_RX_GPIO, CONFIG_TARGET_UART_TX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    const uart_intr_config_t uart_intr = {
        .intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M | UART_RXFIFO_TOUT_INT_ENA_M | UART_FRM_ERR_INT_ENA_M | UART_RXFIFO_OVF_INT_ENA_M,
        .rxfifo_full_thresh = 80,
        .rx_timeout_thresh = 2,
        .txfifo_empty_intr_thresh = 10,
    };

    ESP_ERROR_CHECK(uart_intr_config(TARGET_UART_IDX, &uart_intr));
#endif

    platform_init();

    xTaskCreate(&dbg_task, "dbg_main", 2048, NULL, 4, NULL);
    xTaskCreate(&gdb_net_task, "gdb_net", 8192, NULL, 1, NULL);
    // xTaskCreatePinnedToCore(&gdb_net_task, "gdb_net", 2048, NULL, 1, NULL, portNUM_PROCESSORS - 1);

#if defined(CONFIG_TARGET_UART)
    xTaskCreate(&uart_rx_task, "uart_rx_task", TCP_MSS + 2048, NULL, 5, NULL);
    xTaskCreate(&net_uart_task, "net_uart_task", 1200 + 4096, NULL, 5, NULL);
#endif

    ota_tftp_init_server(69, 4);

    ESP_LOGI(__func__, "Free heap %d", esp_get_free_heap_size());
}

// #ifndef ENABLE_DEBUG
// __attribute((used)) int ets_printf(const char *__restrict c, ...)
// {
//     return 0;
// }

// __attribute((used)) int printf(const char *__restrict c, ...) { return 0; }
// #endif
