#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
from util import *
from ctypes import *
from pyqt_proxy import *

class RenderWidget(QWidget):
	def __init__(self):
		super(RenderWidget, self).__init__()
		self._setup_ui()
		set_win_center(self)
		add_task(1, True, self._init_client)
		proxy().py_client_regist_stream_size_cb(py_object(self), self._stream_size_cb)
		self.stream_width = 0
		self.stream_height = 0

	def _setup_ui(self):
		self.setMouseTracking(True)
		self.setFocusPolicy(Qt.StrongFocus)
		self.mouse_key = 0

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _stream_size_cb(self, width, height):
		self.stream_width = width
		self.stream_height = height

	def _frame_loop(self, task):
		if proxy().py_on_frame() == -1:
			sys.exit(-1);

	def _init_client(self, task):
		if(proxy().py_client_init_config(self.winId().__int__()) == 0):
			task["loop"] = False;
			add_task(1, True, self._frame_loop)

	def _get_remote_pos(self, x , y):
		x = x / self.width() * self.stream_width
		y = y / self.height() * self.stream_height
		return int(x), int(y)

	def keyPressEvent(self, event):
		keycode = get_key_code(event.key())
		proxy().py_key_event(keycode, 1)

	def keyReleaseEvent(self, event):
		keycode = get_key_code(event.key())
		proxy().py_key_event(keycode, 0)

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

		proxy().py_mouse_click(*self._get_remote_pos(event.x(), event.y()),
			self.mouse_key, 1)

	def mouseMoveEvent(self,event):
		proxy().py_mouse_move(*self._get_remote_pos(event.x(), event.y()))

	def mouseReleaseEvent(self,event):
		proxy().py_mouse_click(*self._get_remote_pos(event.x(), event.y()),
			self.mouse_key, 0)
		self.mouse_key = 0

	def resizeEvent(self, event):
		proxy().py_client_resize(event.size().width(), int(event.size().height()))

	def get_stream_size(self):
		return int(self.stream_width), int(self.stream_height)