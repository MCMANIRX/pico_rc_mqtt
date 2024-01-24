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

// request codes
#define RSSI_REQ          0x21


// flags
#define SCRIPT_INCOMING  0x1
#define SCRIPT_BEGIN     0xd
#define SCRIPT_END       0x4                 
#define SCRIPT_RECEIVED  (ACK << 4) | SCRIPT_END  
#define SYNC_STATUS      0x2

 

// drive related constants
#define SPEED_MULT      512
#define STEER_THRESH    30
#define DRIVE_A         14
#define DRIVE_B         15
#define STEER_A         12 
#define STEER_B         13





//static float tsm = 1;
//static float dsm = 1;

