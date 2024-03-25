% Create MQTT client
broker = 'tcp://localhost'; % MQTT broker URL
clientID = 'myClient';  % Client ID

mqttClient = mqttclient(broker,clientID = "feet", Port = 1883);

% Publish message
message = 'connected';
topic = "/rc/com";

write(mqttClient, topic, message);

% Subscribe to topic
subscribe(mqttClient, topic);

% Print a message to confirm subscription
disp(['Subscribed to topic: ' topic]);

% Callback function to confirm subscription
function myMQTT_Callback(topic, message)
fprintf('connected')
end
