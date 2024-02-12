function pub(topic,payload)
    global client
    write(client, topic, payload)
end