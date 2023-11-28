#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "mqtt.h"
#include "rc_config.h"

int main() {

    stdio_init_all();

    if(!init_mqtt())
        while(1) {
    
    sleep_ms(1000);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    sleep_ms(1000);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
    
    else 
        printf("\nerror. couldn't connect.\n");

    return 0;
}