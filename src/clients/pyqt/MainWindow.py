#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
from util import *
from pyqt_proxy import proxy

class MainWindow(QMainWindow):
	def __init__(self):
		global proxy
		super(MainWindow, self).__init__()
		self.setup_ui()
		set_win_center(self)

	def setup_ui(self):
		self.resize(1920, 1080)
		self.setAnimated(True)
		self.setMouseTracking(True)
		self.mouse_key = 0

		# self.setStyleSheet("background:#f00")
		menu_bar = self.menuBar()
		file_menu = menu_bar.addMenu("File")
		new_button = QAction("New Session", self)
		new_button.setShortcut("Ctrl+N")
		file_menu.addAction(new_button)
		quit_button = QAction("Quit", self)
		quit_button.setShortcut("Q")
		file_menu.addAction(quit_button)
		file_menu.triggered[QAction].connect(self.processTrigger)

		display_menu = menu_bar.addMenu("Display")
		scale_button = display_menu.addMenu("Scale")
		scale_button.addAction("adapt")
		scale_button.addAction("stretch")
		display_menu.triggered[QAction].connect(self.processTrigger)

		debug_menu = menu_bar.addMenu("Debug")
		runing_info = QAction("Runing Info", self)
		runing_info.setCheckable(True)
		runing_info.setChecked(False)
		debug_menu.addAction(runing_info)
		debug_menu.triggered[QAction].connect(self.processTrigger)

		self.centralwidget = QWidget()
		# self.centralwidget.setStyleSheet("background:#0f0")
		self.setCentralWidget(self.centralwidget)

		self.statusBar = QStatusBar()
		self.setStatusBar(self.statusBar)
		self.statusBar.showMessage(" 菜单选项被点击了", 5000)
		self.statusBar.setVisible(False)
		
	def processTrigger(self, q):
		if q.text() == "Runing Info":
			if q.isChecked():
				self.statusBar.setVisible(True)
			else:
				self.statusBar.setVisible(False)


	def frame_loop(self, task):
		if proxy().py_on_frame() == -1:
			sys.exit(-1);

	def init_client(self, task):
		if(proxy().py_client_init_config(self.centralwidget.winId().__int__()) == 0):
			task["loop"] = False;
			add_task(1, True, self.frame_loop)

	def mousePressEvent(self,event):
		if self.mouse_key != 0:
			return

		self.mouse_key = 1
		if event.buttons() == Qt.LeftButton:
			self.mouse_key = 1
		if event.buttons() == Qt.RightButton:
			self.mouse_key = 3
		if event.buttons() == Qt.MidButton:
			self.mouse_key = 2

		proxy().py_mouse_click(event.x(), event.y(), self.mouse_key, 1)

	def mouseMoveEvent(self,event):
		proxy().py_mouse_move(event.x(), event.y())

	def mouseReleaseEvent(self,event):
		proxy().py_mouse_click(event.x(), event.y(), self.mouse_key, 0)
		self.mouse_key = 0
