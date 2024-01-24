#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/mqtt.h"
char ssid[] = "XboxOne";
char pass[] = "ctgprshouldbearomhack";
int mqttStatus = 0;

void blink(uint8_t x, uint8_t delay) {

    for(int i = 0; i < x; ++i){
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }
}

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        mqttStatus = 1;
    }
    printf("yo!");
}

static void pub_request_cb(void *arg, err_t result)
{
    printf("Publish result: %d\n", result);
    mqtt_client_t *client = (mqtt_client_t *)arg;
}

void sub_request_cb(void *arg, err_t result)
{
    printf("Subscribe result: %d\n", result);
    mqttStatus = 2;
}

static void incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    printf("Topic %s. %d\n", topic, tot_len);
}

static void incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    char *payload = (char *)data;
    printf("payload\n%.*s\n", len, payload);
}

int wifi_init() {


    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
      printf("failed to initialise\n");
      return 1;
    }
      printf("initialised\n");

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
      printf("failed to connect\n");
      return 1;
    }

    if(cyw43_tcpip_link_status(&cyw43_state,CYW43_ITF_STA)!=CYW43_LINK_UP){
		  printf("tcp not connected, status = %d\n",cyw43_tcpip_link_status(&cyw43_state,CYW43_ITF_STA));
      sleep_ms(500);
      wifi_init();
    }

    printf("connected to Wi-Fi\n");
    ip_addr_t ip = cyw43_state.netif[0].ip_addr;

    blink(4,100);

    // Following strings are the components of ifcongig[]
    printf("connected as %s\n",ip4addr_ntoa(&ip));
    printf("subnetmask: %s\n",ip4addr_ntoa(&cyw43_state.netif[0].netmask));
    printf("gateway: %s\n",ip4addr_ntoa((ip_addr_t*)&cyw43_state.netif[0].gw.addr));
    printf("DNS: %s\n",ip4addr_ntoa(dns_getserver(0)));

    return 0;

}

int main()
{
    stdio_init_all();
    wifi_init();

    mqtt_client_t *client = mqtt_client_new();

    struct mqtt_connect_client_info_t ci;
    memset(&ci, 0, sizeof(ci));
    ci.client_id = "MyPicoW";
   // ci.keep_alive = 10;

    ip_addr_t ip;
    IP4_ADDR(&ip, 192, 168, 1, 137);
    // IP4_ADDR(&ip, 20, 79, 70, 109);


    err_t err = mqtt_client_connect(client, &ip, 1883, mqtt_connection_cb, 0, &ci);

    char payload[] = "Hello MQTT World";
    u8_t qos = 2;
    u8_t retain = 0;
    while (true)
    {
        switch (mqttStatus)
        {
        case 0:
            break;
        case 1:
            mqtt_set_inpub_callback(client, incoming_publish_cb, incoming_data_cb, NULL);
            err = mqtt_subscribe(client, "MyTopic", 1, sub_request_cb, NULL);
            break;

        case 2:
            cyw43_arch_lwip_begin();
            err = mqtt_publish(client, "MyTopic", payload, strlen(payload), qos, retain, pub_request_cb, client);
            cyw43_arch_lwip_end();

            break;
        }
        sleep_ms(2000);
    }
}