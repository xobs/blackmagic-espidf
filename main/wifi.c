#include <stdbool.h>

#include "general.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define TAG "WIFI"

#define WIFI_CONFIG_NVS_KEY "device-wifi-cfg"

struct wifi_sta_config
{
    uint8_t ssid[32];
    uint8_t password[64];
    uint8_t padding[4];
};

struct device_wifi_config
{
    // Magic value, defined as 0xaa25d294, indicating this is a valid config
    uint32_t magic;

    // Configuration for when this device is acting as an AP
    struct wifi_sta_config ap;

    // Configurations for when operating as a station
    struct wifi_sta_config sta[8];

    // A `1` bit indicates a station config is valid
    uint8_t sta_mask;
};

#define DEVICE_WIFI_CONFIG_MAGIC 0xaa25d294

static struct device_wifi_config device_wifi_config;

// case SYSTEM_EVENT_STA_START:
static void esp_system_event_sta_start(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_wifi_connect();
}
// case SYSTEM_EVENT_STA_CONNECTED:
static void esp_system_event_sta_connected(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t *event = event_data;
    ESP_LOGI(TAG, "connected:%s", event->ssid);
#ifdef CONFIG_BLACKMAGIC_HOSTNAME
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, CONFIG_BLACKMAGIC_HOSTNAME);
    ESP_LOGI(TAG, "setting hostname:%s", CONFIG_BLACKMAGIC_HOSTNAME);
#endif
}
// case SYSTEM_EVENT_STA_GOT_IP:
static void esp_system_event_sta_got_ip(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *ip = event_data;
    ESP_LOGI(TAG, "Associated IP address: " IPSTR, IP2STR(&ip->ip_info.ip));
    // xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

// case SYSTEM_EVENT_STA_DISCONNECTED:
static void esp_system_event_sta_disconnected(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *info = event_data;
    ESP_LOGE(TAG, "Disconnect reason : %d", info->reason);
    // if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
    //     /*Switch to 802.11 bgn mode */
    //     //esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
    // }
    esp_wifi_connect();
    // xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

// case SYSTEM_EVENT_AP_STACONNECTED:
static void esp_system_event_ap_staconnected(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_ap_staconnected_t *event = event_data;
    ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
             MAC2STR(event->mac),
             event->aid);
}
// case SYSTEM_EVENT_AP_STADISCONNECTED:
static void esp_system_event_ap_stadisconnected(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_ap_stadisconnected_t *event = event_data;
    ESP_LOGI(TAG, "station:" MACSTR " leave, AID=%d",
             MAC2STR(event->mac),
             event->aid);
}

static void wifi_init_softap(struct device_wifi_config *config)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    uint8_t val = 0;
    tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_ROUTER_SOLICITATION_ADDRESS, &val, sizeof(dhcps_offer_t));
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, esp_system_event_ap_staconnected, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, esp_system_event_ap_stadisconnected, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = CONFIG_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .channel = 6,
        },
    };

    memcpy(wifi_config.ap.password, config->ap.password, sizeof(wifi_config.ap.password));
    if (wifi_config.ap.password[0] == '\0')
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    memcpy(wifi_config.ap.ssid, config->ap.ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen((const char *)wifi_config.ap.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// static esp_err_t wifi_fill_sta_config(wifi_config_t *wifi_config)
// {
//     ESP_LOGI(TAG, "wifi_fill_sta_config begun.");

//     memset(wifi_config, 0, sizeof(*wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, esp_system_event_sta_start, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, esp_system_event_sta_connected, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, esp_system_event_sta_disconnected, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, esp_system_event_sta_got_ip, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, esp_system_event_ap_staconnected, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, esp_system_event_ap_stadisconnected, NULL, NULL));
//     while (1)
//     {
//         uint16_t i;
//         wifi_scan_config_t scan_config = {0};
//         ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
//         uint16_t num_aps = 0;
//         ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&num_aps));
//         wifi_ap_record_t scan_results[num_aps];
//         ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num_aps, scan_results));

//         for (i = 0; i < num_aps; i++)
//         {
//             if ((strlen(CONFIG_ESP_WIFI_SSID) > 0) && !strcmp((char *)scan_results[i].ssid, CONFIG_ESP_WIFI_SSID))
//             {
//                 // DEBUG_WARN("Connecting to %s\n", CONFIG_ESP_WIFI_SSID);
//                 strncpy((char *)wifi_config->sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_config->sta.ssid));
//                 strncpy((char *)wifi_config->sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_config->sta.password));
//                 ESP_ERROR_CHECK(esp_wifi_stop());
//                 return ESP_OK;
//             }
//             if ((strlen(CONFIG_ESP_WIFI_SSID2) > 0) && !strcmp((char *)scan_results[i].ssid, CONFIG_ESP_WIFI_SSID2))
//             {
//                 strncpy((char *)wifi_config->sta.ssid, CONFIG_ESP_WIFI_SSID2, sizeof(wifi_config->sta.ssid));
//                 strncpy((char *)wifi_config->sta.password, CONFIG_ESP_WIFI_PASSWORD2, sizeof(wifi_config->sta.password));
//                 // DEBUG_WARN("Connecting to %s (password: %s)\n", wifi_config->sta.ssid, wifi_config->sta.password);
//                 ESP_ERROR_CHECK(esp_wifi_stop());
//                 return ESP_OK;
//             }
//             if ((strlen(CONFIG_ESP_WIFI_SSID3) > 0) && !strcmp((char *)scan_results[i].ssid, CONFIG_ESP_WIFI_SSID3))
//             {
//                 // DEBUG_WARN("Connecting to %s\n", CONFIG_ESP_WIFI_SSID3);
//                 strncpy((char *)wifi_config->sta.ssid, CONFIG_ESP_WIFI_SSID3, sizeof(wifi_config->sta.ssid));
//                 strncpy((char *)wifi_config->sta.password, CONFIG_ESP_WIFI_PASSWORD3, sizeof(wifi_config->sta.password));
//                 ESP_ERROR_CHECK(esp_wifi_stop());
//                 return ESP_OK;
//             }
//             if ((strlen(CONFIG_ESP_WIFI_SSID4) > 0) && !strcmp((char *)scan_results[i].ssid, CONFIG_ESP_WIFI_SSID4))
//             {
//                 // DEBUG_WARN("Connecting to %s\n", CONFIG_ESP_WIFI_SSID4);
//                 strncpy((char *)wifi_config->sta.ssid, CONFIG_ESP_WIFI_SSID4, sizeof(wifi_config->sta.ssid));
//                 strncpy((char *)wifi_config->sta.password, CONFIG_ESP_WIFI_PASSWORD4, sizeof(wifi_config->sta.password));
//                 ESP_ERROR_CHECK(esp_wifi_stop());
//                 return ESP_OK;
//             }
//         }
//     }
// }

// static void wifi_init_sta(void)
// {
//     uint8_t mac_address[8];
//     esp_err_t result;

//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     wifi_config_t wifi_config;

//     result = esp_base_mac_addr_get(mac_address);

//     if (result == ESP_ERR_INVALID_MAC)
//     {
//         ESP_LOGE(__func__, "base mac address invalid.  reading from fuse.");
//         ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac_address));
//         ESP_ERROR_CHECK(esp_base_mac_addr_set(mac_address));
//         ESP_LOGE(__func__, "base mac address configured.");
//     }

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//     wifi_fill_sta_config(&wifi_config);

//     // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     // result = esp_base_mac_addr_get(mac_address);
//     // if (result == ESP_ERR_INVALID_MAC)
//     // {
//     //     ESP_LOGE(__func__, "base mac address invalid.  reading from fuse.");
//     //     ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac_address));
//     //     ESP_ERROR_CHECK(esp_base_mac_addr_set(mac_address));
//     //     ESP_LOGE(__func__, "base mac address configured.");
//     // }
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(__func__, "wifi_init_sta finished.");
// }

static void fill_default_wifi_config(struct device_wifi_config *config)
{
    memset(config, 0, sizeof(*config));

    if (CONFIG_ESP_WIFI_AP_SSID_APPEND_CHIPID)
    {
        uint8_t chip_id[8];
        esp_read_mac(chip_id, ESP_MAC_WIFI_SOFTAP);
        snprintf((char *)config->ap.ssid, sizeof(config->ap.ssid) - 1, CONFIG_ESP_WIFI_AP_SSID "-"
                                                                                       "%02x%02x%02x%02x%02x%02x",
                 chip_id[0], chip_id[1], chip_id[2], chip_id[3], chip_id[4], chip_id[5]);
    }
    else
    {
        snprintf((char *)config->ap.ssid, sizeof(config->ap.ssid) - 1, CONFIG_ESP_WIFI_AP_SSID);
    }

    strncpy((char *)config->ap.password, CONFIG_ESP_WIFI_AP_PASSWORD, sizeof(config->ap.password) - 1);
    config->magic = DEVICE_WIFI_CONFIG_MAGIC;
}

static void wifiTask(void *ignored)
{
    (void)ignored;
    nvs_handle h_nvs_conf;
    size_t device_wifi_length = sizeof(device_wifi_config);

    bool device_wifi_config_dirty = false;
    ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &h_nvs_conf));
    if (nvs_get_blob(h_nvs_conf, WIFI_CONFIG_NVS_KEY, &device_wifi_config, &device_wifi_length) != ESP_OK)
    {
        fill_default_wifi_config(&device_wifi_config);
        device_wifi_config_dirty = true;
    }
    if (device_wifi_config.magic != DEVICE_WIFI_CONFIG_MAGIC)
    {
        fill_default_wifi_config(&device_wifi_config);
        device_wifi_config_dirty = true;
    }

    if (device_wifi_config_dirty)
    {
        ESP_ERROR_CHECK(nvs_set_blob(h_nvs_conf, WIFI_CONFIG_NVS_KEY, &device_wifi_config, sizeof(device_wifi_config)));
    }

    while (1)
    {

        // Special case -- no configured access points so begin AP mode
        if (device_wifi_config.sta_mask == 0)
        {
            wifi_init_softap(&device_wifi_config);
            esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);
        }
        while (1) {

        taskYIELD();
        }
    }
}

esp_err_t wifi_start(void)
{
    ESP_LOGI(__func__, "wifi_init_sta beginning");
    // Init the network interface which initialises TCP/IP. This will allow other
    // components to run and create network connections even if we don't have
    // an interface up.
    esp_netif_init();

    xTaskCreate(wifiTask, "wifi manager", 8192, NULL, 3, NULL);

    // #if CONFIG_ESP_WIFI_IS_STATION
    //     wifi_init_sta();
    // #else
    //     wifi_init_softap();
    //     esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);
    // #endif
    return ESP_OK;
}