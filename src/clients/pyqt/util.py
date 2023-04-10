#!/usr/bin/python3
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

tasks = []
def on_timer():
	global tasks
	for i in range(len(tasks) - 1, -1, -1):
		task = tasks[i]
		if task["_interval"] == 0:
			task["callback"](*task["params"])
			if task["interval"] == 0:
				tasks.remove(task)
			else:
				task["_interval"] = task["interval"]

		if task["_interval"] > 0:
			task["_interval"] = task["_interval"] - 1

timer = None
interval = 1
def start_timer(app):
	global timer, interval
	interval = int(1000 / 60)
	timer = QTimer(app)
	timer.timeout.connect(on_timer)
	timer.start(interval)

def timer_set_interval(p_interval):
	global timer, interval
	interval = p_interval
	timer.setInterval(interval)

def timer_get_interval():
	global interval
	return interval
"""
delay: delay frames to start
interval: loop interval，0 is not loop
callback: callback func
*params: callback params
"""
def add_task(delay, interval, callback, *params):
	global tasks
	task = {}
	task["_interval"] = delay
	task["interval"] = interval
	task["callback"] = callback
	task["params"] = (task, *params)
	tasks.append(task)

def stop_task(task):
	task["interval"] = 0

def set_win_center(ui):
	main_screen = QApplication.primaryScreen().geometry()
	# screen = QDesktopWidget().screenGeometry(0)
	size = ui.geometry()
	newLeft = main_screen.x() + (main_screen.width() - size.width()) / 2
	newTop = main_screen.y() + (main_screen.height() - size.height()) / 2
	ui.move(int(newLeft),int(newTop))

def get_key_code(code):
	if code <= 222:
		return code

	if code == Qt.Key_Backspace:
		return 8

	if code == Qt.Key_Tab:
		return 9

	if code == Qt.Key_Return:
		return 13

	if code == Qt.Key_Enter:
		return 108

	if code == Qt.Key_Shift:
		return 16

	if code == Qt.Key_Control:
		return 17

	if code == Qt.Key_Alt:
		return 18

	if code == Qt.Key_CapsLock:
		return 20

	if code == Qt.Key_Escape:
		return 27

	if code == Qt.Key_Space:
		return 32

	if code == Qt.Key_PageUp:
		return 33

	if code == Qt.Key_PageDown:
		return 34

	if code == Qt.Key_End:
		return 35

	if code == Qt.Key_Home:
		return 36

	if code == Qt.Key_Left:
		return 37

	if code == Qt.Key_Up:
		return 38

	if code == Qt.Key_Right:
		return 39

	if code == Qt.Key_Down:
		return 40

	return 17

def format_speed(speed):
	if speed < 1024:
		return "%db/s" % (speed)
	if speed < 1024 * 1024:
		return "%dkb/s" % (speed / 1024)
	if speed < 1024 * 1024 * 1024:
		return "%dMb/s" % (speed / 1024 / 1024)
	if speed < 1024 * 1024 * 1024 * 1204:
		return "%dGb/s" % (speed / 1024 / 1024 / 1024)
	if speed < 1024 * 1024 * 1024 * 1024 * 1024:
		return "%dTb/s" % (speed / 1024 / 1024 / 1024 / 1024)

	return "out of range"
