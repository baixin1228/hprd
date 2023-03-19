#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
from util import *
from pyqt_proxy import proxy

class RenderWidget(QWidget):
	def __init__(self):
		super(RenderWidget, self).__init__()
		self.setup_ui()
		set_win_center(self)
		add_task(1, True, self.init_client)

	def setup_ui(self):
		# self.resize(1920, 1080)
		# self.setAnimated(True)
		self.setMouseTracking(True)
		self.mouse_key = 0

	def frame_loop(self, task):
		if proxy().py_on_frame() == -1:
			sys.exit(-1);

	def init_client(self, task):
		if(proxy().py_client_init_config(self.winId().__int__()) == 0):
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
