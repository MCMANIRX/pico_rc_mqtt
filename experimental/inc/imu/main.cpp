#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include <stdint.h>

extern float yaw;
extern volatile bool isData;
extern void poll_imu();

typedef unsigned short u16;
typedef uint32_t u32;

u16 steer_slice;
u16 drive_slice;

const float Kp = 5109.0;

#define SETTLE_THRESH 0.0005
#define RESET_THRESH 20


#define STEER_A 12
#define STEER_B 13
#define DRIVE_A 14
#define DRIVE_B 15



float ref_yaw;
float dist;


u32 clamp(float x, int limit) {

    u32 _x = round(x);

    return _x > limit ? limit : _x;
}


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



int main() {

    ref_yaw = 0;
    dist    = 0;

    stdio_init_all();


    gpio_set_function(DRIVE_A, GPIO_FUNC_PWM);
    gpio_set_function(DRIVE_B, GPIO_FUNC_PWM);

    gpio_set_function(STEER_A, GPIO_FUNC_PWM); 
    gpio_set_function(STEER_B, GPIO_FUNC_PWM);

    drive_slice = pwm_gpio_to_slice_num(14);
    steer_slice = pwm_gpio_to_slice_num(12);

    pwm_set_enabled(steer_slice, true);
    pwm_set_enabled(drive_slice, true);

    cyw43_arch_init();

    sleep_ms(2000);
    blink(2,500);


    multicore_launch_core1(poll_imu);



    while(!isData);
    isData = false;

    float last_yaw  = -360;

        while(1){
         printf("%f\t%f\t%f\n",yaw,last_yaw,dist);


        if(yaw != last_yaw && isData )
{
        dist = fabs(yaw-last_yaw);
        if(dist<SETTLE_THRESH)
            break;
        last_yaw = yaw;
        isData = false;

}
        }
    

    ref_yaw = yaw;
    u32 val = 0;





    printf("IMU Calibrated!\nreference yaw is %.2f\n",ref_yaw);
    blink(2,100);

    sleep_ms(2000);


    pwm_set_gpio_level(DRIVE_B,50*512);

    

    while(1) {

        dist = fabs(yaw-ref_yaw);
        val = clamp(clamp(dist,30)*Kp,65535);

       printf("%.2f\t%.2f\t%d\n",dist,yaw,val);
       sleep_ms(10);


        if(yaw < ref_yaw)
            pwm_set_gpio_level(ref_yaw < 0 ? STEER_A : STEER_B,val);
        else 
            pwm_set_gpio_level(ref_yaw > 0 ? STEER_A : STEER_B,val);


        
    }



    
}