/*

QoS 0 (At most once):

This is the lowest level of service.
The message is delivered at most once, and delivery is not confirmed.
There is no guarantee that the message will be delivered, and it is possible that messages may be lost.

QoS 1 (At least once):

This level guarantees that the message will be delivered at least once to the receiver.
The sender (publisher) sends the message and expects an acknowledgment (ACK) from the receiver (subscriber).
If an ACK is not received, the sender will resend the message.
This level introduces the possibility of duplicate messages in case of network issues.

QoS 2 (Exactly once):

This is the highest level of service.
It ensures that the message is delivered exactly once to the receiver.
It involves a four-step handshake process: publish, PUBREC (publish received), PUBREL (publish release), and PUBCOMP (publish complete).
This level provides the strongest guarantee of message delivery but involves more overhead and complexity.

*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/apps/mqtt.h"
#include "rc_config.h"
#include "queue.h"

err_t err;
ip_addr_t ip;

MESSAGE rcv_buf;
mqtt_client_t *client;
struct mqtt_connect_client_info_t ci;

char *_id;  //MQTT client id for reconnect
int (*_ext_callback)(); //ptr for callback fcn local to main.c

char ssid[] = "XboxOne";
char pass[] = "ctgprshouldbearomhack";


u8_t qos = 0;


/// @brief 
/// @param count
/// @param delay 
void blink(uint8_t count, int delay)
{

    for (int i = 0; i < count; ++i)
    {
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }
}

// #################### CALLBACKS ####################
static void pub_request_cb(void *arg, err_t result)
{
    // printf("Publish result: %d\n", result);
    mqtt_client_t *client = (mqtt_client_t *)arg;
}

void sub_request_cb(void *arg, err_t result)
{
    ///   printf("Subscribe result: %d\n", result);
}

static void incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{

    tot_len = strlen(topic);
    memcpy(rcv_buf.topic, topic, tot_len);
    //    printf("Topic %s. %d\n", rcv_buf.topic, tot_len);
    //  printf("Topic %s. %d\n", topic, tot_len);
}

static void incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    memcpy(rcv_buf.data, data, len);

     //call external callback in main.c
    _ext_callback(rcv_buf.topic, rcv_buf.data);
    memset(rcv_buf.topic,0,TOPIC_SIZE);
    memset(rcv_buf.data,0,MESSAGE_SIZE);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        blink(3, 50);

        mqtt_set_inpub_callback(client, incoming_publish_cb, incoming_data_cb, arg);
    }

    else{

        //commented out wi-fi reconnect, throws CYW43 error and hangs
       /* if(cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP)
            wifi_init();*/
        
        /* try reconnect */
        while(connect(_id)!=ERR_OK) {

            blink(6, 50);
            sleep_ms(500);

        }

    }
}
// ###################################################

/// @brief Connects locally defined callback to backend defined in this header file.
void set_message_decoder(void (*ext_callback)(char *topic, char *data))
{

    _ext_callback = ext_callback;
}

int subscribe(char *topic)
{

    return mqtt_subscribe(client, topic, 1, sub_request_cb, NULL);
}

int unsubscribe(char *topic)
{

    return mqtt_unsubscribe(client, topic, sub_request_cb, NULL);
}


int publish(char *topic, char *payload)
{

    // u8_t qos = 2;        //changed to 0, 2 has issues with pico
    // u8_t retain = 0;

    return mqtt_publish(client, topic, payload, strlen(payload), qos, 0, pub_request_cb, NULL);
}


int publish_with_strlen(char *topic, char *payload, int len)
{

    // u8_t qos = 2;        //changed to 0, 2 has issues with pico
    // u8_t retain = 0;

    return mqtt_publish(client, topic, payload, len, qos, 0, pub_request_cb, NULL);
}

/// @brief redundant, might be deleted soon but will try setting global qos to 1?
int publish_with_strlen_qos0(char *topic, char *payload,int len)
{

    // u8_t qos = 0;
    // u8_t retain = 0;

    return mqtt_publish(client, topic, payload, len, 0, 0, pub_request_cb, NULL);
}

void set_id_for_reconnect(char *id) {
    _id = id;
}

// ################## Connectivity ###################

int32_t rssi;
int get_rssi() {

   return cyw43_ioctl(&cyw43_state, 254, sizeof rssi, (uint8_t *)&rssi, CYW43_ITF_STA);

}

static int wifi_init()
{

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
    {
        printf("failed to initialise\n");
        return 1;
    }
    printf("initialised\n");

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        blink(2,500);
        printf("failed to connect\n");

        if(cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP)
            wifi_init();
    }

    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP)
    {
        printf("tcp not connected, status = %d\n", cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA));
        sleep_ms(500);
        wifi_init();
    }

    printf("connected to Wi-Fi\n");
    ip_addr_t ip = cyw43_state.netif[0].ip_addr;

    blink(4, 100);

    // Following strings are the components of ifcongig[]
    printf("connected as %s\n", ip4addr_ntoa(&ip));
    printf("subnetmask: %s\n", ip4addr_ntoa(&cyw43_state.netif[0].netmask));
    printf("gateway: %s\n", ip4addr_ntoa((ip_addr_t *)&cyw43_state.netif[0].gw.addr));
    printf("DNS: %s\n", ip4addr_ntoa(dns_getserver(0)));

    get_rssi();
    printf("RSSI: %d\n",rssi);

    

    return 0;
}

int connect(char *id)
{
    mqtt_client_t *_client = mqtt_client_new();
    client = _client;
    ci.client_id = id; //"rc_unassigned";

    mqtt_client_connect(client, &ip, 1883, mqtt_connection_cb, 0, &ci);
    return subscribe(CTRL_TOPIC);
}

void disconnect()
{
    unsubscribe(CTRL_TOPIC);
    mqtt_disconnect(client);
}
// ###################################################

static int _mqtt_init(char *id)
{

    if (!ip4addr_aton(STATIC_HOST_IP, &ip))
    {
        printf("ip error\n");
        return 0;
    }

    memset(&ci, 0, sizeof(ci));

    return connect(id);
}

int mqtt_init(char *id)
{

    wifi_init();

    _mqtt_init(id);
    set_id_for_reconnect(id);

}

