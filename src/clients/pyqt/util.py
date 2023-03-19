#!/usr/bin/python3
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

tasks = []
def on_timer():
	global tasks
	for i in range(len(tasks) - 1, -1, -1):
		task = tasks[i]
		if task["delay"] == 0:
			task["callback"](*task["params"])
			if not task["loop"]:
				tasks.remove(task)

		if task["delay"] > 0:
			task["delay"] = task["delay"] - 1

def add_task(delay, loop, callback, *params):
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
