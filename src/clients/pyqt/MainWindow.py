#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
import time
from util import *
from ctypes import *
from pyqt_proxy import proxy
from PlatformRes import PlatformRes
from RenderWidget import RenderWidget

class MainWindow(QMainWindow):
	def __init__(self, params):
		super(MainWindow, self).__init__()
		self.platform_res = PlatformRes()
		self.enable_kcp = params["kcp"]
		self.ip = params["ip"]
		self.share_clipboard = params["share_clipboard"]
		self.setup_ui()
		self.init_ui()
		set_win_center(self)

	def setup_ui(self):
		self.resize(600, 600)
		self.setAnimated(True)

		# self.setStyleSheet("background:#f00")
		self.menu_bar = self.menuBar()
		file_menu = self.menu_bar.addMenu("File")
		new_button = QAction("New Session", self)
		# new_button.setShortcut("Ctrl+N")
		file_menu.addAction(new_button)
		quit_button = QAction("Quit", self)
		# quit_button.setShortcut("Ctrl+Alt+Q")
		file_menu.addAction(quit_button)
		file_menu.triggered[QAction].connect(self.processTrigger)

		setting_menu = self.menu_bar.addMenu("Setting")

		scale_button = setting_menu.addMenu("Scale")
		self.d_a_adapt = QAction("Adapt", self)
		self.d_a_adapt.setCheckable(True)
		scale_button.addAction(self.d_a_adapt)
		self.d_a_stretch = QAction("Stretch", self)
		self.d_a_stretch.setCheckable(True)
		scale_button.addAction(self.d_a_stretch)
		self.d_a_match = QAction("1:1", self)
		self.d_a_match.setCheckable(True)
		scale_button.addAction(self.d_a_match)
		scale_group = QActionGroup(self);
		scale_group.addAction(self.d_a_adapt);
		scale_group.addAction(self.d_a_stretch);
		scale_group.addAction(self.d_a_match);
		scale_group.setExclusive(True);

		framerate_button = setting_menu.addMenu("Taget Frame Rate")
		self.fps_30 = QAction("30fps", self)
		self.fps_30.setCheckable(True)
		framerate_button.addAction(self.fps_30)
		self.fps_60 = QAction("60fps", self)
		self.fps_60.setCheckable(True)
		framerate_button.addAction(self.fps_60)
		self.fps_120 = QAction("120fps", self)
		self.fps_120.setCheckable(True)
		framerate_button.addAction(self.fps_120)
		self.fps_240 = QAction("240fps", self)
		self.fps_240.setCheckable(True)
		framerate_button.addAction(self.fps_240)
		frame_rate_group = QActionGroup(self);
		frame_rate_group.addAction(self.fps_30);
		frame_rate_group.addAction(self.fps_60);
		frame_rate_group.addAction(self.fps_120);
		frame_rate_group.addAction(self.fps_240);
		frame_rate_group.setExclusive(True);

		bitrate_button = setting_menu.addMenu("Target Bit Rate")
		self.b_1M = QAction("1Mbps", self)
		self.b_1M.setCheckable(True)
		bitrate_button.addAction(self.b_1M)
		self.b_3M = QAction("3Mbps", self)
		self.b_3M.setCheckable(True)
		bitrate_button.addAction(self.b_3M)
		self.b_10M = QAction("10Mbps", self)
		self.b_10M.setCheckable(True)
		bitrate_button.addAction(self.b_10M)
		self.b_30M = QAction("30Mbps", self)
		self.b_30M.setCheckable(True)
		bitrate_button.addAction(self.b_30M)
		target_bit_group = QActionGroup(self);
		target_bit_group.addAction(self.b_1M);
		target_bit_group.addAction(self.b_3M);
		target_bit_group.addAction(self.b_10M);
		target_bit_group.addAction(self.b_30M);
		target_bit_group.setExclusive(True);

		self.share_clipboard_action = QAction("Share ClipBoard", self)
		self.share_clipboard_action.setCheckable(True)
		self.share_clipboard_action.setChecked(False)
		setting_menu.addAction(self.share_clipboard_action)

		setting_menu.triggered[QAction].connect(self.processTrigger)

		debug_menu = self.menu_bar.addMenu("Debug")
		self.status_bar_action = QAction("Status Bar", self)
		self.status_bar_action.setCheckable(True)
		debug_menu.addAction(self.status_bar_action)
		debug_menu.triggered[QAction].connect(self.processTrigger)

		self.centralwidget = RenderWidget(self)
		# self.centralwidget.setStyleSheet("background:#0f0")
		self.setCentralWidget(self.centralwidget)

		self.statusBar = QStatusBar()
		fixedFont = QFontDatabase.systemFont(QFontDatabase.FixedFont)
		self.statusBar.setFont(fixedFont)
		self.setStatusBar(self.statusBar)

	def init_ui(self):
		self.setWindowTitle("High Performance Remote Desktop - %s" % self.ip)
		icon = QIcon()
		qpix = QPixmap(self.platform_res.get_icon_path())
		icon.addPixmap(qpix, QIcon.Normal, QIcon.Off)
		self.setWindowIcon(icon)

		self.status_bar_action.setChecked(True)

		self.d_a_adapt.setChecked(True)
		self.start_time_ms = int(round(time.time() * 1000))
		self.ping = 0
		self.dsp_mode = 1
		self.remote_fps = 0
		self.render_fps = 30
		self.interval = int(1000 / self.render_fps)
		self.render_speed = "x1"

		proxy().py_get_bit_rate(py_object(self), self._on_bit_cb)
		proxy().py_get_frame_rate(py_object(self), self._on_fr_cb)
		if self.enable_kcp:
			proxy().py_get_client_id(py_object(self), self._on_client_id)

		if self.share_clipboard:
			proxy().py_set_share_clipboard(py_object(self), 1, self._on_share_clipboard)

		self.time_ms = 0

		add_task(1, 30, self._loop_30)
		add_task(1, 1, self._render_monitor)

	def _loop_30(self, task):
		time_ms = int(round(time.time() * 1000))
		time_sub = time_ms - self.time_ms
		if(time_sub == 0):
			time_sub = 1

		if self.statusBar.isVisible():
			if self.time_ms == 0:
				frame_rete = 0
			else:
				frame_rete = int(30 * 1000 / time_sub)

			stream_frame_rate = int(proxy().py_get_and_clean_frame() * 1000 / time_sub)
			bit_rate = format_speed(proxy().py_get_recv_count() * 1000 / time_sub)
			ping = "%dms"%self.ping

			self.statusBar.showMessage("渲染帧率:%-3d  码流帧率:%-3d  服务端帧率:%-3d 码率:%-8s 渲染速度:%-4s  缓冲区长度:%-3d ping:%-6s kcp:%s" % (frame_rete, stream_frame_rate, self.remote_fps, bit_rate,
				self.render_speed, proxy().py_get_queue_len(), ping,
				"on" if proxy().py_kcp_active() else "off"), 0)
			proxy().py_get_remote_fps(py_object(self), self._on_fps_cb)
			proxy().py_ping(py_object(self), int(round(time.time() * 1000)) - self.start_time_ms, self._on_ping)

		self.time_ms = time_ms

	def _render_monitor(self, task):
		if proxy().py_get_queue_len() > self.render_fps and timer_get_interval() == self.interval:
			self.render_speed = "x4"
			interval = int(self.interval / 4)
			add_task(1, False, self._reset_interval, interval if interval > 0 else 1)
			return

		if proxy().py_get_queue_len() > self.render_fps / 5 and timer_get_interval() == self.interval:
			self.render_speed = "x2"
			interval = int(self.interval / 2)
			add_task(1, False, self._reset_interval, interval if interval > 0 else 1)
			return

		if proxy().py_get_queue_len() < self.render_fps / 5 and timer_get_interval() != self.interval:
			self.render_speed = "x1"
			add_task(1, False, self._reset_interval, self.interval)

	def _on_render_show(self):
		render_size = self.centralwidget.get_stream_size()
		if self.statusBar.isVisible():
			self.resize(render_size[0], render_size[1] + self.menu_bar.height() + self.statusBar.height())
		else:
			self.resize(render_size[0], render_size[1] + self.menu_bar.height())

	def _resize_to_1_1(self):
		render_size = self.centralwidget.get_stream_size()
		if self.statusBar.isVisible():
			self.setFixedSize(render_size[0], render_size[1] + self.menu_bar.height() + self.statusBar.height())
		else:
			self.setFixedSize(render_size[0], render_size[1] + self.menu_bar.height())

	def _update_dsp_mode(self):
		self.centralwidget.set_scale_mode(self.dsp_mode)
		if self.dsp_mode == 1 or self.dsp_mode == 2:
			self.setMinimumSize(0,0);
			self.setMaximumSize(QSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX));
			""" 触发渲染控件重新布局 """
			self.centralwidget.update_size()
			return

		if self.dsp_mode == 3:
			self._resize_to_1_1()
			return

	def _set_dsp_mode(self, mode):
		self.dsp_mode = mode
		self._update_dsp_mode()

	def _reset_interval(self, task, value):
		timer_set_interval(value)

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_ping(self, ret, value):
		if ret == 1:
			time_ms = int(round(time.time() * 1000)) - self.start_time_ms
			self.ping = time_ms - value

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_fps_cb(self, ret, value):
		if ret == 1:
			self.remote_fps = value

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_share_clipboard(self, ret, value):
		print("_on_share_clipboard", ret, hex(value))
		if ret == 1 and value == 1:
			self.share_clipboard_action.setChecked(True)
			self.centralwidget.set_share_clipboard(True)
		else:
			self.share_clipboard_action.setChecked(False)
			self.centralwidget.set_share_clipboard(False)

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_fr_cb(self, ret, value):
		print("frame rate ret:%s value:%u"%("success" if ret == 1 else "fail", value))
		if ret == 1:
			self.render_fps = value + 2
			self.interval = int(1000 / self.render_fps)
			add_task(1, False, self._reset_interval, self.interval);
			if hasattr(self, f'fps_{ self.render_fps }'):
				getattr(self, f'fps_{ self.render_fps }').setChecked(True)

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_bit_cb(self, ret, value):
		value = int(value / 1024 / 1024)
		print("bit rate ret:%s value:%u"%("success" if ret == 1 else "fail", value))
		if ret == 1:
			if hasattr(self, f'b_{ value }M'):
				getattr(self, f'b_{ value }M').setChecked(True)

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_client_id(self, ret, value):
		print("client id ret:%s value:%u"%("success" if ret == 1 else "fail", 
			value))
		if ret == 1:
			self.kcp_client_id = value
			proxy().py_kcp_connect(value)

	def get_fps(self):
		return self.render_fps
		
	def processTrigger(self, q):
		if q.text() == "Status Bar":
			if q.isChecked():
				self.statusBar.setVisible(True)
				self._update_dsp_mode()
			else:
				self.statusBar.setVisible(False)
				self._update_dsp_mode()

		elif q.text() == "Adapt":
			self._set_dsp_mode(1)
		elif q.text() == "Stretch":
			self._set_dsp_mode(2)
		elif q.text() == "1:1":
			self._set_dsp_mode(3)

		elif q.text() == "30fps":
			proxy().py_set_frame_rate(py_object(self), 28, self._on_fr_cb)
		elif q.text() == "60fps":
			proxy().py_set_frame_rate(py_object(self), 58, self._on_fr_cb)
		elif q.text() == "120fps":
			proxy().py_set_frame_rate(py_object(self), 118, self._on_fr_cb)
		elif q.text() == "240fps":
			proxy().py_set_frame_rate(py_object(self), 238, self._on_fr_cb)

		elif q.text() == "1Mbps":
			proxy().py_set_bit_rate(py_object(self), 1 * 1024 * 1024, self._on_bit_cb)
		elif q.text() == "3Mbps":
			proxy().py_set_bit_rate(py_object(self), 3 * 1024 * 1024, self._on_bit_cb)
		elif q.text() == "10Mbps":
			proxy().py_set_bit_rate(py_object(self), 10 * 1024 * 1024, self._on_bit_cb)
		elif q.text() == "30Mbps":
			proxy().py_set_bit_rate(py_object(self), 30 * 1024 * 1024, self._on_bit_cb)
		elif q.text() == "Share ClipBoard":
			if self.share_clipboard_action.isChecked():
				value = 1
			else:
				value = 0

			proxy().py_set_share_clipboard(py_object(self), value, self._on_share_clipboard)

		elif q.text() == "Quit":
			sys.exit(0)