rc_config

init_mqtt()




% Define the function to be executed
function enquire_rssi(~, ~)
    global ENQ
    global ENQUIRE_TOPIC
    global client
    enquire = char(ENQ);
    pub(ENQUIRE_TOPIC,enquire);
end



function init_mqtt()



    global RC_TOPIC;
    global clientID;
    clientID     = "rc_matlab";
    addr         = "tcp://localhost";

    port     =  1883;

    
    global client
    client =  mqttclient(addr, Port = port);
    
    %subscribe(client,RC_TOPIC,Callback=@echo_)
    

    % Create a timer object
    rssi_timer = timer('ExecutionMode', 'fixedRate', ... 
                     'Period', 1, ... % Interval of 1 second
                     'TimerFcn', @enquire_rssi);
    
    % Start the timer
    start(rssi_timer);

end





