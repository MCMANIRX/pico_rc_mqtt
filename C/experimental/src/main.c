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
#include "hardware/watchdog.h"

#include "mqtt_helper.h"
#include "pico/multicore.h"
#include <math.h>

#include "inc/imu/mpu6050_DMP_port.h"
#include "inc/ultra/hcsr04.h"
#include "inc/ultra/util/smooth_average.h"
#include "inc/ultra/util/nbt.h"

    non_blocking_timer timer0;
    non_blocking_timer timer1;

    smooth_average smooth; //= malloc(sizeof(smooth_average));

volatile bool _run_script;
volatile bool core_1_running;
volatile bool core_1_watchdog;

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


bool obj_flag;

u8_t ult_buf[ULT_BUF_LEN];
u8_t ult_buf_flag;

u8_t imu_buf[IMU_BUF_LEN];
u8_t imu_buf_flag;


s8_t last_x,last_y;

float ult_dist = 0;


volatile bool dmpCalibrated;
volatile bool prop_steer;
float ref_yaw;
float dist;
u32_t rot_amt;

extern float yaw;
extern volatile bool isData;
extern void poll_imu();
extern void init_imu();

alarm_id_t failsafe;
alarm_id_t waiter;



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

    if(waiting)
        cancel_alarm(waiter);

    waiting = true;
    waiter = add_alarm_in_ms(time, wait_cb, NULL, false);
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
     //   pwm_set_gpio_level(DRIVE_F, abs(127 * SPEED_MULT)); 

     // 14 = DRIVE_B = INB1
     // 15 = DRIVE_F = INA1

     // 12 = STEER_L = INA2
     // 13 = STEER_R = INB2

        pwm_set_gpio_level(DRIVE_F, abs(y * SPEED_MULT)); //may need to redo with smaller function??
        pwm_set_gpio_level(DRIVE_B, 0);
    }
    else if (y > 0)
    {
      //  pwm_set_gpio_level(DRIVE_B, abs(127 * SPEED_MULT)); 
        pwm_set_gpio_level(DRIVE_B, abs(y * SPEED_MULT));
        pwm_set_gpio_level(DRIVE_F, 0);
    }

    else if (abs(y) < 10)
    {
       // printf("y\t%d\n",abs(y));
        pwm_set_gpio_level(DRIVE_F, 0);
        pwm_set_gpio_level(DRIVE_B, 0);
        prop_steer = false;

    }

    if(y<0&&obj_flag)return; // camera steer


    if (abs(x) > STEER_THRESH)

        {
            prop_steer = false;

            if (x < 0)
            {

            pwm_set_gpio_level(STEER_L, abs(x * SPEED_MULT));
            pwm_set_gpio_level(STEER_R, 0);
            }

            else if (x > 0)
            {

            pwm_set_gpio_level(STEER_L, 0);
            pwm_set_gpio_level(STEER_R,  abs(x * SPEED_MULT));

            }
        }
    else if(abs(y)>10){
        prop_steer = true;
        ref_yaw = yaw;
    }

    else
    {

    pwm_set_gpio_level(STEER_L, 0);
    pwm_set_gpio_level(STEER_R, 0);

     //   gpio_put(STEER_R, 0);
     //   gpio_put(STEER_L, 0);
    }
}

int failsafe_cb() {
            if(script_active)return;

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


void message_decoder(char *topic, char *data)
{
    watchdog_update();

    all = false;
    _this = false;

    if (data[0] == assign_data)
        _this = true;
    else if (data[0] == 0xff)
        all = true;

    //printf("%s %x %x %x %x %x %x \n",topic,data[0], data[1], data[2], data[3], data[4], data[5]);


    // enquire logic
    if (*(topic + 10) == 'n')
    {
        if (assign && strlen(id) <= 2)
        {
            //blink(1,1);

            cancel_alarm(failsafe);
            if(!script_active)
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
    if(data[1] == PARAMS_OP) {

        if(data[2] == YAW && imu_buf_flag-->0){
            publish_with_strlen_qos0(RC_TOPIC, imu_buf, IMU_BUF_LEN);
            if(ult_buf_flag-->0)
                publish_with_strlen_qos0(RC_TOPIC, ult_buf, ULT_BUF_LEN);

        }
    }
    else if(data[1] == CAM_OP) {
        if(last_y!=0 && last_y<0){
        u16_t _rot_amt = clamp(abs(data[2]) * 13000,65535);
        pwm_set_gpio_level(((s8_t)data[2])<0?STEER_L:STEER_R, _rot_amt);
        pwm_set_gpio_level(((s8_t)data[2])<0?STEER_R:STEER_L, 0);

        printf("rot!%d\tamt %d\n",_rot_amt,(s8_t)data[2]);
        }
        else {
                pwm_set_gpio_level(STEER_L, 0);
                pwm_set_gpio_level(STEER_R, 0);

        }

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
            ult_buf[0] = assign_data;
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

    if(_run_script)return;
    
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
                _run_script = true;


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

    core_1_running = false;
    core_1_watchdog = false;

    /* PWM init */
    gpio_set_function(DRIVE_B, GPIO_FUNC_PWM);
    gpio_set_function(DRIVE_F, GPIO_FUNC_PWM);

    gpio_set_function(STEER_L, GPIO_FUNC_PWM); 
    gpio_set_function(STEER_R, GPIO_FUNC_PWM);

    drive_slice = pwm_gpio_to_slice_num(14);
    steer_slice = pwm_gpio_to_slice_num(12);

    pwm_set_enabled(drive_slice, true);
    pwm_set_enabled(steer_slice, true);

    pwm_set_gpio_level(STEER_L,0);
    pwm_set_gpio_level(STEER_R,0);
    pwm_set_gpio_level(DRIVE_F,0);
    pwm_set_gpio_level(DRIVE_B,0);


    /* Ultrasonic LED init */
    gpio_init(ULT_LED_PIN);
    gpio_set_dir(ULT_LED_PIN,true);
    
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
    ult_buf_flag = 0;

    imu_buf[1] = PARAMS_OP;
    ult_buf[1] = PARAMS_OP;

  //  dmpCalibrated = true;

    // run hw polling on core1
    multicore_launch_core1(hw_loop);
    start_timer(&timer1,30000);

    // continue main mqtt logic on core0
    mqtt_loop();

    return 0;


}

static void mqtt_loop()
{
    watchdog_enable(3000,true);
    watchdog_start_tick(12);
    while (1)
    {
    //    watchdog_update();
        if (!assign)
        {   watchdog_update();
            wait(500);
            publish(RC_TOPIC, ASSIGN_REQ);
            watchdog_update();
            wait(500);
        }
        // loop logic to keep from getting too tight

        if(_run_script){
                gpio_put(ULT_LED_PIN,1);

                run_script();
                ack[0] = assign_data;
                ack[1] = SCRIPT_END;
                publish_with_strlen(RC_TOPIC, ack, 2); // strlen is null-terminated so extra fcn in case the id is 0
                script_active = false;
                
                pwm_set_gpio_level(STEER_L,0);
                pwm_set_gpio_level(STEER_R,0);
                pwm_set_gpio_level(DRIVE_F,0);
                pwm_set_gpio_level(DRIVE_B,0);

                _run_script = false;
                gpio_put(ULT_LED_PIN,0);

          //          pwm_set_gpio_level(DRIVE_F, 65535);
    }

    if(timer_expired(&timer1))
        if(core_1_running){
            if(!core_1_watchdog){
                printf("reset core 1!\n");
                core_1_running = false;
                core_1_watchdog = false;
                multicore_reset_core1();
                multicore_launch_core1(hw_loop);
            } else start_timer(&timer1,1000);
            core_1_watchdog = false;

    }


}
}

// I2C reading for async IMU data buffer updates

static void hw_loop()
{




    hcsr04_init(TRIGGER_GPIO, ECHO_GPIO);
    init_smooth(&smooth,4);
    obj_flag = false;
    float _ult_dist = 0;
    ult_dist = 0;


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
    core_1_watchdog = true;
    core_1_running = true;
    while(!assign);

    imu_buf[2] = IMU_INIT;

    publish_with_strlen_qos0(RC_TOPIC,imu_buf,3);
    ult_buf[2] = ULT_DIST;
    imu_buf[2] = YAW;



    u16_t temp = 0;
    float abs_y,abs_ry;

    start_timer(&timer0,20);
    start_timer(&timer1,1000);

    while(1) {
        watchdog_update();
        core_1_watchdog = true;

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

            abs_y = fabs(yaw+180);
            abs_ry = fabs(ref_yaw+180);




            dist = fabs(abs_y<180+abs_ry && abs_ry < 180+abs_y ? abs_y-abs_ry : abs_y > abs_ry ? 360-abs_y+abs_ry : 360-abs_ry+abs_y );

            rot_amt = clamp(clamp(dist, 30) * Kp, 65535);

            int pin = (dist < 180) ? (yaw > ref_yaw ? STEER_L : STEER_R) :  (last_y < 0 ? (yaw > ref_yaw ? STEER_R : STEER_L) :  (last_y > 0 ? STEER_L : STEER_R));

            pwm_set_gpio_level(pin,rot_amt);

          //  dist = fabs(abs_y<180+abs_ry && abs_ry < 180+abs_y ? abs_y-abs_ry : abs_y > abs_ry ? 360-abs_y+abs_ry : 360-abs_ry+abs_y );

           // pwm_set_gpio_level(yaw>ref_yaw ? (ref_yaw > 180 + yaw ? STEER_R : STEER_L) : (ref_yaw + 180 < yaw ? STEER_L : STEER_R),rot_amt);

           // dist = fabs((abs_y > 90 || abs_ry > 90) ? ((abs_ry > 90 && abs_y > 90) ? ((yaw * ref_yaw > 0) ? abs_y - 180 - (abs_ry - 180) : abs_y - 180 + abs_ry - 180) : ((yaw * ref_yaw > 0) ? abs_y - abs_ry : abs_ry + abs_y)) : yaw - ref_yaw);

           // rot_amt = clamp(clamp(dist, 30) * Kp, 65535);

            //pwm_set_gpio_level(yaw < ref_yaw ? (last_y < 0 ? STEER_R : STEER_L) : (last_y < 0 ? STEER_L : STEER_R), rot_amt);

            printf("%c\t%d\t%.2f\t%.2f\t%.2f\n",pin == STEER_L ? "l":"r",rot_amt,dist,ref_yaw,yaw);
        }




        /*


        
        working on doing proportional car steering with an IMU. my imu has a yaw range of 0 to 360 degrees and im trying to write code such that the wheels turn left or right a specified distance, rot_amt, toward the reference yaw. the yaw is compared toward the reference yaw to determine which pin to set to rot_amt to turn the motor toward the axis. this is my current code, where abs_y is the absolute value of the yaw, and abs_ry is the absolute value of the reference yaw. if last_y is greater than 0 that means the car is moving backwards, so reverse steer pins

            pwm_set_gpio_level(yaw>ref_yaw ? (ref_yaw > 180 + yaw ? STEER_R : STEER_L) : (ref_yaw + 180 < yaw ? STEER_L : STEER_R),rot_amt);*/


        if(timer_expired(&timer0)){


            float _ult_dist = hcsr04_get_distance_cm(TRIGGER_GPIO, ECHO_GPIO);
    

            ult_dist = filter_median_kalman(&smooth,_ult_dist,RANGE);
           // ult_dist = filter_average_kalman(&smooth,_ult_dist,RANGE_AND_ROC);
           // printf("Distance %.2f\n",ult_dist);
            temp = half_float(ult_dist/VALUE_THRESH);
            ult_buf[3] = (temp>>8)&0xff;
            ult_buf[4] =  temp&0xff;
            
            ult_buf_flag = 1;
            start_timer(&timer0,20);
        }


        if(ult_dist < 80) {
         //       gpio_put(ULT_LED_PIN,1);
                ult_buf[5] = 1;
                obj_flag = true;
        }

        else {
          //  gpio_put(ULT_LED_PIN,0);
            ult_buf[5] = 0;
            obj_flag = false;


        }


    }
}


s16_t arduino_map(s16_t x, s16_t in_min, s16_t in_max, s16_t out_min, s16_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

