#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import sys
import sip
from util import *
from pyqt_proxy import proxy
from RenderWidget import RenderWidget

class MainWindow(QMainWindow):
	def __init__(self):
		super(MainWindow, self).__init__()
		self.setup_ui()
		self.init_ui()
		set_win_center(self)

	def setup_ui(self):
		self.resize(1920, 1080)
		self.setAnimated(True)

		# self.setStyleSheet("background:#f00")
		self.menu_bar = self.menuBar()
		file_menu = self.menu_bar.addMenu("File")
		new_button = QAction("New Session", self)
		new_button.setShortcut("Ctrl+N")
		file_menu.addAction(new_button)
		quit_button = QAction("Quit", self)
		quit_button.setShortcut("Q")
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

		self.algrithmAction = QActionGroup(self);
		self.algrithmAction.addAction(self.d_a_adapt);
		self.algrithmAction.addAction(self.d_a_stretch);
		self.algrithmAction.addAction(self.d_a_match);
		self.algrithmAction.setExclusive(True);

		display_menu.triggered[QAction].connect(self.processTrigger)

		debug_menu = self.menu_bar.addMenu("Debug")
		self.runing_info = QAction("Runing Info", self)
		self.runing_info.setCheckable(True)
		debug_menu.addAction(self.runing_info)
		debug_menu.triggered[QAction].connect(self.processTrigger)

		self.centralwidget = RenderWidget()
		# self.centralwidget.setStyleSheet("background:#0f0")
		self.setCentralWidget(self.centralwidget)

		self.statusBar = QStatusBar()
		self.setStatusBar(self.statusBar)
		self.statusBar.showMessage(" 菜单选项被点击了", 5000)
		self.statusBar.setVisible(False)

	def init_ui(self):
		self.runing_info.setChecked(False)
		self.d_a_adapt.setChecked(True)

	def _resize_to_1_1(self):
		render_size = self.centralwidget.get_stream_size()
		if self.statusBar.isVisible():
			self.setFixedSize(render_size[0], render_size[1] + self.menu_bar.height() + self.statusBar.height())
		else:
			self.setFixedSize(render_size[0], render_size[1] + self.menu_bar.height())

	def processTrigger(self, q):
		if q.text() == "Runing Info":
			if q.isChecked():
				self.statusBar.setVisible(True)
			else:
				self.statusBar.setVisible(False)
		elif q.text() == "1:1":
			self._resize_to_1_1()