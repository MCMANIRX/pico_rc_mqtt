#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt_priv.h"
#include "tusb.h"

//home/fd/pico-sdk-develop/lib/mbedtls/library/mbedtls_config.h

#define DEBUG_printf printf

//#define WIFI_SSID "XboxOne"
//#define WIFI_PASSWORD "ctgprshouldbearomhack"

 char ssid[] = "XboxOne";
 char pass[] = "ctgprshouldbearomhack";

typedef struct MQTT_CLIENT_DATA_T {
    mqtt_client_t *mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    uint8_t data[MQTT_OUTPUT_RINGBUF_SIZE];
    uint8_t topic[100];
    uint32_t len;
} MQTT_CLIENT_DATA_T;

MQTT_CLIENT_DATA_T *mqtt;

void blink(uint8_t x, uint8_t delay) {

    for(int i = 0; i < x; ++i){
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }}

struct mqtt_connect_client_info_t mqtt_client_info =
{
  "picow",
  "mqtt", /* user */
  "", /* pass */
  0,  /* keep alive */
  NULL, /* will_topic */
  NULL, /* will_msg */
  0,    /* will_qos */
  0     /* will_retain */
#if LWIP_ALTCP && LWIP_ALTCP_TLS
  , NULL
#endif
};

/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result)
{
  if(result != ERR_OK) {
    printf("Publish result: %d\n", result);
  }
}



err_t example_publish(mqtt_client_t *client, void *arg)
{
  const char *pub_payload= "Picow MQTT";
  err_t err;
  u8_t qos = 2; /* 0 1 or 2, see MQTT specification */
  u8_t retain = 0; /* No don't retain such crappy payload... */
  cyw43_arch_lwip_begin();
  err = mqtt_publish(client, "pub_topic", pub_payload, strlen(pub_payload), qos, retain, mqtt_pub_request_cb, arg);
  cyw43_arch_lwip_end();
  if(err != ERR_OK) {
    printf("Publish err: %d\n", err);
  }
  return err;
}


static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    printf("mqtt_incoming_data_cb\n");
    MQTT_CLIENT_DATA_T* mqtt_client = (MQTT_CLIENT_DATA_T*)arg;
    LWIP_UNUSED_ARG(data);
    strncpy((char*)mqtt_client->data, (char*)data, len);

    mqtt_client->len=len;
    mqtt_client->data[len]='\0';

 
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  MQTT_CLIENT_DATA_T* mqtt_client = (MQTT_CLIENT_DATA_T*)arg;
  strcpy((char*)mqtt_client->topic, (char*)topic);

  if (strcmp((const char*)mqtt->topic, "stop") == 0) {
    printf("STOP\n");
  }
  if (strcmp((const char*)mqtt->topic, "start") == 0) {
    printf("START\n");
  }

}

static void mqtt_request_cb(void *arg, err_t err) {
  MQTT_CLIENT_DATA_T* mqtt_client = ( MQTT_CLIENT_DATA_T*)arg;

  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" request cb: err %d\n", mqtt_client->mqtt_client_info.client_id, (int)err));
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  MQTT_CLIENT_DATA_T* mqtt_client = (MQTT_CLIENT_DATA_T*)arg;
  LWIP_UNUSED_ARG(client);

  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" connection cb: status %d\n", mqtt_client->mqtt_client_info.client_id, (int)status));

  if (status == MQTT_CONNECT_ACCEPTED) {
    printf("MQTT_CONNECT_ACCEPTED\n");

    //example_publish(client, arg);
    //mqtt_disconnect(client);

    mqtt_sub_unsub(client,
            "start", 0,
            mqtt_request_cb, arg,
            1);
    mqtt_sub_unsub(client,
            "stop", 0,
            mqtt_request_cb, arg,
            1);
        
  }
}

int main()
{
  stdio_init_all();
    int sm = 0;


    mqtt=(MQTT_CLIENT_DATA_T*)calloc(1, sizeof(MQTT_CLIENT_DATA_T));

    if (!mqtt) {
        printf("mqtt client instant ini error\n");
        return 0;
    }

    mqtt->mqtt_client_info = mqtt_client_info;

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
    printf("connected to Wi-Fi\n");
    ip_addr_t ip = cyw43_state.netif[0].ip_addr;

    blink(4,100);

    // Following strings are the components of ifcongig[]
    printf("connected as %s\n",ip4addr_ntoa(&ip));
    printf("subnetmask: %s\n",ip4addr_ntoa(&cyw43_state.netif[0].netmask));
    printf("gateway: %s\n",ip4addr_ntoa((ip_addr_t*)&cyw43_state.netif[0].gw.addr));
    printf("DNS: %s\n",ip4addr_ntoa(dns_getserver(0)));


    ip_addr_t addr;
    IP4_ADDR(&addr, 192, 168, 1, 137);


   
    mqtt->mqtt_client_inst = mqtt_client_new();
    mqtt_set_inpub_callback(mqtt->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, mqtt);
 
    err_t err = mqtt_client_connect(mqtt->mqtt_client_inst, &addr, MQTT_PORT, &mqtt_connection_cb, mqtt, &mqtt->mqtt_client_info);
    sleep_ms(1000);
    if (err != ERR_OK) {
      printf("connect error\n");
      return 0;
    }  

    
      while(1) {
        example_publish(mqtt->mqtt_client_inst, mqtt);
        sleep_ms(1000);
   }
   
   
    return 0;
}