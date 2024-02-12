% Various constants and shared functions for the control clients and local client test

global CTRL_TOPIC ASSIGN_TOPIC ACTION_TOPIC SCRIPT_TOPIC ENQUIRE_TOPIC RC_TOPIC ...
       MOVE_OP SCRIPT_OP WAIT_OP MSG_OP YMOVE_OP PRINT_OP ASSIGN_OP PARAMS_OP ...
       RSSI_REQ YAW INIT_IMU SCRIPT_INCOMING SCRIPT_BEGIN SCRIPT_END ...
       SCRIPT_RECEIVED SYNC_STATUS ACK ENQ move_direction_map DEAD_ZONE ...
       STEER_THRESH TIMEOUT to_8_signed from_8_signed from_16_float ...
       clamp arduino_map MATLAB_FLAG;


CTRL_TOPIC      = '/rc_ctrl/+';
ASSIGN_TOPIC    = '/rc_ctrl/assign';
ACTION_TOPIC    = '/rc_ctrl/action';
SCRIPT_TOPIC    = '/rc_ctrl/script';
ENQUIRE_TOPIC   = '/rc_ctrl/enq';
RC_TOPIC        = '/rc/com';

% command opcodes (byte 1)
MOVE_OP         = uint8(hex2dec('0'));
SCRIPT_OP       = uint8(hex2dec('1'));
WAIT_OP         = uint8(hex2dec('2'));
MSG_OP          = uint8(hex2dec('3'));
YMOVE_OP        = uint8(hex2dec('4'));
PRINT_OP        = uint8(hex2dec('e'));
ASSIGN_OP       = uint8(hex2dec('1a'));
PARAMS_OP       = uint8(hex2dec('1f'));


% MATLAB encryption byte (character ONLY MQTT?????)
MATLAB_FLAG     = uint8(hex2dec('40'));

% request codes
RSSI_REQ        = uint8(hex2dec('21'));
YAW             = uint8(hex2dec('0'));
INIT_IMU        = uint8(hex2dec('1'));

% byte 2
SCRIPT_INCOMING = uint8(hex2dec('1'));
SCRIPT_BEGIN    = uint8(hex2dec('d'));
SCRIPT_END      = uint8(hex2dec('4')); % byte 1 client-side
SCRIPT_RECEIVED = bitor(bitshift(uint8(hex2dec('6')), 4), uint8(hex2dec('4'))); % byte 2 client-sides
SYNC_STATUS     = uint8(hex2dec('2'));

ACK             = uint8(hex2dec('6'));
ENQ             = uint8(hex2dec('5'));

move_direction_map = containers.Map({'f', 'b', 'r', 'l'}, {-1, 1, 1, -1});

DEAD_ZONE       = 20;
STEER_THRESH    = 40;

TIMEOUT         = 10;

% I miss C...
to_8_signed     = @(x) bitand(bitxor(abs(x), 255), 255) - bitand(uint8(x < 0), 1);
from_8_signed   = @(x) uint8(bitand(x, 255)) - bitand(uint8(bitshift(x, -7)), 1) * 255;
from_16_float   = @(x) (bitand(uint16(x), 65535) - bitand(bitshift(uint16(x), -8), 255) * 256) / 32767.0;

clamp           = @(value, min_val, max_val) min(max(value, min_val), max_val);

% Map the value from one range to another
arduino_map     = @(value, from_low, from_high, to_low, to_high, dead_zone) ...
    (clamp(int32((value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low), to_low, to_high) .* (abs(value) > abs(dead_zone)));

