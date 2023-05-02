#!/usr/bin/python3
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from pyqt_proxy import proxy

tasklock = proxy().alloc_mutex()
tasks = []
def on_timer():
	global tasks, tasklock
	_rm_list = []
	proxy().mutex_lock(tasklock)
	_tasks = []
	for item in tasks:
		_tasks.append(item)
	proxy().mutex_unlock(tasklock)

	for i in range(len(_tasks) - 1, -1, -1):
		task = _tasks[i]
		if task["_interval"] == 0:
			task["callback"](*task["params"])
			if task["interval"] == 0:
				_rm_list.append(task)
			else:
				task["_interval"] = task["interval"]

		if task["_interval"] > 0:
			task["_interval"] = task["_interval"] - 1

	del _tasks

	proxy().mutex_lock(tasklock)
	for i in range(len(_rm_list) - 1, -1, -1):
		item = _rm_list[i]
		if item in tasks:
			tasks.remove(item)
			del item

	del _rm_list
	proxy().mutex_unlock(tasklock)

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
	global tasks, tasklock
	proxy().mutex_lock(tasklock)
	task = {}
	task["_interval"] = delay
	task["interval"] = interval
	task["callback"] = callback
	task["params"] = (task, *params)
	tasks.append(task)
	proxy().mutex_unlock(tasklock)

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

	if code == Qt.Key_Insert:
		return 45

	if code == Qt.Key_Delete:
		return 46

	if code == Qt.Key_F1:
		return 112

	if code == Qt.Key_F2:
		return 113

	if code == Qt.Key_F3:
		return 114

	if code == Qt.Key_F4:
		return 115

	if code == Qt.Key_F5:
		return 116

	if code == Qt.Key_F6:
		return 117

	if code == Qt.Key_F7:
		return 118

	if code == Qt.Key_F8:
		return 119

	if code == Qt.Key_F9:
		return 120

	if code == Qt.Key_F10:
		return 121

	if code == Qt.Key_F11:
		return 122

	if code == Qt.Key_F12:
		return 123

	if code == Qt.Key_Period:
		return 190

	if code == Qt.Key_BracketLeft:
		return 219

	if code == Qt.Key_BraceLeft:
		return 219

	if code == Qt.Key_BracketRight:
		return 221

	if code == Qt.Key_BraceRight:
		return 221

	if code == Qt.Key_Apostrophe:
		return 222

	if code == Qt.Key_QuoteDbl:
		return 222

	print(code)
	return code

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
