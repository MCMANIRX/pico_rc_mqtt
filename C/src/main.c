/*
*   TODO:
*
*   1. P axis lock (use IMU to drive straight on an axis with PID loop)
*   2. Add functionality (script) to set local x y or z axis
*   3. Add data send functionality (script)
*   4. Multicore for I2C interface polling
*
*
*/


#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "hardware/pwm.h"
#include "hardware/vreg.h"
#include "rc_mqtt.h"

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


s16_t arduino_map(s16_t x, s16_t in_min, s16_t in_max, s16_t out_min, s16_t out_max);


/* non-blocking wait with spinning wheel to make it work */

bool waiting;

int alarm_cb(alarm_id_t id, void *user_data)
{

    waiting = false;
    return 0;
}

char wheel[] = {'-', '\\', '|', '/'};

void wait(int time)
{

    waiting = true;
    add_alarm_in_ms(time, alarm_cb, NULL, false);
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

    if (y < 0)
    {
     //   pwm_set_gpio_level(DRIVE_B, abs(127 * SPEED_MULT)); 
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
        if (x < 0)
        {

        pwm_set_gpio_level(STEER_A, abs(x * SPEED_MULT));
        pwm_set_gpio_level(STEER_B, 0);


           // gpio_put(STEER_A, 1);
          //  gpio_put(STEER_B, 0);
        }

        else if (x > 0)
        {

        pwm_set_gpio_level(STEER_A, 0);
        pwm_set_gpio_level(STEER_B,  abs(x * SPEED_MULT));
       //     gpio_put(STEER_B, 1);
       //     gpio_put(STEER_A, 0);
        }
    }
    else
    {

    pwm_set_gpio_level(STEER_A, 0);
    pwm_set_gpio_level(STEER_B, 0);

     //   gpio_put(STEER_B, 0);
     //   gpio_put(STEER_A, 0);
    }
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

bool this;
bool all;
bool receiving = false;

void message_decoder(char *topic, char *data)
{
    all = false;
    this = false;

    if (data[0] == assign_data)
        this = true;
    else if (data[0] == 0xff)
        all = true;

   // printf("%s %x %x %x %x \n",topic,data[0], data[1], data[2], data[3]);


    // enquire logic
    if (*(topic + 10) == 'n')
    {
        if (assign && strlen(id) <= 2)
        {
            //blink(1,1);

            ack[0] = assign_data;
            ack[1] = ACK;
            publish_with_strlen_qos0(RC_TOPIC, ack, 2); // QoS 0 to alleviate packet loss errors
        }
    }

    // assign logic
    else if (*(topic + 10) == 's')
    {
        if (this || !assign)
        {
            assign = true;
            disconnect();
            sleep_ms(40);
            assign_data = data[2];
            sprintf(id, "%d", assign_data);
            set_id_for_reconnect(id);
            connect(&id);
        }
    }

    // action logic
    else if (*(topic + 11) == 't')
    {
        if (!script_active)
            if (this || all){
                if (data[1] == MOVE_OP)
                    move_motors((s8_t)data[2], (s8_t)data[3]);
                else if (data[1] == RSSI_REQ)
                    send_rssi();
            }
    }
    
    // script logic
    else if (this || all) {
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
                while (deque(&op_queue, &cmd))
                    run_command(cmd.data);
                
                   // printf("%x %x %x %x\n", cmd.data[0],cmd.data[1],cmd.data[2],cmd.data[3]);
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
    /*
                if((msg.payload[0] == assign_data^0x80) or  msg.payload[0] == 0xff):
                if(msg.payload[1] == rc.SCRIPT_OP):
                    if(msg.payload[2] == rc.SCRIPT_INCOMING):
                        synched = False
                        cmd_buf = []
                        operations = msg.payload[3]
                        receiving = True
                    elif(msg.payload[2]== rc.SCRIPT_BEGIN):
                        if(cmd_buf):
                            script_active = True
                            for cmd in cmd_buf:
                                msg.topic = rc.SCRIPT_TOPIC
                                msg.payload = cmd
                                run_script(client,msg) //weird python thing won't let me recursively call this on_message fcn...
                            client.publish("/rc/com", ((int(client_id) << 8) | rc.SCRIPT_END).to_bytes(2,'big'))
                            cmd_buf = []

                    elif(msg.payload[2] == rc.SYNC_STATUS):
                        synched = msg.payload[3]
                    elif(msg.payload[2] == rc.SCRIPT_END):
                        if(receiving):
                            client.publish("/rc/com", ((int(client_id) << 8) | rc.SCRIPT_RECIEVED).to_bytes(2,'big'))
                            receiving = False
                            print(cmd_buf)
                if receiving:
                    cmd_buf.append(msg.payload)
           */
}

int main()
{
    /* script init */
    op_cnt = 0;

    script_active = false;
    /* PWM init */
    gpio_set_function(DRIVE_A, GPIO_FUNC_PWM);
    gpio_set_function(DRIVE_B, GPIO_FUNC_PWM);

    gpio_set_function(STEER_A, GPIO_FUNC_PWM); 
    gpio_set_function(STEER_B, GPIO_FUNC_PWM);

   // gpio_init(STEER_A);
   // gpio_init(STEER_B);
   // gpio_set_dir(STEER_A, GPIO_OUT); 
   // gpio_set_dir(STEER_B, GPIO_OUT); 

    drive_slice = pwm_gpio_to_slice_num(14);
    steer_slice = pwm_gpio_to_slice_num(12);

    pwm_set_enabled(drive_slice, true);
    pwm_set_enabled(steer_slice, true);
    

    //   pwm_set_enabled(steer_slice, true);
    // #################################################



    stdio_init_all();

    /* overclock (optional) */
   // vreg_set_voltage(VREG_VOLTAGE_1_30);
   // set_sys_clock_khz(250000,true);

    //connect to wifi and MQTT broker
    mqtt_init("rc_unassigned");

    //set local message decoder callback
    set_message_decoder(&message_decoder);

    int x = 0;

    while (1)
    {
        if (!assign)
        {
            wait(500);
            publish(RC_TOPIC, ASSIGN_REQ);
            wait(500);
        }
        // wheel just in case loop is too tight
        else /*while (x < 3)
        {
            putchar(wheel[x++]);
            putchar('\r');
        }
        */x = 0;
        }
        
    
    
    return 0;
}

s16_t arduino_map(s16_t x, s16_t in_min, s16_t in_max, s16_t out_min, s16_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

