function move_motors(id,x,y)
    global MOVE_OP;
    global ACTION_TOPIC
    id = int8(id);
    x = int8(x);
    y = -int8(y);
    payload = char(encode_message([id, MOVE_OP, x, y]));
    pub(ACTION_TOPIC,payload);
end