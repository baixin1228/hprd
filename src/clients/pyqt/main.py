#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
import argparse
import configparser

from ctypes import *
from platform import *

cdll_names = {
            'Darwin' : 'libc.dylib',
            'Linux'  : '../../../client_build/src/clients/pyqt/libpyqt_proxy.so',
            'Windows': 'msvcrt.dll'
        }

clib = cdll.LoadLibrary(cdll_names[system()])
clib.py_client_connect.argtypes = [POINTER(c_char), c_ushort]
clib.py_client_connect.restype = c_int

clib.py_on_frame.argtypes = []
clib.py_on_frame.restype = c_int

clib.py_client_init_config.argtypes = []
clib.py_client_init_config.restype = c_int

clib.py_client_close.argtypes = []
clib.py_client_close.restype = c_int

tasks = []
def _on_timer():
	global tasks
	for i in range(len(tasks) - 1, -1, -1):
		task = tasks[i]
		if task["delay"] == 0:
			task["callback"](*task["params"])
			if not task["loop"]:
				tasks.remove(task)

		if task["delay"] > 0:
			task["delay"] = task["delay"] - 1

def addTask(delay, loop, callback, *params):
	global tasks
	task = {}
	task["delay"] = delay
	task["loop"] = loop
	task["callback"] = callback
	task["params"] = (task, *params)
	tasks.append(task)

def set_win_center(ui):
    main_screen = QApplication.primaryScreen().geometry()
    # screen = QDesktopWidget().screenGeometry(0)
    size = ui.geometry()
    newLeft = main_screen.x() + (main_screen.width() - size.width()) / 2
    newTop = main_screen.y() + (main_screen.height() - size.height()) / 2
    ui.move(int(newLeft),int(newTop))

class RenderWindow(QMainWindow):
	def __init__(self, parent=None):
		global clib
		super(RenderWindow, self).__init__(parent)
		self.setupUi()
		set_win_center(self)

	def setupUi(self):
		self.resize(1920, 1080)
		self.setAnimated(True)

	def frame_loop(self, task):
		if clib.py_on_frame() == -1:
			sys.exit(-1);

	def init_client(self, task):
		print(self.winId())
		print(sip.voidptr(self.winId()))
		print(hex(self.winId().__int__()))
		if(clib.py_client_init_config(self.winId().__int__()) == 0):
			task["loop"] = False;
			addTask(1, True, self.frame_loop)

def recv_pkt(buf, len):
	pass
	# print(len)

class LoginWindow(QMainWindow):
	def __init__(self):
		super(LoginWindow, self).__init__(None)
		self.initArg();
		self.setupUi()
		self.initUi()
		set_win_center(self)

	def initArg(self):
		# parser = argparse.ArgumentParser(description='hprd')
		parser = argparse.ArgumentParser()
		parser.add_argument('-a', '--ip', dest='ip', type=str, metavar='', required=True, help='Remote server ip addr')

		args = parser.parse_args()
		self.ip = args.ip

	def setupUi(self):
		self.resize(720, 400)
		self.setAnimated(True)
		self.centralwidget = QWidget(self)
		self.centralwidget.setObjectName("centralwidget")
		self.layout = QGridLayout()
		self.centralwidget.setLayout(self.layout)
		self.setCentralWidget(self.centralwidget)
		
		self.ip_lable = QLabel()
		self.ip_lable.setText("IP:")
		self.layout.addWidget(self.ip_lable,0,0,1,1, Qt.AlignRight)
		self.ip_edit = QLineEdit()
		self.ip_edit.setText(self.ip or "127.0.0.1:9999")
		self.layout.addWidget(self.ip_edit,0,1,1,3)

		self.user_lable = QLabel()
		self.user_lable.setText("Name:")
		self.layout.addWidget(self.user_lable,1,0,1,1, Qt.AlignRight)
		self.user_edit = QLineEdit()
		self.user_edit.setText("")
		self.layout.addWidget(self.user_edit,1,1,1,3)

		self.passwd_lable = QLabel()
		self.passwd_lable.setText("Password:")
		self.layout.addWidget(self.passwd_lable,2,0,1,1, Qt.AlignRight)
		self.passwd_edit = QLineEdit()
		self.passwd_edit.setText("")
		self.passwd_edit.setEchoMode(QLineEdit.Password)
		self.layout.addWidget(self.passwd_edit,2,1,1,3)

		self.conn_button = QPushButton()
		self.conn_button.setText("连接")
		self.layout.addWidget(self.conn_button,3,1,1,2)

		self.render_win = RenderWindow()
 
	def initUi(self):
		self.ip_edit.setFocus()
		self.ip_edit.setPlaceholderText("请输入ip地址")
		self.user_edit.setPlaceholderText("请输入用户名")
		self.passwd_edit.setPlaceholderText("请输入密码")

		self.conn_button.clicked.connect(self.on_connect)  # 登录

	def render_win_show(self, task):
		self.render_win.show()
		self.close()

	def on_connect(self):
		global clib
		port = 0
		ip_port = self.ip_edit.text().split(":")

		if len(ip_port) == 2:
			port = int(ip_port[1])
		else:
			port = 9999

		ret = clib.py_client_connect(ip_port[0].encode('utf-8'), port)
		if ret == 0:
			_cb = CFUNCTYPE(None, POINTER(c_char), c_ulong)
			self.cb = _cb(recv_pkt)
			self.buf = create_string_buffer(1024 * 1024);
			addTask(1, False, self.render_win_show)
			addTask(2, True, self.render_win.init_client)

if __name__=="__main__":
	app = QApplication(sys.argv)
	w = LoginWindow()
	w.show()

	timer = QTimer(app)
	timer.timeout.connect(_on_timer)
	timer.start(int(1000 / 60))

	sys.exit(app.exec())