#define STATIC_HOST_IP   "192.168.1.137" // IP of my laptop 
#define HOTSPOT_IP       "192.168.137.1"
#define ASSIGN_TOPIC     "/rc_ctrl/assign"
#define CTRL_TOPIC       "/rc_ctrl/+"
#define RC_TOPIC         "/rc/com"
#define ASSIGN_REQ       "unassigned"

#define ACK              0x6
#define ENQ              0x5 

// operation codes (opcodes recieved from ctrl_client)
#define MOVE_OP          0x0
#define SCRIPT_OP        0x1
#define WAIT_OP          0x2
#define MSG_OP           0x3
#define YMOVE_OP         0x4
#define PRINT_OP         0xe
#define ASSIGN_OP        0x1a
#define PARAMS_OP        0x1f

// request codes
#define RSSI_REQ         0x21


// flags
#define SCRIPT_INCOMING  0x1
#define SCRIPT_BEGIN     0xd
#define SCRIPT_END       0x4                 
#define SCRIPT_RECEIVED  (ACK << 4) | SCRIPT_END  
#define SYNC_STATUS      0x2

#define MATLAB_FLAG      0x40

 

// drive related constants
#define SPEED_MULT      512
#define STEER_THRESH    30
#define DRIVE_A         14
#define DRIVE_B         15
#define STEER_A         12 
#define STEER_B         13


// multicore-related constants

#define IMU_PARAM_COUNT 1
#define IMU_BUF_LEN     (IMU_PARAM_COUNT * 2)+3
#define SETTLE_THRESH   0.0005
#define RESET_THRESH    20

#define EULER_NORM      180.0f


#define YAW             0x0
#define IMU_INIT        0x1


const float Kp = 5109.0; //5109.0;






