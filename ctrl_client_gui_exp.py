# GUI frontend for ccgbackend

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *
import qdarktheme


import rc_config as rc
import ccgbackend as cc



import sys
import os

    

# Client gui object subclass for CtrlClientGui

class Client():
            
    def __init__(self,id):
        self.data = QTableWidget()
        self.id = id
        old_id = self.id
        self.data.setColumnCount(2)
        self.data.setRowCount(4)
        self.data.setHorizontalHeaderLabels(['Client', str(id)])    
          
        
    def update_id(self,id):
        self.data.setHorizontalHeaderLabels(['Client', str(id)])
        self.id = id

        
    
    g_y_r  = [QColor(0,255,0),QColor(255,255,0),QColor(255,0,0)]
    
        
    #update client RSSI with cool colors    
    def update_rssi(self,val):
        
        label = QTableWidgetItem("RSSI")
        value = QTableWidgetItem(str(val))
        
        idx = 0
            
        if val <= -70 or val > 0:
            idx = 2

        elif val <= -60:
            idx = 1
                        
        label.setForeground(self.g_y_r[idx]) 
        value.setForeground(self.g_y_r[idx])  
            
        self.data.setItem(0, 0, label)
        self.data.setItem(0, 1, value)
        

    def update_yaw(self,val):
        
        label = QTableWidgetItem("Yaw")
        value = QTableWidgetItem(str(round(val*180.0,4)))
            
        self.data.setItem(1, 0, label)
        self.data.setItem(1, 1, value)
            
    def update_ult(self,val):
        label = QTableWidgetItem("Ult")
        value = QTableWidgetItem(str(round(val*300.0,4)))
            
        self.data.setItem(2, 0, label)
        self.data.setItem(2, 1, value) 
        
    def update_bat(self,val):
        label = QTableWidgetItem("Bat")
        value = QTableWidgetItem(str(val)+"%")
            
        self.data.setItem(3, 0, label)
        self.data.setItem(3, 1, value)          
                
    def update_values(self,keys_values):
    
        self.data.setRowCount(len(keys_values))

        for row, (property_name, property_value) in enumerate(keys_values):
            self.data.setItem(row, 0, QTableWidgetItem(property_name))
            self.data.setItem(row, 1, QTableWidgetItem(property_value))
            
        self.data.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
   
   
   
        
class CtrlClientGUI(QMainWindow):
    
    ctrl_modes = ["Joystick","Trigger"]
    ctrl_mode_index = 0

    v1_buttons = ["Tune Controller","Switch Control Mode","Re-home IMUs", "Run Script","Clear Log"] #Get RSSIs removed
    v1_radios  = ["Auto-Reassign"]
    menu_options = ["New Script","Open Script","Save Script","Run Script"]
  
    clients = []
    client_update_signal = pyqtSignal(list,list,list,list,bool)
    ext_logText_signal   = pyqtSignal(str)
    run_script_signal    = pyqtSignal()

    filePath = ""
    
    auto_rename = False
    
    

 
    def __init__(self):
        super().__init__()
        
        self.init_ui()
            
        
    def separator_gen(self):
              
        separator_line = QFrame()
        separator_line.setFrameShape(QFrame.VLine)  # Use QFrame.VLine for a vertical separator
        separator_line.setFrameShadow(QFrame.Sunken)  # Other options: QFrame.Plain, QFrame.Sunken, QFrame.Raised
        separator_line.setLineWidth(15)  # Set the desired width in pixels

        separator_layout = QVBoxLayout()
        separator_layout.addWidget(separator_line)
        
        return separator_layout
    
    
  
    def logText(self,txt):
        
        self.log.insertHtml(rc.ansi2html(txt,palette='solarized'))
        self.log.append('')
        self.scrollbar = self.log.verticalScrollBar()
        self.scrollbar.setValue(self.scrollbar.maximum())
        
    def menu_handler(self):
        
        arg = self.sender().text()
        
        # new script
        if arg == self.menu_options[0]:
            self.script_edit.setPlainText('')
            self.filePath = None

        # open script
        elif arg == self.menu_options[1]:
            options = QFileDialog.Options()
            self.filePath, _ = QFileDialog.getOpenFileName(self, 'Open File', '', 'RC Script files (*.rcs);;All Files (*)', options=options)
            if self.filePath:
                with open(self.filePath, 'r') as file:
                    content = file.read()
                    self.script_edit.setPlainText(content)
                    self.v2_label.setText(os.path.basename(file.name))
       
        # save script             
        elif arg == self.menu_options[2]:
            options = QFileDialog.Options()
            if self.filePath:
                with open(self.filePath, 'w') as file:
                    file.write(self.script_edit.toPlainText())
                self.logText(f"File '{os.path.basename(file.name)}' saved.")
                return
            else:   
                self.filePath, _ = QFileDialog.getSaveFileName(self, 'Save File', '', 'RC Script files (*.rcs);;All Files (*)', options=options)
                if self.filePath:
                    with open(self.filePath, 'w') as file:
                        file.write(self.script_edit.toPlainText())
                    self.logText(f"File '{os.path.basename(file.name)}' saved.")
                else:
                    self.logText("File not saved.")
        
        # run script
        elif arg == self.menu_options[3]:
            self.run_script()
            
    def radio_handler(self):
        arg = self.sender().text()
        
        if arg == self.v1_radios[0]:
            self.auto_rename = not self.auto_rename
        
    def button_handler(self):
        arg = self.sender().text()

        if arg == self.v1_buttons[0]:
            None
            
        # change ctrl mode   
        elif arg == self.v1_buttons[1]:
            self.ctrl_mode_index = (self.ctrl_mode_index + 1) % len(self.ctrl_modes)
            self.ctrl_mode_indicator.setText("Control Mode: "+'<font color="yellow">'+self.ctrl_modes[self.ctrl_mode_index]+'</font>')

            
            cc.set_ctrl_mode(self.ctrl_mode_index)
        
        # rehome 
        elif arg == self.v1_buttons[2]:
            cc.rehome()

                            
        # run script
        elif arg == self.v1_buttons[3]:
            self.run_script()
        
        # clear log
        elif arg == self.v1_buttons[4]:
            self.log.setPlainText("")
            
        # rssi req    
       # elif arg == self.v1_buttons[4]:
        #    cc.rssi_flag = True
                
    def add_client(self,id):
        
        new_client = Client(id)
        self.v3.addWidget(new_client.data)
        self.clients.append(new_client)

    def removeAllClients(self):
        while self.v3.count()>1:
            item = self.v3.takeAt(0)
            widget = item.widget()
            if widget:
                widget.setParent(None)
                widget.deleteLater()
       # self.clients = []        
            
    
    def remove_client(self,client):
        for x in range(1,self.v3.count()):
            
            if(self.v3.itemAt(x)):
                table = self.v3.itemAt(x).widget()
                
                if isinstance(table,QTableWidget):
                    client_id = table.horizontalHeaderItem(1).text()
                    
                    if str(client_id) == str(client.id):
                        self.v3.removeWidget(table)
                        table.deleteLater()
                        if client in self.clients:
                            self.clients.remove(client)
                
    
    
    def run_script(self):
        cc.script_run()        
        
    # add and remove clients based on backend tracking
    def client_update(self,client_ids,rssi_values,client_and_id,yaw_values,pulse):

        
       # print(yaw_values)
        
        change = False

        if pulse:
            
            
            if len(self.clients) < len(client_ids):
                change = True
                for client_id in client_ids:
                    if not any(client.id == client_id for client in self.clients):
                        self.add_client(client_id)

                
                        
            elif len(self.clients) > len(client_ids):
                change = True
                for client in self.clients:
                    if client.id not in client_ids:
                        self.remove_client(client)
                #self.removeAllClients()
                                            
        
            if (self.auto_rename and client_and_id):
                for client in self.clients:
                    if client_and_id and client.id == client_and_id[0]:
                        client.update_id(client_and_id[1])
                return
            
            for rssi_datum in rssi_values:
                for client in self.clients:
                    if client.id == rssi_datum['rssi_data'][0]:
                        client.update_rssi(rssi_datum['rssi_data'][1])
        else:
            for yaw in yaw_values:
                for client in self.clients:   
                    first_key = next(iter(yaw.keys()))
                    if client.id == next(iter(yaw.values()))[0]:
                        if first_key == 'yaw_data':
                            client.update_yaw(yaw['yaw_data'][1])   
                        elif first_key == 'ult_data':
                            client.update_ult(yaw['ult_data'][1])
                        elif first_key == 'bat_data':
                            client.update_bat(yaw['bat_data'][1])


        if(change):
            self.v3.addStretch()


        self.v3_label.setText(f'Connected Clients: {len(self.clients)}')


    
    # start backend polling loop    
    class bt(QThread):

        def run(self):
            cc.polling_loop()    
            
        
    def init_ui(self):
        
        # formatting
        font = self.font()
        font.setPointSize(10)  # Set the desired font size
        QApplication.setFont(font)
        
        self.setWindowTitle("RC Car Control Client Suite v0.1") 
        self.label = QLabel(self)
        
        # geometry set
        desktop = QDesktopWidget()
        screen_size = desktop.screenGeometry()
        w = screen_size.width()/1.5
        h = w/1.5
        self.setGeometry(int(screen_size.width()/2-w/2),int(screen_size.height()/2-h/2),int(w),int(h)) 
        
        # master layout set
        self.master = QWidget(self)
        self.setCentralWidget(self.master)
        self.v_master = QVBoxLayout(self.master)   
        
        # pygame polling set
       # self.timer = QTimer(self)
        # Connect the timeout signal of the timer to the update_counter slot
       # self.timer.timeout.connect(cc.pygame_loop)
        # Set the interval in milliseconds (e.g., 1000 ms = 1 second)
       # self.timer.start(POLL_RATE)
       
        menubar = self.menuBar()
        fileMenu = menubar.addMenu('File')
        
        for option in self.menu_options:
           self.o = QAction(option, self)
           self.o.triggered.connect(self.menu_handler)
           fileMenu.addAction(self.o)
       
        
        # v1
        
        self.v1 = QVBoxLayout()
        self.v1_label = QLabel("Tools")

        
        for button in self.v1_buttons:
           self.b = QPushButton(self)
           self.b.setText(button)
           self.b.clicked.connect(self.button_handler)
           self.v1.addWidget(self.b)
        
        for radio in self.v1_radios:
            self.b = QRadioButton(self)
            self.b.setText(radio)
            self.v1.addWidget(self.b)
            self.b.toggled.connect(self.radio_handler)
            
        self.v1.addSpacerItem(QSpacerItem(0, 20, QSizePolicy.Minimum, QSizePolicy.Minimum))
            
        self.ctrl_mode_indicator = QLabel("Control Mode: "+'<font color="yellow">'+self.ctrl_modes[self.ctrl_mode_index]+'</font>')
        self.v1.addWidget(self.ctrl_mode_indicator)

        self.v1.addStretch(1)
        self.v1.setSpacing(3)
        
     
        # v2
              
        self.v2 = QVBoxLayout()
        self.v2.setSpacing(1)
        
        self.v2_label = QLabel("New") # script name 
        
        self.v2_splitter = QSplitter()
        self.v2_splitter.setOrientation(Qt.Vertical)
        
        self.script_edit = QPlainTextEdit()
        self.log = QTextEdit()
        self.log.setReadOnly(True)
        
        self.script_edit.setMinimumHeight(self.label.sizeHint().height()*1)
        self.log.setMinimumHeight(self.label.sizeHint().height()*4)
    
        self.script_edit.setMinimumWidth(self.label.sizeHint().height()*36)
        
        self.v2_splitter.addWidget(self.script_edit)
        self.v2_splitter.addWidget(self.log)

        self.v2.addWidget(self.v2_splitter)


        
        
         
        # v3 
        
        self.v3 = QVBoxLayout()
        self.v3.setSpacing(1)
        
        self.v3_label = QLabel("Connected Clients: 0") # connected clients
        
        # keeps shape of v3
        self.v3_placeholder = QWidget()
        self.v3.addWidget(self.v3_placeholder)


       # self.v3.addWidget(self.v3_label)
        
        
     ################################################################################
        
        # final setup 
        
        v1_v2_space = QSpacerItem(0, 0, QSizePolicy.MinimumExpanding, QSizePolicy.Minimum)
        v2_v3_space = QSpacerItem(10, 10, QSizePolicy.Expanding, QSizePolicy.Minimum)



        self.h0 = QHBoxLayout()
        self.h0.addWidget(self.v1_label)
        self.h0.addSpacerItem(v1_v2_space)
        self.h0.addWidget(self.v2_label)
        self.h0.addSpacerItem(v2_v3_space)
        self.h0.addWidget(self.v3_label)
        
        
        self.h1 = QHBoxLayout()
        self.h1.addLayout(self.v1)
        self.h1.addLayout(self.separator_gen())
        self.h1.addLayout(self.v2)
        self.h1.addLayout(self.separator_gen())
        self.h1.addLayout(self.v3)
        

        self.v_master.addLayout(self.h0)
        self.v_master.addLayout(self.h1)
        
        # start backend as QThread
        self.backend_thread = self.bt()
        self.backend_thread.start()
        
        # setup signals from QThread
        self.client_update_signal.connect(self.client_update)
        self.ext_logText_signal.connect(self.logText)
        self.run_script_signal.connect(self.run_script)



        

        
    

def init_ui():
    app = QApplication(sys.argv)
    qdarktheme.setup_theme()
    app.setStyle('Fusion')
    gui = CtrlClientGUI()
    cc.get_gui_instance(gui)
    gui.show()
    app.exec_()
#sys.exit(app.exec_())

init_ui()