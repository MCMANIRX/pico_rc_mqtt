# No longer compatible with ccgbackend and will probably be completely deleted. For archival purposes only.

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
import qdarktheme


import rc_config as rc
import ccgbackend as cc



import sys
import os


v1_buttons = ["Tune Controller","Reassign IDs", "Run Script","Get RSSIs"]
menu_options = ["New Script","Open Script","Save Script","Run Script"]


class CtrlClientGUI(QMainWindow):
    clients = []
    client_count = 0
    client_update_signal = pyqtSignal(list)
    ext_logText_signal   = pyqtSignal(str)
    filePath = ""

 
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
        if arg == menu_options[0]:
            self.script_edit.setPlainText('')
            self.filePath = None

        # open script
        elif arg == menu_options[1]:
            options = QFileDialog.Options()
            self.filePath, _ = QFileDialog.getOpenFileName(self, 'Open File', '', 'RC Script files (*.rcs);;All Files (*)', options=options)
            if self.filePath:
                with open(self.filePath, 'r') as file:
                    content = file.read()
                    self.script_edit.setPlainText(content)
                    self.v2_label.setText(os.path.basename(file.name))
       
        # save script             
        elif arg == menu_options[2]:
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
        elif arg == menu_options[3]:
            cc.script_run()
            
            
        
    def button_handler(self):
        arg = self.sender().text()
        # run script    
        if arg == v1_buttons[2]:
            cc.script_run()
            
        # rssi req    
        elif arg == v1_buttons[3]:
            cc.rssi_flag = True
                
    def add_client(self,client):
        self.new_client = QTableWidget()
        self.new_client.setColumnCount(2)
        self.new_client.setHorizontalHeaderLabels(['Client', str(client)])

        
        keys_values =[('foo','bar')]

        self.new_client.setRowCount(len(keys_values))

        for row, (property_name, property_value) in enumerate(keys_values):
            self.new_client.setItem(row, 0, QTableWidgetItem(property_name))
            self.new_client.setItem(row, 1, QTableWidgetItem(property_value))
            
        self.new_client.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)

        self.v3.addWidget(self.new_client)
        self.clients.append(client)

    # essentially deprecated? for r&d
    def removeAllClients(self):
        for x in range(1,self.v3.count()):
            if(self.v3.itemAt(x) and isinstance(self.v3.itemAt(x).widget(),QTableWidget)):
                table = self.v3.itemAt(x).widget()
                self.v3.removeWidget(table)
                table.deleteLater()
        self.clients = []
                

            
    
    def remove_client(self,client):
        for x in range(1,self.v3.count()):
            
            if(self.v3.itemAt(x)):
                table = self.v3.itemAt(x).widget()
                
                if isinstance(table,QTableWidget):
                    client_id = table.horizontalHeaderItem(1).text()
                    
                    if str(client_id) == str(client):
                        self.v3.removeWidget(table)
                        table.deleteLater()
                        self.clients.remove(client)
                
            
        
    # add and remove clients based on backend tracking
    def client_update(self,clients):
        change = False
        
        if self.client_count < len(clients):
            change = True
            for client in clients:
                if client not in self.clients:
                    self.add_client(client)
                    self.client_count += 1


                    
        elif self.client_count > len(clients):
            change = True
            for client in self.clients:
                if client not in clients:
                    #self.removeAllClients()
                    self.remove_client(client)
                    self.client_count -=1

        if(change):
            self.v3.addStretch()
        self.v3_label.setText(f'Connected Clients: {self.client_count}')

    
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
        self.setGeometry(screen_size.width()/2-w/2,screen_size.height()/2-h/2,w,h) 
        
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
       
       
        #script i/o set
        menubar = self.menuBar()
        fileMenu = menubar.addMenu('File')
        
        for option in menu_options:
           self.o = QAction(option, self)
           self.o.triggered.connect(self.menu_handler)
           fileMenu.addAction(self.o)
       
        
        # v1
        
        self.v1 = QVBoxLayout()
        self.v1_label = QLabel("Tools")

        
        for button in v1_buttons:
           self.b = QPushButton(self)
           self.b.setText(button)
           self.b.clicked.connect(self.button_handler)
           self.v1.addWidget(self.b)
           
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
        
        
        # maintains shape of v3
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


        

        
    

def init_ui():
    app = QApplication(sys.argv)
    qdarktheme.setup_theme()
    app.setStyle('Fusion')
    gui = CtrlClientGUI()
    cc.get_gui_instance(gui)
    gui.show()
    app.exec_()

init_ui()
