// ChatGPT generated header

#ifndef IMU_H
#define IMU_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#ifndef PICO_DEFAULT_LED_PIN // PICO w with WiFi
#include "pico/cyw43_arch.h"
#endif


//insane black magic
#ifdef __cplusplus
extern "C" {
#endif

void init_imu();
void poll_imu();
void reset_mpu();

#ifdef __cplusplus
}
#endif

// Variable declarations
//extern MPU6050 mpu; // MPU6050 object for IMU
extern bool dmpReady; // Flag to indicate if DMP initialization was successful
/*extern uint8_t mpuIntStatus; // Holds actual interrupt status byte from MPU
extern uint8_t devStatus; // Return status after each device operation (0 = success, !0 = error)
extern uint16_t packetSize; // Expected DMP packet size (default is 42 bytes)
extern uint16_t fifoCount; // Count of all bytes currently in FIFO
extern uint8_t fifoBuffer[64]; // FIFO storage buffer
extern Quaternion q; // Quaternion container [w, x, y, z]
extern VectorInt16 aa; // Accelerometer sensor measurements [x, y, z]
extern VectorInt16 gy; // Gyroscope sensor measurements [x, y, z]
extern VectorInt16 aaReal; // Gravity-free accelerometer sensor measurements [x, y, z]
extern VectorInt16 aaWorld; // World-frame accelerometer sensor measurements [x, y, z]
extern VectorFloat gravity; // Gravity vector [x, y, z]
*/
extern float euler[3]; // Euler angle container [psi, theta, phi]
extern float ypr[3]; // Yaw/pitch/roll container and gravity vector [yaw, pitch, roll]
extern volatile float yaw, pitch, roll; // Yaw, pitch, roll angles
extern volatile bool mpuInterrupt; // Indicates whether MPU interrupt pin has gone high
extern volatile bool isData; // Indicates whether MPU interrupt pin has gone high


// Function prototypes
void initLED();
void waitForUsbConnect();
// void poll_imu();
void dmpDataReady();

#endif // IMU_H