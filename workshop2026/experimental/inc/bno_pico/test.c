#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include <stdint.h>
#include "bno_controller.h"

void aimu();

uint16_t drive_slice;
#define DRIVE_A 14
#define DRIVE_B 15


int main() {

    stdio_init_all();

    const uint sda_pin = 4;
    const uint scl_pin = 5;

    gpio_init(sda_pin);
    gpio_init(scl_pin);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    
    gpio_set_function(DRIVE_A, GPIO_FUNC_PWM);
    gpio_set_function(DRIVE_B, GPIO_FUNC_PWM);




    drive_slice = pwm_gpio_to_slice_num(14);

    pwm_set_enabled(drive_slice, true);

    sleep_ms(5000);

    init_imu();
 

    multicore_launch_core1(aimu);
   // busy_wait_us(1000*1000);
    //pwm_set_gpio_level(DRIVE_B,80*512);

    while(1) {
        sleep_ms(33);
        printf("weed");


    }


    return 0;

}

void aimu() {

    while(1){
        poll_imu();
        busy_wait_us(33*100);
    }
}





