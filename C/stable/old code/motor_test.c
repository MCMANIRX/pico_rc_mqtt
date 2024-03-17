#include "hardware/pwm.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "rc_config.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/apps/mqtt.h"

int main()
{

    gpio_set_function(12, GPIO_FUNC_PWM);
    gpio_set_function(13, GPIO_FUNC_PWM);
    gpio_set_function(14, GPIO_FUNC_PWM);
    gpio_set_function(15, GPIO_FUNC_PWM);

    u16_t drive_slice = pwm_gpio_to_slice_num(12);
    u16_t steer_slice = pwm_gpio_to_slice_num(14);
while(1)
    for (int i = 12; i <= 15; ++i)
    {
        sleep_ms(1000);
        pwm_set_enabled(drive_slice, true);
        pwm_set_enabled(steer_slice, true);

       /* for (int j = 0; j < 65535; ++j){
            pwm_set_gpio_level(i, j);
            sleep_us(10);
        }*/

        for (int j = 65535; j > 1 ; --j){
            pwm_set_gpio_level(i, j);
            sleep_us(10);
        }
        pwm_set_enabled(drive_slice, false);
        pwm_set_enabled(steer_slice, false);
        sleep_ms(1000);

    }

    return 0;
}
/*

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/pwm.h"


int main() {


    gpio_set_function(12, GPIO_FUNC_PWM);
    gpio_set_function(13, GPIO_FUNC_PWM);
    gpio_set_function(14, GPIO_FUNC_PWM);
    gpio_set_function(15, GPIO_FUNC_PWM);

   /* uint slice1 = pwm_gpio_to_slice_num(12);
    uint slice2 = pwm_gpio_to_slice_num(14);

    pwm_set_enabled(slice1, true);
    pwm_set_enabled(slice2, true);

for (int i = 0; i < 5000; ++i)
{

    pwm_set_gpio_level(12, i);

    pwm_set_gpio_level(13, i);

    pwm_set_gpio_level(14, i);

    pwm_set_gpio_level(15, i);

    sleep_us(100);
}

return 0;
}*/