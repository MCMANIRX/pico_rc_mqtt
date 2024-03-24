#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
//#include <stdio.h>

#include "lib/BNO055_driver/bno055.h"
//#include "bno_controller.h"





typedef struct bno055_t bno055_t;
typedef struct bno055_accel_float_t bno055_accel_float_t;
typedef struct bno055_euler_float_t bno055_euler_float_t;


bno055_t bno;

float yaw;
volatile bool isData;

bool request_read;










void _poll_imu() {


    uint8_t calibrationStatus;
    bno055_get_sys_calib_stat(&calibrationStatus);
       // printf("Calibration Status: %u\n", calibrationStatus);

    bno055_accel_float_t accelData;
    bno055_convert_float_accel_xyz_msq(&accelData);
       // printf("x: %3.2f,   y: %3.2f,   z: %3.2f\n", accelData.x, accelData.y, accelData.z);

    bno055_euler_float_t eulerAngles;
    bno055_convert_float_euler_hpr_deg(&eulerAngles);
    yaw = eulerAngles.h-180.0;

    isData = true;

  //  printf("h: %3.2f,   p: %3.2f,   r: %3.2f\n\n", yaw, eulerAngles.p, eulerAngles.r);



   // return 0;

}


void init_imu() {
    request_read = false;

    const uint sda_pin = 4;
    const uint scl_pin = 5;

    gpio_init(sda_pin);
    gpio_init(scl_pin);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);



    
    i2c_init(i2c_default, 50 * 1000);

    int8_t res = bno055_pico_init(&bno, i2c_default, BNO055_I2C_ADDR1);

    if (res) {
        printf("BNO055 inilization failed!\n");
    }
    sleep_ms(100);

    res = bno055_set_power_mode(BNO055_POWER_MODE_NORMAL);
    if (res) {
        printf("BNO055 power mode set failed!\n");
    }
    sleep_ms(100);

    res = bno055_set_operation_mode(BNO055_OPERATION_MODE_COMPASS);
    if (res) {
        printf("BNO055 operation mode set failed!\n");
    }
    sleep_ms(100);
    yaw = 0.0;
    poll_imu();
    isData = true;


/*while(1) {
    poll_imu();
    busy_wait_us(33*100);

    }*/

    //return 0;
}



void poll_imu() {

        _poll_imu();


}