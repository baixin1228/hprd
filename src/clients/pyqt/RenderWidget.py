#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
import time
from util import *
from ctypes import *
from pyqt_proxy import *

class RenderWidget(QWidget):
	def __init__(self, main_window):
		super(RenderWidget, self).__init__()
		self.main_window = main_window
		self._setup_ui()
		set_win_center(self)
		add_task(1, 1, self._waiting_init)
		proxy().py_client_regist_stream_size_cb(py_object(self), self._stream_size_cb)
		self.stream_width = 0
		self.stream_height = 0
		self.old_width = self.width()
		self.old_height = self.height()
		self.last_time = 0

	def _setup_ui(self):
		self.setMouseTracking(True)
		self.setFocusPolicy(Qt.StrongFocus)
		self.mouse_key = 0
		self.installEventFilter(self)
		self.modifiers = 0
		self.angle_key = 0
		self.angle_key_times = 0
		self.grab = False
		self.enable_clip = False
		self.text_clip = 0
		self.html_clip = 0
		self.image_clip = 0

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _stream_size_cb(self, width, height):
		self.stream_width = width
		self.stream_height = height

	def _frame_loop(self, task):
		if proxy().py_on_frame() == -1:
			print("py_on_frame fail.")
			sys.exit(-1);

	def _waiting_init(self, task):
		ret = proxy().py_client_init_config(self.winId().__int__())
		if ret == 0:
			stop_task(task)
			add_task(1, 1, self._frame_loop)
			if self.main_window._on_render_show:
				self.main_window._on_render_show()
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
			if self.angle_key == 4 and self.angle_key_times < 10:
				self.angle_key_times = self.angle_key_times + 1
			else:
				proxy().py_wheel_event(4)
				self.angle_key = 4
				self.angle_key_times = 1

		if angle.y() < 0:
			if self.angle_key == 5 and self.angle_key_times < 10:
				self.angle_key_times = self.angle_key_times + 1
			else:
				proxy().py_wheel_event(5)
				self.angle_key = 5
				self.angle_key_times = 1

		if angle.x() < 0:
			if self.angle_key == 6 and self.angle_key_times < 10:
				self.angle_key_times = self.angle_key_times + 1
			else:
				proxy().py_wheel_event(6)
				self.angle_key = 6
				self.angle_key_times = 1

		if angle.x() > 0:
			if self.angle_key == 7 and self.angle_key_times < 10:
				self.angle_key_times = self.angle_key_times + 1
			else:
				proxy().py_wheel_event(7)
				self.angle_key = 7
				self.angle_key_times = 1

	def keyPressEvent(self, event):
		self.modifiers = event.modifiers()
		if self.modifiers & Qt.AltModifier and self.modifiers & Qt.ControlModifier and event.key() == Qt.Key_G:
			proxy().py_key_event(Qt.Key_Control, 0)
			proxy().py_key_event(Qt.Key_Alt, 0)
			self._ui_release_grab()
			return
		keycode = get_key_code(event.key())
		proxy().py_key_event(keycode, 1)

	def keyReleaseEvent(self, event):
		self.modifiers = event.modifiers()
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
		time_ms = int(round(time.time() * 1000))
		self._ui_grab()
		if time_ms - self.last_time > 1000 / self.main_window.get_fps():
			proxy().py_mouse_move(*self._get_remote_pos(event.x(), event.y()))
			self.last_time = time_ms

	def mouseReleaseEvent(self,event):
		proxy().py_mouse_click(*self._get_remote_pos(event.x(), event.y()),
			self.mouse_key, 0)
		self.mouse_key = 0

	def update_size(self):
		if self.old_width != self.width() or self.old_height != self.height():
			self.old_width = self.width()
			self.old_height = self.height()
			proxy().py_client_resize(self.width(), int(self.height()))

	def _ui_grab(self):
		if self.grab == False and self.geometry().contains(self.mapFromGlobal(QCursor.pos())):
			self.grabKeyboard()
			self.grab = True
			print("grabKeyboard")

	def _ui_release_grab(self):
		if self.grab == True:
			self.releaseKeyboard()
			self.grab = False
			print("releaseKeyboard")

	def enterEvent(self, event):
		self._ui_grab()

	def leaveEvent(self, event):
		self._ui_release_grab()

	def _focus_in(self):
		self._ui_grab()
		clipboard = QApplication.clipboard()
		mimeData = clipboard.mimeData()
		# print(mimeData.formats())
		if mimeData.hasFormat('text/plain'):
			pass
			# print(mimeData.text())
		elif mimeData.hasHtml():
			pass
			# print(mimeData.html())
		elif mimeData.hasFormat('application/x-qt-image'):
			pass
			# print("img")
			# self.showBox.setPixmap(clipboard.pixmap())

	def _focus_out(self):
		if self.modifiers & Qt.ShiftModifier:
			proxy().py_key_event(Qt.Key_Shift, 0)
		if self.modifiers & Qt.ControlModifier:
			proxy().py_key_event(Qt.Key_Control, 0)
		if self.modifiers & Qt.AltModifier:
			proxy().py_key_event(Qt.Key_Alt, 0)
		self._ui_release_grab()

	def eventFilter(self, widget, event):
		if event.type() == QEvent.FocusIn:
			if event.reason() == Qt.ActiveWindowFocusReason:
				self._focus_in()
			return True
		if event.type() == QEvent.FocusOut:
			if event.reason() == Qt.ActiveWindowFocusReason:
				self._focus_out()
			return True

		return False

	def resizeEvent(self, event):
		self.update_size()

	def get_stream_size(self):
		return int(self.stream_width), int(self.stream_height)