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
	def __init__(self, on_render_show):
		super(RenderWidget, self).__init__()
		self.on_render_show = on_render_show
		self._setup_ui()
		set_win_center(self)
		add_task(1, True, self._waiting_init)
		proxy().py_client_regist_stream_size_cb(py_object(self), self._stream_size_cb)
		self.stream_width = 0
		self.stream_height = 0
		self.old_width = self.width()
		self.old_height = self.height()

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

	def _waiting_init(self, task):
		ret = proxy().py_client_init_config(self.winId().__int__())
		if ret == 0:
			task["loop"] = False;
			add_task(1, True, self._frame_loop)
			if self.on_render_show:
				self.on_render_show()
		if ret == -1:
			print("py_client_init_config fail.")
			sys.exit(-1)

	def _get_remote_pos(self, x , y):
		x = x / self.width() * self.stream_width
		y = y / self.height() * self.stream_height
		return int(x), int(y)

	def wheelEvent(self, event):
		angle = event.angleDelta()
		if angle.y() > 0:
			proxy().py_wheel_event(4)

		if angle.y() < 0:
			proxy().py_wheel_event(5)

		if angle.x() < 0:
			proxy().py_wheel_event(6)

		if angle.x() > 0:
			proxy().py_wheel_event(7)

		event.accept()

	def keyPressEvent(self, event):
		print(event.key())
		keycode = get_key_code(event.key())
		proxy().py_key_event(keycode, 1)
		event.accept()

	def keyReleaseEvent(self, event):
		keycode = get_key_code(event.key())
		proxy().py_key_event(keycode, 0)
		event.accept()

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
		event.accept()

	def mouseMoveEvent(self,event):
		proxy().py_mouse_move(*self._get_remote_pos(event.x(), event.y()))
		event.accept()

	def mouseReleaseEvent(self,event):
		proxy().py_mouse_click(*self._get_remote_pos(event.x(), event.y()),
			self.mouse_key, 0)
		self.mouse_key = 0
		event.accept()

	def update_size(self):
		if self.old_width != self.width() or self.old_height != self.height():
			print("resize", self.width(), self.height())
			self.old_width = self.width()
			self.old_height = self.height()
			proxy().py_client_resize(self.width(), int(self.height()))

	def resizeEvent(self, event):
		print("resizeEvent")
		self.update_size()
		event.accept()

	def get_stream_size(self):
		return int(self.stream_width), int(self.stream_height)