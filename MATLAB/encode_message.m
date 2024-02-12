function ret = encode_message(payload)
    global MATLAB_FLAG
    idx = 3;

    bitfield = 0;

    
    for x = 2:length(payload)
        if payload(x) < 0
            bitfield = bitset(payload(2), 8 - idx + 1);
            payload(x) = abs(payload(x));
            idx = idx + 1;
        end
    end

    payload = [payload(1:1), MATLAB_FLAG, payload(2:end)]

    payload(2) = bitor(payload(2),bitfield);

    
    ret = payload;
end

