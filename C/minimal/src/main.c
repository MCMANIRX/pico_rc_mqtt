// Mateo Smith, 2024

#include <math.h>

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/apps/mqtt.h"

#include "hardware/pwm.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"

#include "mqtt_helper.h"

#include "inc/ultra/hcsr04.h"
#include "inc/ultra/util/smooth_filter.h"
#include "inc/ultra/util/nbt.h"
#include "inc/adc/battery_monitor.h"
#include "inc/esp_uart/esp_uart.h"

#define IMU MPU
//#define ULT_CONNECT 
//#define ESP_CONNECT 
#define BAT_CONNECT 



#if     IMU == MPU
    #include "inc/imu/mpu6050_DMP_port.h"
#elif   IMU == BNO
    #include "inc/bno_pico/bno_controller.h"
#endif

/*#define ALT_DRIVE_PINS false

#if ALT_DRIVE_PINS ==  true
    #undef DRIVE_B
    #undef DRIVE_F
    #undef STEER_L
    #undef STEER_R

    #define DRIVE_B 16
    #define DRIVE_F 17
    #define STEER_L 18
    #define STEER_R 19
#endif*/


    

non_blocking_timer ultra_timer;
non_blocking_timer core_1_wd_timer;
non_blocking_timer stop_sign_timer;
non_blocking_timer wait_timer;
non_blocking_timer bat_esp_timer;


smooth_filter smooth; //= malloc(sizeof(smooth_filter));

volatile bool _run_script;
volatile bool core_1_running;
volatile bool core_1_watchdog;

bool stop_sign;
bool saw_stop_sign;
bool stop_sign_timer_began;

bool hard_stop;
bool hard_stopped;

u16_t drive_slice;
u16_t steer_slice;

bool script_active;
u8_t op_cnt;
QUEUE op_queue;
COMMAND cmd;

bool assign;
u8_t assign_data;
char id[4];
char gpb[3];


bool obj_flag;

u8_t ult_buf[ULT_BUF_LEN];
u8_t ult_buf_flag;

u8_t bat_buf[BAT_BUF_LEN];
u8_t bat_buf_flag;

u8_t imu_buf[IMU_BUF_LEN];
u8_t imu_buf_flag;

u8_t esp_buf[ESP_BUF_LEN];
esp_buf_flag = 1;


float imu_offset;




s8_t last_x,last_y;

float ult_dist = 0;



volatile bool imu_calibrated;
volatile bool prop_steer;
volatile float ref_yaw;
volatile float effective_yaw;
float dist;
u32_t rot_amt;

extern volatile float yaw;
extern volatile bool isData;
extern void poll_imu();
extern void init_imu();
extern void reset_mpu();
extern void init_imu();

alarm_id_t failsafe;
alarm_id_t waiter;

void stop_sign_routine();
void set_imu_offset();
uint32_t swap_endian(uint32_t value);

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

float calc_average(uint8_t trigger_gpio, uint8_t echo_gpio)
{
  static float average = -1.0 ;
  if (average == -1.0)  // special case for first reading.
  {
    average = hcsr04_get_distance_cm( trigger_gpio,  echo_gpio);
    return average ;
  }
  average += 0.1 * (hcsr04_get_distance_cm( trigger_gpio,  echo_gpio) - average) ;  // 0.1 for approx 10 sample averate, 0.01 for 100 sample etc.
  return average ;
}


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
    static bool ss_script = false;

    int64_t time_left = abs(wait_timer.target_time_us - wait_timer.duration_ms);
    

    start_timer(&wait_timer,time);
    waiting = true;
    //waiter = add_alarm_in_ms(time, wait_cb, NULL, false);
    printf("start wait %d\n", time);

    int x = 0;
    while (waiting)
    {   
        
    watchdog_update();
    if(timer_expired(&wait_timer))
        break;

        putchar(wheel[x++]);
        putchar('\r');
        if (x == 3)
            x = 0;
    }
    waiting = false;

    if(time != 40 && stop_sign&script_active && (!saw_stop_sign) && (!ss_script)) {
        printf("stop sign! time left: %d\n", time_left);
        ss_script = true;
        wait(time_left);
        ss_script = false;
    }
    else ss_script = false;

}

void move_motors(s8_t x, s8_t y)
{
   /* if(!imu_calibrated)
        return;*/

    //printf("y x %d %d\t",y,x);

    // make steering a little bit better
    if(!script_active)
        x = clamp8(x*STEER_MULT,127);

    if(saw_stop_sign && y < 0 && !stop_sign_timer_began){
        start_timer(&stop_sign_timer,4000); 
        stop_sign_timer_began = true;
    }

    last_x = x;
    last_y = y;

    #if IMU != MPU || IMU != BNO
        hard_stopped = false;
    #endif


    //printf("y\t%d\thardstopped\t%s\n",abs(y),hard_stopped?"true":"false");


    if (y < 0 && !hard_stopped)
    {
     //   pwm_set_gpio_level(DRIVE_F, abs(127 * SPEED_MULT)); 

     // 14 = DRIVE_B = INB1
     // 15 = DRIVE_F = INA1

     // 12 = STEER_L = INA2
     // 13 = STEER_R = INB2
        gpio_put(ULT_LED_PIN,1);


        pwm_set_gpio_level(DRIVE_F, abs(y * SPEED_MULT)); //may need to redo with smaller function??
        pwm_set_gpio_level(DRIVE_B, 0);
        printf("ddy\t%d\n",abs(y));

    }
    else if (y > 0)
    {
        gpio_put(ULT_LED_PIN,0);

      //  pwm_set_gpio_level(DRIVE_B, abs(127 * SPEED_MULT)); 
        pwm_set_gpio_level(DRIVE_B, abs(y * SPEED_MULT));
        pwm_set_gpio_level(DRIVE_F, 0);
             //   printf("y\t%d\n",abs(y));

    }

    else if (abs(y) < DRIVE_THRESH)
    {
        gpio_put(ULT_LED_PIN,0);

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
    else if(abs(y)>DRIVE_THRESH && imu_calibrated && !prop_steer){
        prop_steer = true;
        ref_yaw = yaw;
    }

    else
    {

    pwm_set_gpio_level(STEER_L, 0);
    pwm_set_gpio_level(STEER_R, 0);

    }
}


void _hard_stop(int time) {
    gpio_put(ULT_LED_PIN,1);
    pwm_set_gpio_level(DRIVE_B,0);
    pwm_set_gpio_level(DRIVE_F,0);
    pwm_set_gpio_level(DRIVE_B,65535);
    wait(300);
    pwm_set_gpio_level(DRIVE_B,0);
    wait(time);
    pwm_set_gpio_level(DRIVE_B,0);
    gpio_put(ULT_LED_PIN,0);

}

void stop_sign_routine(){
            script_active = true;
            stop_sign_timer_began = false;
            _hard_stop(2000);
            stop_sign = false;
            saw_stop_sign = true;
            script_active = false;


}

void hard_stop_routine(){
            if(hard_stopped)return;
            script_active = true;
            _hard_stop(1000);
            hard_stopped = true;
            hard_stop = false;
            script_active = false;

}

int failsafe_cb() {
    printf("failsafe!");

    if(script_active)return;
    _hard_stop(1000);
    move_motors(0,0);   



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
            prop_steer = true;
            wait((u32_t)((command[3] << 8) | command[4]));
            prop_steer = false;
        //    move_motors(0, 0);
            break;


        case WAIT_OP: // len 4
            wait((u32_t)((command[2] << 8) | command[3]));
            break;

        case MSG_OP: // len 3
            gpb[0] = assign_data;
            gpb[1] = PRINT_OP;
            publish_with_strlen(RC_TOPIC, gpb, 2);
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


            gpb[0] = assign_data;
            gpb[1] = ACK;
            publish_with_strlen(RC_TOPIC, gpb, 2); // QoS 0 to alleviate packet loss errors
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

        if(data[2] == YAW){
            //imu_buf[3] = 0xaa;

            #if IMU == MPU || IMU == BNO
                if(imu_buf_flag--==1)
                    publish_with_strlen_qos0(RC_TOPIC, imu_buf, IMU_BUF_LEN);
            #endif


            #ifdef ULT_CONNECT
                if(ult_buf_flag--==1)
                    publish_with_strlen_qos0(RC_TOPIC, ult_buf, ULT_BUF_LEN);
            #endif


            #ifdef BAT_CONNECT
            if(bat_buf_flag--==1)
                publish_with_strlen_qos0(RC_TOPIC, bat_buf,BAT_BUF_LEN);
            #endif

            
            #ifdef ESP_CONNECT
            if(esp_buf_flag--==1)
                publish_with_strlen_qos0(RC_TOPIC, esp_buf,ESP_BUF_LEN);
            #endif
        }
        else if (data[2] == IMU_REHOME){
            if(imu_calibrated) {
                set_imu_offset();
            }
          //  printf("%.2f\t",yaw);
         //   printf("%.2f\t%.2f\n",imu_offset,yaw);
        }
         else if (data[2] == IMU_REF_SET) {
            float nry = (float)data[3];
            blink(2,50);
            if(imu_calibrated){
                ref_yaw = ref_yaw > 0 ? (ref_yaw + nry > 180 ? -360+(nry+ref_yaw): ref_yaw + nry ) : (ref_yaw + nry < -180 ? 360+(nry+ref_yaw) : ref_yaw + nry);
                if(abs(last_y) > DRIVE_THRESH)
                    prop_steer = true;
                printf("%.2f ref_yaw %d prop_steer %d imu_calib\n",ref_yaw,prop_steer, imu_calibrated);
        }        }
        

    }
    else if(_this && data[1] == CAM_OP) {
        if(data[2] == CAM_EVADE){
            if(last_y!=0 && last_y<0){
            u16_t _rot_amt = clamp(abs(data[3]) * 13000,65535);
            pwm_set_gpio_level(((s8_t)data[3])<0?STEER_L:STEER_R, _rot_amt);
            pwm_set_gpio_level(((s8_t)data[3])<0?STEER_R:STEER_L, 0);
            //blink(10,100);

            printf("rot!%d\tamt %d\n",_rot_amt,(s8_t)data[3]);
            }
            else {
                    pwm_set_gpio_level(STEER_L, 0);
                    pwm_set_gpio_level(STEER_R, 0);

            }
        }
        else if(data[2] == CAM_STOP_SIGN && !stop_sign_timer_began && !saw_stop_sign) 
            stop_sign = true;
        

        
        else if(data[2] == CAM_HARD_STOP)
            hard_stop = true;
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
            bat_buf[0] = assign_data;

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
                gpb[0] = assign_data;
                gpb[1] = SCRIPT_OP;
                gpb[2] = SCRIPT_RECEIVED;
                publish_with_strlen_qos0(RC_TOPIC, gpb, 3); // strlen is null-terminated so extra fcn in case the id is 0
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
    watchdog_enable(30*1000,true);

    // init IMU data transfer logic
    imu_buf_flag = 0;
    ult_buf_flag = 0;
    bat_buf_flag = 0;
    esp_buf_flag = 0;

    /* script init */
    op_cnt = 0;
    script_active = false;

    /* steer init */
    imu_calibrated = false;
    prop_steer = false;
    ref_yaw = 0;
    effective_yaw  =0;
    last_x = 0;
    last_y = 0;

    core_1_running  = false;
    core_1_watchdog = false;
    stop_sign       = false;
    saw_stop_sign   = false;
    stop_sign_timer_began = false;
    hard_stop       = false;
    hard_stopped    = false;

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
    init_esp_uart(UART_TX,UART_RX);

    /* overclock (optional) */
   // vreg_set_voltage(VREG_VOLTAGE_1_30);
   // set_sys_clock_khz(250000,true);


    //connect to wifi and MQTT broker

    mqtt_init("rc_unassigned");



    //set local message decoder callback
    set_message_decoder(&message_decoder);



    imu_buf[1] = PARAMS_OP;
    ult_buf[1] = PARAMS_OP;
    bat_buf[1] = PARAMS_OP;
    esp_buf[1] = PARAMS_OP;

  //  imu_calibrated = true;

    // run hw polling on core1
    multicore_launch_core1(hw_loop);

    // continue main mqtt logic on core0
    mqtt_loop();

    return 0;


}
int imu_failures;
bool skip_imu;
static void mqtt_loop()
{   
    imu_failures = 0;
    skip_imu = false;

    #if IMU != MPU && IMU != BNO
        skip_imu = true;
    #endif

    //start_timer(&core_1_wd_timer, 5000);
    watchdog_enable(3500,true);
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

           /* if(watchdog_get_count()<500){
                reset_mpu();

                i2c_deinit(i2c0);
                skip_imu = true;
            }*/

        if(_run_script){
                gpio_put(ULT_LED_PIN,1);

                run_script();
                gpb[0] = assign_data;
                gpb[1] = SCRIPT_OP;
                gpb[2] = SCRIPT_END;
                publish_with_strlen(RC_TOPIC, gpb, 3); // strlen is null-terminated so extra fcn in case the id is 0
                script_active = false;
                pwm_set_gpio_level(STEER_L,0);
                pwm_set_gpio_level(STEER_R,0);
                pwm_set_gpio_level(DRIVE_F,0);
                pwm_set_gpio_level(DRIVE_B,0);

                _run_script = false;
                gpio_put(ULT_LED_PIN,0);

          //          pwm_set_gpio_level(DRIVE_F, 65535);
    }

    if(stop_sign && !saw_stop_sign)
        stop_sign_routine();
    
    if(hard_stop && !hard_stopped)
        hard_stop_routine();
    
    if (timer_expired(&core_1_wd_timer)){
            putchar(core_1_watchdog ? 'c' : 'f');
            if (!core_1_watchdog)
            {
                printf("reset core 1!\n");
                pwm_set_gpio_level(DRIVE_B, 0);
                pwm_set_gpio_level(DRIVE_F, 0);
                imu_calibrated = false;
                gpio_put(ULT_LED_PIN, 1);
                imu_failures++;
                if(imu_failures>3)
                    skip_imu = true;
                multicore_reset_core1();
                multicore_launch_core1(hw_loop);
            }

            start_timer(&core_1_wd_timer, 1000);
            core_1_watchdog = false;
        }

    if(timer_expired(&stop_sign_timer) && stop_sign_timer_began){
        saw_stop_sign = false;
        stop_sign_timer_began = false;
    }
}
}

// I2C reading for async IMU data buffer updates

static void hw_loop()
{   
    core_1_watchdog = true;

    printf("init ultrasonic...");
    hcsr04_init(TRIGGER_GPIO, ECHO_GPIO);
    init_smooth(&smooth,16);
    printf("done.\n");
    obj_flag = false;
    float _ult_dist = 0;
    ult_dist = 0;
    printf("init battery monitor...");

    battery_monitor_init();
    printf("done.\n");
    if(!skip_imu){
    printf("init IMU...");

    /* calibrate IMU */
    if(watchdog_caused_reboot())
        i2c_deinit(i2c0);
    init_imu();
    printf("done.\n");
    printf("Calibrating IMU...");

    while(!isData) {
        core_1_watchdog = true;

        poll_imu();
        ref_yaw = 0;
    }
    isData = false;


    float last_yaw  = -360;
    imu_offset = 0;


    while (1)
    {   



        poll_imu();
        printf("%f\t%f\t%f\n", yaw, last_yaw, dist);

        if (yaw != last_yaw /*&& isData*/)
        {
            core_1_watchdog = true;

            dist = fabs(fabs(yaw) - fabs(last_yaw));
            if (dist < SETTLE_THRESH)
                break;
            last_yaw = yaw;
            isData = false;
        }
    }
    ///////////////////////////////////////////
    set_imu_offset(); 
    effective_yaw = yaw+imu_offset;
    ref_yaw = yaw;
    rot_amt = 0;

    printf("IMU calibrated.\n");
    blink(2,100);

    imu_calibrated = true;
    imu_failures=0;

    }

    else 
        printf("Skipping IMU...broken.\n");

    core_1_watchdog = true;
    printf("halting until assignment...");

    while(!assign){
        core_1_watchdog = true;
        putchar('\r');
    }

    printf("assigned. Initializing...");
    sleep_ms(500);

    if(!skip_imu){
    imu_buf[2] = IMU_INIT;
    publish_with_strlen_qos0(RC_TOPIC,imu_buf,3);
    }
    
    ult_buf[2] = ULT_DIST;
    imu_buf[2] = YAW;
    bat_buf[2] = BAT;
    esp_buf[2] = ESP_IP;
 
    gpio_put(ULT_LED_PIN,0);

    #ifdef BAT_CONNECT
        get_battery_percentage(&bat_buf[3]);
    #endif

    #ifdef ESP_CONNECT
        if(!watchdog_caused_reboot()&& watchdog_hw->scratch[0]==0)
            get_esp_ip(&esp_buf[3]);
        else{
            uint32_t __ip;
            memcpy(&__ip, &watchdog_hw->scratch[0], 4);
            __ip = swap_endian(__ip);
            memcpy(&esp_buf[3], &__ip, 4);
        // memcpy(&esp_buf[3], &watchdog_hw->scratch[0], 4);
        }

    #endif


    esp_buf_flag = 1;
    bat_buf_flag = 1;    



    u16_t temp = 0;
    float abs_y,abs_ry,last_yaw;

    printf("done. Entering loop.\n");
    start_timer(&ultra_timer,ULT_INTERVAL);
    start_timer(&core_1_wd_timer,1000);
    start_timer(&bat_esp_timer, 10 * 1000);


    imu_calibrated =true;

    while(1) {
        //watchdog_update();
        core_1_watchdog = true;

    if(!skip_imu){
        poll_imu();
        effective_yaw = yaw+imu_offset;
        imu_buf_flag = 1;



  //     printf("%.2f\t%.2f\t%d\n",dist,yaw,val);
     //  sleep_ms(10);


             for (int i = 0; i < IMU_PARAM_COUNT; i+=2)
        {
            //convert normalized floats to half float in u16 format
            temp = half_float(effective_yaw/EULER_NORM);
            imu_buf[i + 3] = (temp >> 8) & 0xff;
            imu_buf[i + 4] = temp & 0xff;
        }

        if (prop_steer && imu_calibrated)
        {
            
            abs_y = fabs(yaw+180);
            abs_ry = fabs(ref_yaw+180);



            dist = fabs(abs_y<180+abs_ry && abs_ry < 180+abs_y ? abs_y-abs_ry : abs_y > abs_ry ? 360-abs_y+abs_ry : 360-abs_ry+abs_y );

            rot_amt = clamp(clamp(dist, 30) * Kp, 65535);

            int pin = (dist < 180) ? (last_y < 0 ? (yaw > ref_yaw ? STEER_L : STEER_R) :  (yaw > ref_yaw ? STEER_R : STEER_L)) : (last_y < 0 ? (yaw > ref_yaw ? STEER_R : STEER_L) :  (yaw > ref_yaw ? STEER_L : STEER_R));

            pwm_set_gpio_level(pin,rot_amt);

          //  dist = fabs(abs_y<180+abs_ry && abs_ry < 180+abs_y ? abs_y-abs_ry : abs_y > abs_ry ? 360-abs_y+abs_ry : 360-abs_ry+abs_y );

           // pwm_set_gpio_level(yaw>ref_yaw ? (ref_yaw > 180 + yaw ? STEER_R : STEER_L) : (ref_yaw + 180 < yaw ? STEER_L : STEER_R),rot_amt);

           // dist = fabs((abs_y > 90 || abs_ry > 90) ? ((abs_ry > 90 && abs_y > 90) ? ((yaw * ref_yaw > 0) ? abs_y - 180 - (abs_ry - 180) : abs_y - 180 + abs_ry - 180) : ((yaw * ref_yaw > 0) ? abs_y - abs_ry : abs_ry + abs_y)) : yaw - ref_yaw);

           // rot_amt = clamp(clamp(dist, 30) * Kp, 65535);

            //pwm_set_gpio_level(yaw < ref_yaw ? (last_y < 0 ? STEER_R : STEER_L) : (last_y < 0 ? STEER_L : STEER_R), rot_amt);

            //printf("%c\t%d\t%.2f\t%.2f\t%.2f\n",pin == STEER_L ? "l":"r",rot_amt,dist,ref_yaw,yaw);
        }

    }


        


        
        #ifdef ULT_CONNECT
            if(timer_expired(&ultra_timer)){

            float _ult_dist = hcsr04_get_distance_cm(TRIGGER_GPIO, ECHO_GPIO);
            // ult_dist = kalman(calc_average(TRIGGER_GPIO,ECHO_GPIO));

            ult_dist = filter_median_kalman(&smooth,_ult_dist,RANGE);
            // ult_dist = filter_average_kalman(&smooth,_ult_dist,RANGE_AND_ROC);
            // printf("Distance %.2f\n",ult_dist);
                temp = half_float(ult_dist/VALUE_THRESH);
                ult_buf[3] = (temp>>8)&0xff;
                ult_buf[4] =  temp&0xff;
                
                ult_buf_flag = 1;
                start_timer(&ultra_timer,ULT_INTERVAL);
            }


            if(ult_dist < ULT_THRESH) {
            //       gpio_put(ULT_LED_PIN,1);
                    ult_buf[5] = 1;
                    obj_flag = true;
            }

            else {
            //  gpio_put(ULT_LED_PIN,0);
                ult_buf[5] = 0;
                obj_flag = false;

                if(hard_stopped)
                    hard_stopped = false;


            }
        #endif


        if(timer_expired(&bat_esp_timer)){
            #ifdef BAT_CONNECT
                get_battery_percentage(&bat_buf[3]);
            #endif

            #ifdef ESP_CONNECT
                if(!watchdog_caused_reboot() && watchdog_hw->scratch[0]==0)
                    get_esp_ip(&esp_buf[3]);
                esp_buf_flag = 1;
                bat_buf_flag = 1;
                printf("esp_ip = %x%x%x%x\n",esp_buf[3],esp_buf[4],esp_buf[5],esp_buf[6]);
            #endif
            start_timer(&bat_esp_timer,10* 1000);
        }



    }
}


void set_imu_offset() {
   imu_offset = -yaw ;

}


s16_t arduino_map(s16_t x, s16_t in_min, s16_t in_max, s16_t out_min, s16_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

uint32_t swap_endian(uint32_t value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >>  8) & 0x0000FF00) |
           ((value <<  8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}