/*
*   TODO:
*
*   1. P axis lock (use IMU to drive straight on an axis with PID loop)
*   2. Add functionality (script) to set local x y or z axis
*
*/


#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "hardware/pwm.h"
#include "hardware/vreg.h"
#include "mqtt_helper.h"
#include "pico/multicore.h"
#include <math.h>
#include "inc/imu/mpu6050_DMP_port.h"




u16_t drive_slice;
u16_t steer_slice;

bool script_active;
u8_t op_cnt;
QUEUE op_queue;
COMMAND cmd;

bool assign;
u8_t assign_data;
char id[4];
char ack[2];


u8_t imu_buf[IMU_BUF_LEN];
u8_t imu_buf_flag;

s8_t last_x,last_y;


bool dmpCalibrated;
bool prop_steer;
float ref_yaw;
float dist;
u32_t rot_amt;

extern float yaw;
extern volatile bool isData;
extern void poll_imu();
extern void init_imu();


u16_t half_float(float x) {

    return x < 1 ? (s16_t)(x * 32767) : -1;
}


u32_t clamp(float x, int limit) {

    u32_t _x = round(x);

    return _x > limit ? limit : _x;
}

s8_t clamp8(float x, int limit) {

    s32_t _x = round(x);

    return (s8_t)(abs(_x) > limit ? (_x < 0 ? -limit : limit) : _x);
}


s16_t arduino_map(s16_t x, s16_t in_min, s16_t in_max, s16_t out_min, s16_t out_max);


/* non-blocking wait with spinning wheel to make it work */

bool waiting;

int wait_cb(alarm_id_t id, void *user_data)
{

    waiting = false;
    return 0;
}

char wheel[] = {'-', '\\', '|', '/'};

void wait(int time)
{

    waiting = true;
    add_alarm_in_ms(time, wait_cb, NULL, false);
    printf("start wait %d\n", time);

    int x = 0;
    while (waiting)
    {
        putchar(wheel[x++]);
        putchar('\r');
        if (x == 3)
            x = 0;
    }
}

void move_motors(s8_t x, s8_t y)
{
    if(!dmpCalibrated)
        return;

    // make steering a little bit better
    if(!script_active)
        x = clamp8(x*STEER_MULT,127);

    last_x = x;
    last_y = y;

    if (y < 0)
    {
     //   pwm_set_gpio_level(DRIVE_B, abs(127 * SPEED_MULT)); 

     // 14 = DRIVE_A = INB1
     // 15 = DRIVE_B = INA1

     // 12 = STEER_A = INA2
     // 13 = STEER_B = INB2

        pwm_set_gpio_level(DRIVE_B, abs(y * SPEED_MULT)); //may need to redo with smaller function??
        pwm_set_gpio_level(DRIVE_A, 0);
    }
    else if (y > 0)
    {
      //  pwm_set_gpio_level(DRIVE_A, abs(127 * SPEED_MULT)); 
        pwm_set_gpio_level(DRIVE_A, abs(y * SPEED_MULT));
        pwm_set_gpio_level(DRIVE_B, 0);
    }

    else
    {
        pwm_set_gpio_level(DRIVE_B, 0);
        pwm_set_gpio_level(DRIVE_A, 0);
    }



    if (abs(x) > STEER_THRESH)

        {
            prop_steer = false;

            if (x < 0)
            {

            pwm_set_gpio_level(STEER_A, abs(x * SPEED_MULT));
            pwm_set_gpio_level(STEER_B, 0);
            }

            else if (x > 0)
            {

            pwm_set_gpio_level(STEER_A, 0);
            pwm_set_gpio_level(STEER_B,  abs(x * SPEED_MULT));

            }
        }
    else if(y){
        prop_steer = true;
        ref_yaw = yaw;
    }

    else
    {
        prop_steer = false;

    pwm_set_gpio_level(STEER_A, 0);
    pwm_set_gpio_level(STEER_B, 0);

     //   gpio_put(STEER_B, 0);
     //   gpio_put(STEER_A, 0);
    }
}

int failsafe_cb() {

    move_motors(0,0);   
    printf("will");

    return 0;


}

void send_rssi() {
    
    get_rssi();
    char payload[3];
    payload[0] = assign_data;
    payload[1] = RSSI_REQ;
    payload[2] = (s8_t)rssi;
    publish_with_strlen(RC_TOPIC,payload,3);

  //  publish_with_strlen(RC_TOPIC,(assign_data << 16) | (RSSI_REQ << 8) | ((s8_t)rssi &0xff),3);
   // printf("%x",(assign_data << 16) | (RSSI_REQ << 8) | ((s8_t)rssi &0xff));
}


void run_command(char *command)
{
    printf("cmd %x\n",command[1]);

  if(command[0] != YMOVE_OP && command != MOVE_OP)
    move_motors(0, 0);

    if (script_active)
        switch (command[1])
        {

        case MOVE_OP: // len 6
            move_motors(command[3], command[2]);
            wait((u32_t)((command[4] << 8) | command[5]));
            break;

        case YMOVE_OP: // len 5
            move_motors(0, command[2]);
            wait((u32_t)((command[3] << 8) | command[4]));
        //    move_motors(0, 0);
            break;


        case WAIT_OP: // len 4
            wait((u32_t)((command[2] << 8) | command[3]));
            break;

        case MSG_OP: // len 3
            ack[0] = assign_data;
            ack[1] = PRINT_OP;
            publish_with_strlen(RC_TOPIC, ack, 2);
        default:
            break;
        }
}

// run through script outside of interrupt
void run_script() {

    while (deque(&op_queue, &cmd))
        run_command(cmd.data);
                
        // printf("%x %x %x %x\n", cmd.data[0],cmd.data[1],cmd.data[2],cmd.data[3]);
        move_motors(0, 0);
}

bool _this;
bool all;
bool receiving = false;
alarm_id_t failsafe;


void message_decoder(char *topic, char *data)
{
    all = false;
    _this = false;

    if (data[0] == assign_data)
        _this = true;
    else if (data[0] == 0xff)
        all = true;

   // printf("%s %x %x %x %x \n",topic,data[0], data[1], data[2], data[3]);


    // enquire logic
    if (*(topic + 10) == 'n')
    {
        if (assign && strlen(id) <= 2)
        {
            //blink(1,1);

            cancel_alarm(failsafe);
            failsafe = add_alarm_in_ms(2500,failsafe_cb, NULL, false);


            ack[0] = assign_data;
            ack[1] = ACK;
            publish_with_strlen(RC_TOPIC, ack, 2); // QoS 0 to alleviate packet loss errors
        }
    }

    else if (data[1] & MATLAB_FLAG)
    {
        u8_t decoded_msg[8];
        decoded_msg[0] = data[0];
        decoded_msg[1] = data[2];
        for (int i = 2; i < 8; ++i)
            decoded_msg[i] = data[i+1] * (data[1] << (i - 1) & 0x80 ? -1 : 1);

        message_decoder(topic, decoded_msg);
        return;
    }

    // params logic
    if(data[1] == PARAMS_OP && imu_buf_flag-->0) {

        if(data[2] == YAW)
            publish_with_strlen_qos0(RC_TOPIC, imu_buf, IMU_BUF_LEN);

    }



    // assign logic
    else if (*(topic + 10) == 's')
    {
        if (_this || !assign)
        {
            assign = true;
            disconnect();
            sleep_ms(40);
            assign_data = data[2];
            imu_buf[0] = assign_data;
            sprintf(id, "%d", assign_data);
            set_id_for_reconnect(id);
            connect(&id);
        }
    }

    // action logic
    else if (*(topic + 11) == 't')
    {
        if (!script_active)
            if (_this || all){
                if (data[1] == MOVE_OP)
                    move_motors((s8_t)data[2], (s8_t)data[3]);
                else if (data[1] == RSSI_REQ)
                    send_rssi();
            }
    }
    
    // script logic
    else if (_this || all) {
        if(data[1] == SCRIPT_OP)
    {

        if (data[2] == SCRIPT_INCOMING)
        {
            op_cnt = data[3];
            init_queue(&op_queue);
            receiving = true;

        }
        else if (data[2] == SCRIPT_BEGIN)
        {
            if (op_queue.current_load)
            {
                script_active = true;
                run_script();
                
                move_motors(0, 0);

                ack[0] = assign_data;
                ack[1] = SCRIPT_END;
                publish_with_strlen(RC_TOPIC, ack, 2); // strlen is null-terminated so extra fcn in case the id is 0
                script_active = false;

            }
        }

        else if (data[2] == SCRIPT_END)
        {
            if (receiving)
            {
                ack[0] = assign_data;
                ack[1] = SCRIPT_RECEIVED;
                publish_with_strlen(RC_TOPIC, ack, 2); // strlen is null-terminated so extra fcn in case the id is 0
                receiving = false;
              //  puts("end!\n"); //debug
            }
        }

        else if (data[2] == SYNC_STATUS); // unimportant for rn

    } //end script ctrl

        if(receiving){
            memcpy(cmd.data,data,MESSAGE_SIZE);
            enque(&op_queue,&cmd);
            memset(cmd.data,0,MESSAGE_SIZE);
        }
    
    } // end message rcv

}


static void hw_loop();
static void mqtt_loop();

int main()
{

    /* script init */
    op_cnt = 0;
    script_active = false;

    /* steer init */
    dmpCalibrated = false;
    prop_steer = false;
    ref_yaw = 0;
    last_x = 0;
    last_y = 0;

    /* PWM init */
    gpio_set_function(DRIVE_A, GPIO_FUNC_PWM);
    gpio_set_function(DRIVE_B, GPIO_FUNC_PWM);

    gpio_set_function(STEER_A, GPIO_FUNC_PWM); 
    gpio_set_function(STEER_B, GPIO_FUNC_PWM);

    drive_slice = pwm_gpio_to_slice_num(14);
    steer_slice = pwm_gpio_to_slice_num(12);

    pwm_set_enabled(drive_slice, true);
    pwm_set_enabled(steer_slice, true);
    
    // #################################################

    stdio_init_all();

    /* overclock (optional) */
   // vreg_set_voltage(VREG_VOLTAGE_1_30);
   // set_sys_clock_khz(250000,true);


    //connect to wifi and MQTT broker

    mqtt_init("rc_unassigned");

    //set local message decoder callback
    set_message_decoder(&message_decoder);

    // init IMU data transfer logic
    imu_buf_flag = 0;
    imu_buf[1] = PARAMS_OP;

  //  dmpCalibrated = true;

    // run hw polling on core1
    multicore_launch_core1(hw_loop);

    // continue main mqtt logic on core0
    mqtt_loop();

    return 0;


}

static void mqtt_loop()
{
    int x = 0;

    while (1)
    {
        if (!assign)
        {
            wait(500);
            publish(RC_TOPIC, ASSIGN_REQ);
            wait(500);
        }
        // loop logic to keep from getting too tight
        else
            x = 0;
          //          pwm_set_gpio_level(DRIVE_B, 65535);

    }

}


// I2C reading for async IMU data buffer updates

static void hw_loop()
{

    /* calibrate IMU */
    init_imu();
    while(!isData) {
        poll_imu();
        ref_yaw = 0;
    }
    isData = false;

    float last_yaw  = -360;

    while (1)
    {
        poll_imu();
        printf("%f\t%f\t%f\n", yaw, last_yaw, dist);

        if (yaw != last_yaw && isData)
        {
            dist = fabs(fabs(yaw) - fabs(last_yaw));
            if (dist < SETTLE_THRESH)
                break;
            last_yaw = yaw;
            isData = false;
        }
    }
    ///////////////////////////////////////////

    ref_yaw = yaw;
    rot_amt = 0;

    printf("IMU Calibrated!\nreference yaw is %.2f\n",ref_yaw);
    blink(2,100);
    dmpCalibrated = true;
    imu_buf[2] = IMU_INIT;

    while(!assign);
    publish_with_strlen_qos0(RC_TOPIC,imu_buf,3);

    imu_buf[2] = YAW;



    u16_t temp = 0;
    float abs_y,abs_ry;

    

    while(1) {

        poll_imu();
        imu_buf_flag = 1;

        

  //     printf("%.2f\t%.2f\t%d\n",dist,yaw,val);
     //  sleep_ms(10);


             for (int i = 0; i < IMU_PARAM_COUNT; i+=2)
        {
            //convert normalized floats to half float in u16 format
            temp = half_float(yaw/EULER_NORM);
            imu_buf[i + 3] = (temp >> 8) & 0xff;
            imu_buf[i + 4] = temp & 0xff;
        }

        if (prop_steer)
        {

            abs_y = fabs(yaw);
            abs_ry = fabs(ref_yaw);

            dist = fabs((abs_y > 90 || abs_ry > 90) ? ((abs_ry > 90 && abs_y > 90) ? ((yaw * ref_yaw > 0) ? abs_y - 180 - (abs_ry - 180) : abs_y - 180 + abs_ry - 180) : ((yaw * ref_yaw > 0) ? abs_y - abs_ry : abs_ry + abs_y)) : yaw - ref_yaw);

            /*        if ((abs_y > 90 || abs_ry > 90)) {
               if (abs_ry > 90 && abs_y > 90) {
                   if (yaw * ref_yaw > 0) {
                       dist = abs_y - 180 - (abs_ry - 180);
                       printf("Case 1: yaw * ref_yaw > 0\n");
                   } else {
                       dist = abs_y - 180 + abs_ry - 180;
                       printf("Case 2: yaw * ref_yaw <= 0\n");
                   }
               } else {
                   if (yaw * ref_yaw > 0) {
                       dist = abs_y - abs_ry;
                       printf("Case 3: yaw * ref_yaw > 0\n");
                   } else {
                       dist = abs_ry + abs_y;
                       printf("Case 4: yaw * ref_yaw <= 0\n");
                   }
               }

           } else {
               dist = yaw - ref_yaw;
               printf("Case 5: fabs((abs_y > 90 || abs_ry > 90)) is false\n");
           }*/


            rot_amt = clamp(clamp(dist, 30) * Kp, 65535);

            pwm_set_gpio_level(yaw < ref_yaw ? (last_y < 0 ? STEER_B : STEER_A) : (last_y < 0 ? STEER_A : STEER_B), rot_amt);

            /*
                        if (ref_yaw < 0) {
                            if (yaw < ref_yaw) {
                            pwm_set_gpio_level(STEER_B, rot_amt);

                            }
                        else {
                            pwm_set_gpio_level(STEER_A, rot_amt);

                        }
                        } else  {
                            if (yaw < ref_yaw) {
                                                pwm_set_gpio_level(STEER_B, rot_amt);

                            }
                            else
                                                pwm_set_gpio_level(STEER_A, rot_amt);
                        }
            */

            //   printf("%.2f\t%.2f\t%.2f\n",dist,ref_yaw,yaw);
        }
    }
}

  /*  for (;;)
    {
        //example data
        float x[] = {0.1234567};
        u16_t temp = 0;

        // put i2c code here to get the 3 vectors with correct calibration and then normalize each vector component

        for (int i = 0; i < IMU_PARAM_COUNT; i+=2)
        {
            //convert normalized floats to half float in u16 format
            temp = x[i] < 1 ? (u16_t)((((float)x[i])) * 65535) : -1;
            imu_buf[i + 3] = (temp >> 8) & 0xff;
            imu_buf[i + 4] = temp & 0xff;
        }

        imu_buf_flag = 1;
        sleep_ms(33); // ~30 updates/sec
    }
}*/

s16_t arduino_map(s16_t x, s16_t in_min, s16_t in_max, s16_t out_min, s16_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

