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
from RenderWidget import RenderWidget

class MainWindow(QMainWindow):
	def __init__(self):
		super(MainWindow, self).__init__()
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

		display_menu = self.menu_bar.addMenu("Display")

		scale_button = display_menu.addMenu("Scale")
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

		framerate_button = display_menu.addMenu("Taget Frame Rate")
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

		bitrate_button = display_menu.addMenu("Target Bit Rate")
		self.b_1M = QAction("1Mbps", self)
		self.b_1M.setCheckable(True)
		bitrate_button.addAction(self.b_1M)
		self.b_10M = QAction("10Mbps", self)
		self.b_10M.setCheckable(True)
		bitrate_button.addAction(self.b_10M)
		self.b_100M = QAction("100Mbps", self)
		self.b_100M.setCheckable(True)
		bitrate_button.addAction(self.b_100M)
		self.b_1000M = QAction("1000Mbps", self)
		self.b_1000M.setCheckable(True)
		bitrate_button.addAction(self.b_1000M)
		target_bit_group = QActionGroup(self);
		target_bit_group.addAction(self.b_1M);
		target_bit_group.addAction(self.b_10M);
		target_bit_group.addAction(self.b_100M);
		target_bit_group.addAction(self.b_1000M);
		target_bit_group.setExclusive(True);

		display_menu.triggered[QAction].connect(self.processTrigger)

		debug_menu = self.menu_bar.addMenu("Debug")
		self.status_bar_action = QAction("Status Bar", self)
		self.status_bar_action.setCheckable(True)
		debug_menu.addAction(self.status_bar_action)
		debug_menu.triggered[QAction].connect(self.processTrigger)

		self.centralwidget = RenderWidget(self._on_render_show)
		# self.centralwidget.setStyleSheet("background:#0f0")
		self.setCentralWidget(self.centralwidget)

		self.statusBar = QStatusBar()
		self.setStatusBar(self.statusBar)

	def init_ui(self):
		self.setWindowTitle("High Performance Remote Desktop")
		self.status_bar_action.setChecked(True)

		self.d_a_adapt.setChecked(True)
		self.dsp_mode = 1

		self.b_10M.setChecked(True)
		proxy().py_get_frame_rate(py_object(self), self._on_fr_cb)

		self.time_ms = int(round(time.time() * 1000))

		add_task(1, 20, self._loop_20)

	def _loop_20(self, task):
		time_ms = int(round(time.time() * 1000))
		time_sub = time_ms - self.time_ms
		if(time_sub == 0):
			time_sub = 1
		if self.statusBar.isVisible():
			self.statusBar.showMessage("渲染帧率:%d  码流帧率:%d  码率:%s" %
			(20 * 1000 / time_sub,
			proxy().py_get_and_clean_frame() * 1000 / time_sub,
			format_speed(proxy().py_get_and_clean_recv_sum() * 1000 / time_sub)), 0)
		self.time_ms = time_ms

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
		if(self.dsp_mode == 1):
			return

		if(self.dsp_mode == 2):
			return

		if(self.dsp_mode == 3):
			self._resize_to_1_1()
			return

	def _set_dsp_mode(self, mode):
		self.dsp_mode = mode
		self._update_dsp_mode()

	def _reset_fr(self, task, value):
		timer_set_interval(1000 / value)

	@CFUNCTYPE(None, py_object, c_uint, c_uint)
	def _on_fr_cb(self, ret, value):
		print("frame rate setting:%s value:%u"%("success" if ret == 1 else "fail", value))
		add_task(1, False, self._reset_fr, value + 2);
		if hasattr(self, f'fps_{ value + 2 }'):
			getattr(self, f'fps_{ value + 2 }').setChecked(True)

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

		elif q.text() == "Quit":
			sys.exit(0)
