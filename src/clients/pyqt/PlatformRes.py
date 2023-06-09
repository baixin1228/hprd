#!/usr/bin/python3

import os
from platform import *

_instance = {}
def singleton(cls):
	global _instance
	def inner():
		if cls not in _instance:
			_instance[cls] = cls()
		return _instance[cls]
	return inner
	
@singleton
class PlatformRes():
	def __init__(self):
		self.system = system()
		self.is_install = os.getcwd().startswith("/usr")
		linux_true_res = {
			"icon" : "/usr/share/icons/hprd/hprd-icon.png"
		}
		linux_true_res["proxy_so"] = os.popen("whereis libpyqt_proxy.so | awk '{print $2}'").read().replace("\n", "")

		linux_false_res = {
			"icon" : "../../../res/logo_128.png",
			"proxy_so" : "../../../client_build/src/clients/pyqt/libpyqt_proxy.so"
		}
		self.res = {
					'Linux' : {
						True : linux_true_res,
						False : linux_false_res,
					},
					'Windows' : {},
					'Darwin' : {},
				}

	def get_icon_path(self):
		return self.res[self.system][self.is_install]["icon"]

	def get_proxy_path(self):
		return self.res[self.system][self.is_install]["proxy_so"]