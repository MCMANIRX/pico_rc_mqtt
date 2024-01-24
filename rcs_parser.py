# RC Script (*.rcs) file parser and interpreter

# NOTE:
# Command Structures (MSB first):
#
#--------------------------------------------------------
# ECHO 
#
# byte 0: ID
# byte 1: MSG_OP (3)
# byte 2: null (0)
#
# example: echo("Aggie Pride!")
#--------------------------------------------------------
# WAIT
#
# byte 0: ID
# byte 1: WAIT_OP (2)
# byte 2: hi byte of 16 bit duration in ms
# byte 3: lo byte of 16 bit duration in ms
#
# example: wait(1000)
#--------------------------------------------------------
# YMOVE
#
# byte 0: ID
# byte 1: YMOVE_OP (4)
# byte 2: y-axis speed + dir as int8_t 
# byte 3: hi byte of 16 bit duration in ms
# byte 4: lo byte of 16 bit duration in ms
#
# example: ymove(0,f,100,1000)
#--------------------------------------------------------
# MOVE
#
# byte 0: ID
# byte 1: MOVE_OP (0)
# byte 2: y-axis speed + dir as int8_t 
# byte 3: x-axis dir as int8_t
# byte 4: hi byte of 16 bit duration in ms
# byte 5: lo byte of 16 bit duration in ms
#
# example: move(0,f,100,r,1000)
#--------------------------------------------------------
import re
import rc_config as rc
import time
import io


ESC_FILTER = ''.join([chr(i) for i in range(1, 32)])

KEYWORDS = ['sync','sync_on_start','car(']
COMMANDS = ['echo','wait','ymove','move']

global _time
_time = 0

# show off how cool and efficient the code is
def timekeep_start(topic):
    print("Beginning",topic,"...")
    global _time
    _time = time.time()
    
def timekeep_stop(topic):
    global _time
    print(topic,"time was",str((time.time()-_time)*1000),"miliseconds.")
    

############################################## parse utility fcns
def getParenValue(line):
     match = re.search(r'\((.*?)\)', line)
     if match:
         return match.group(1)

def getText(txt):
    match = re.search(r'"(.*?)"', getParenValue(txt))
    if match:
        return match.group(1)
            
def getCommand(line):
    return line.translate(str.maketrans('', '', ESC_FILTER))
#############################################################

# Huge manual mess of a parser function. Be safe reading this.
def parseScript(_script,isFile):  
    topic = "Parse"
    sequences = []
    command_buf = []    
    synched = 0
    seq_continuous = False
    block = False
    
    timekeep_start(topic)
    if(isFile):
        script =  open(_script,'r')
    else:
        script = io.StringIO(_script)
    line = script.readline()
    
    while line:
        
        if not(line.find("#") >=0):
            
        
            for word in COMMANDS:
                if line.find(word+'(') >=0:
                    command_buf.append(getCommand(line))
                    seq_continuous = True
                    break
                    
                    
            for word in KEYWORDS:
                if line.find(word) >=0:
                    block = True
                    if word == KEYWORDS[0]:
                        synched = 1
                    elif word == KEYWORDS[1]:
                        synched = 2
                    elif word == KEYWORDS[2]:
                        car = {}
                        commands = []
                        car.update({"id" : getParenValue(line)})
                        while(True):
                            _line = script.readline().split('}')
                            commands.append(getCommand(_line[0]))
                            if len(_line)>1:
                                break
                        car.update({"commands" : commands})
                        command_buf.append(car)

                    
        if (synched or block) and ((not line) or line.find('}')>=0):

            seq = {}
            seq.update({"sync" : synched})
            seq.update({"commands" : command_buf})
            command_buf = []
            synched = 0
            block = False
            sequences.append(seq)
                
        line = script.readline()
        
        for word in KEYWORDS:        
            if not(line.find("#") >=0):                     
                if block == False and line.find(word) >=0:
                    #print(word,seq_continuous)
                    block = True
                    break
                elif block == True and not synched:
                    seq = {}
                    seq.update({"commands" : command_buf})
                    command_buf = []
                    block = False
                    sequences.append(seq)
                    
            for word in COMMANDS:
                if block == True and line.find(word) >=0:
                    seq = {}
                    seq.update({"commands" : command_buf})
                    command_buf = []
                    block = False
                    sequences.append(seq)
                    
        if(seq_continuous):
            if block or not line:
                seq = {}
                seq.update({"commands" : command_buf})
                command_buf = []
                seq_continuous = False
                sequences.append(seq)
                block = False
            

    if(command_buf and block):
        seq = {}
        seq.update({"commands" : command_buf})
        command_buf = []
        block = False
        sequences.append(seq)
    
    timekeep_stop(topic) 
    return sequences

msg_buf = []
_cmd_buf =[]

global block_len
block_len = 0

# low-level text->binary converter logic
def compileCommands(command,id):
    global _cmd_buf
    global block_len


    if 'id' in command:
        for _command in command['commands']:
            compileCommands(_command,command['id'])
        return command['id']
    else:
        for word in COMMANDS:
            if command.find(word+'(') >=0:
                params = getParenValue(command).split(",")

                if word == COMMANDS[0]:
                    if id != 'none':
                        _cmd_buf.append(rc.compileCommand(id,rc.MSG_OP,0x0,3))
                    else:
                        _cmd_buf.append(rc.compileCommand(0xff,rc.MSG_OP,0x0,3))
                    msg_buf.append({'msg' : getText(command), 'id' : id})
                    break

                elif word == COMMANDS[1]:
                    if id != 'none':
                        _cmd_buf.append(rc.compileCommand(id,rc.WAIT_OP,params[0],4))
                    else:
                      _cmd_buf.append(rc.compileCommand(0xff,rc.WAIT_OP,params[0],4))
                    break
                
                elif word == COMMANDS[2]:
                    if id != 'none':
                        _cmd_buf.append(rc.compileCommand(id,rc.YMOVE_OP,rc.to_8_signed(rc.move_direction_map.get(params[0])*rc.arduino_map(int(params[1]),0,100,0,127,0))<<16 | int(params[2]),5))
                    else:
                        _cmd_buf.append(rc.compileCommand(params[0],rc.YMOVE_OP,rc.to_8_signed(rc.move_direction_map.get(params[1])*rc.arduino_map(int(params[2]),0,100,0,127,0))<<16 | int(params[3]),5))
                    break
                elif word == COMMANDS[3]:
                    if id != 'none':
                        _cmd_buf.append(rc.compileCommand(id,rc.MOVE_OP,
                                                          rc.to_8_signed(rc.move_direction_map.get(params[0])*rc.arduino_map(int(params[1]),0,100,0,127,0))<<24 
                                                        | rc.to_8_signed(rc.move_direction_map.get(params[2])*rc.STEER_THRESH)<<16
                                                        | int(params[3]),6))
                    else: 
                        _cmd_buf.append(rc.compileCommand(params[0],rc.MOVE_OP,
                                                          rc.to_8_signed(rc.move_direction_map.get(params[1])*rc.arduino_map(int(params[2]),0,100,0,127,0))<<24 
                                                        | rc.to_8_signed(rc.move_direction_map.get(params[3])*rc.STEER_THRESH)<<16
                                                        | int(params[4]),6))                        
                    break        

    block_len+=1
    return 0

# high-level text->binary converter for command sequences
def compileSequences(sequences):
    topic = "Compilation"
    global _cmd_buf
    global block_len 
    binary_sequences = []
    sync_status = 0
    sync_id = []
    block_len = 0
   #print(sequences)

    timekeep_start(topic)
    
    for sequence in sequences:
        sync_status = 0
        sync_id = []
        cmd_buf = {'commands' : [], 'len': []}

        if 'sync' in sequence:
            sync_status = sequence['sync']
            cmd_buf["sync"] = sequence['sync']
        
        if 'commands' in sequence:
            _cmd_buf = []       
            for commands in sequence['commands']:
                ret = compileCommands(commands,"none")
                if ret:
                    sync_id.append(ret)
                    _cmd_buf.insert(0,rc.compileCommand(ret,rc.SCRIPT_OP,((rc.SCRIPT_INCOMING << 8) | block_len-1),4))
                    block_len = 0
                    
            
                    

                    
        if(sync_id):
            cmd_buf.update({"sync_id" : sync_id})
            idx = 0
            for id in sync_id:
                while(_cmd_buf[idx][1] == rc.SCRIPT_OP and _cmd_buf[idx][2] == rc.SCRIPT_INCOMING):
                    idx+=1
                _cmd_buf.insert(idx,rc.compileCommand(id,rc.SCRIPT_OP,((rc.SYNC_STATUS << 8) | sync_status&0xff),4))
                
            _cmd_buf.append(rc.compileCommand(0xff,rc.SCRIPT_OP,((rc.SCRIPT_END << 8) | len(_cmd_buf)-1),4))

        cmd_buf.update({"commands": _cmd_buf})
        cmd_buf.update({"len":len(_cmd_buf)})

        binary_sequences.append(cmd_buf)
        

        
    timekeep_stop(topic)      
    print(binary_sequences)
    return binary_sequences


# entry point
def read_script(script,isFile):    
    
    return compileSequences(parseScript(script,isFile)), msg_buf

# returns list of sequence dictionaries and a message buffer dictionary





#sequences = parseScript(script)    
#binary_sequences = compileSequences(sequences) 

         
