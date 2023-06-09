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
		self.last_time = 0
		self.x_offset = 0
		self.y_offset = 0
		self.x_scale = 1
		self.y_scale = 1
		self.x_adapt_scale = 1
		self.y_adapt_scale = 1

	def _setup_ui(self):
		self.setMouseTracking(True)
		self.setFocusPolicy(Qt.StrongFocus)
		self.mouse_key = 0
		self.installEventFilter(self)
		self.modifiers = 0
		self.angle_y = 0
		self.angle_x = 0
		self.grab = False
		self.enable_clip = False
		self.text_clip = ""
		self.html_clip = ""
		self.image_clip = 0
		self.scale_mode = 1

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _stream_size_cb(self, width, height):
		self.stream_width = width
		self.stream_height = height
		""" 触发父window resize """
		if self.main_window._on_render_show:
			self.main_window._on_render_show()

	def _frame_loop(self, task):
		if proxy().py_on_frame() == -1:
			print("py_on_frame fail.")
			sys.exit(-1);

	def _waiting_init(self, task):
		""" 初始化显示 """
		ret = proxy().py_client_init_config(self.winId().__int__())
		if ret == 0:
			stop_task(task)
			add_task(1, 1, self._frame_loop)

		if ret == -1:
			print("py_client_init_config fail.")
			sys.exit(-1)

	def _get_remote_pos(self, x , y):
		x = (self.x_offset + x) / self.x_adapt_scale
		y = (self.y_offset + y) / self.y_adapt_scale
		return int(x), int(y)

	def wheelEvent(self, event):
		if self.grab == True:
			angle = event.angleDelta()
			self.angle_y += angle.y()
			self.angle_x += angle.x()

			if self.angle_y >= 120:
				proxy().py_wheel_event(4)
				self.angle_y = 0

			if self.angle_y <= -120:
				proxy().py_wheel_event(5)
				self.angle_y = 0

			if self.angle_x <= -120:
				proxy().py_wheel_event(6)
				self.angle_x = 0

			if self.angle_x >= 120:
				proxy().py_wheel_event(7)
				self.angle_x = 0

	def keyPressEvent(self, event):
		if self.grab == True:
			self.modifiers = event.modifiers()
			if self.modifiers & Qt.AltModifier and self.modifiers & Qt.ControlModifier and event.key() == Qt.Key_G:
				proxy().py_key_event(Qt.Key_Control, 0)
				proxy().py_key_event(Qt.Key_Alt, 0)
				self._ui_release_grab()
				return
			keycode = get_key_code(event.key())
			proxy().py_key_event(keycode, 1)

	def keyReleaseEvent(self, event):
		if self.grab == True:
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

		x, y = self._get_remote_pos(event.x(), event.y())
		if x >= 0 and y >= 0 and x <= self.stream_width and y <= self.stream_height:
			proxy().py_mouse_click(x, y, self.mouse_key, 1)

	def mouseMoveEvent(self,event):
		time_ms = int(round(time.time() * 1000))
		self._ui_grab()
		if time_ms - self.last_time > 1000 / self.main_window.get_fps():
			x, y = self._get_remote_pos(event.x(), event.y())
			if x >= 0 and y >= 0 and x <= self.stream_width and y <= self.stream_height:
				proxy().py_mouse_move(x, y)
				self.last_time = time_ms

	def mouseReleaseEvent(self,event):
		proxy().py_mouse_click(*self._get_remote_pos(event.x(), event.y()),
			self.mouse_key, 0)
		self.mouse_key = 0

	def update_size(self):
		proxy().py_client_resize(self.width(), int(self.height()))
		if self.stream_width > 0 and self.stream_height > 0:
			self.x_scale = float(self.width()) / self.stream_width
			self.y_scale = float(self.height()) / self.stream_height
			self.x_adapt_scale = self.x_scale
			self.y_adapt_scale = self.y_scale

		if self.scale_mode == 1 and self.stream_width > 0 and self.stream_height > 0:
			ratio_window = float(self.width()) / self.height()
			ratio_stream = float(self.stream_width) / self.stream_height
			if ratio_window > ratio_stream:
				self.y_offset = 0
				self.x_offset = -int((self.width() - self.stream_width * self.y_scale) / 2)
				proxy().py_client_scale(float(self.stream_width * self.y_scale) / float(self.width()) , 1.0)
				self.x_adapt_scale = self.y_scale
			else:
				self.x_offset = 0
				self.y_offset = -int((self.height() - self.stream_height * self.x_scale) / 2)
				proxy().py_client_scale(1.0, float(self.stream_height * self.x_scale) / float(self.height()))
				self.y_adapt_scale = self.x_scale
		else:
			proxy().py_client_scale(1.0, 1.0)
			self.x_offset = 0
			self.y_offset = 0

	def _ui_grab(self):
		if self.grab == False and self.geometry().contains(self.mapFromGlobal(QCursor.pos())):
			self.grabKeyboard()
			self.grab = True

	def _ui_release_grab(self):
		if self.grab == True:
			self.releaseKeyboard()
			self.grab = False

	def enterEvent(self, event):
		self._ui_grab()

	def leaveEvent(self, event):
		self._ui_release_grab()

	def set_share_clipboard(self, value):
		self.enable_clip = value
		
	def _focus_in(self):
		self._ui_grab()

		if not self.enable_clip:
			return
		clipboard = QApplication.clipboard()
		mimeData = clipboard.mimeData()
		if mimeData.hasFormat('text/plain'):
			if self.text_clip != mimeData.text():
				self.text_clip = mimeData.text()
				if self.text_clip != "" and len(self.text_clip) < 10240:
					c_format = 'text/plain'.encode('utf-8')
					c_str = self.text_clip.encode('utf-8')
					c_str_len = len(self.text_clip) + 1
					proxy().py_clip_event(c_format, c_str, c_str_len)
		elif mimeData.hasHtml():
			print(mimeData.formats())
			pass
			# print(mimeData.html())
		elif mimeData.hasFormat('application/x-qt-image'):
			pass
			# print("img")
			# self.showBox.setPixmap(clipboard.pixmap())

	def _focus_out(self):
		if self.grab == True:
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

	def set_scale_mode(self, scale_mode):
		self.scale_mode = scale_mode