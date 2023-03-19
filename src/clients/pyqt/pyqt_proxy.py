#!/usr/bin/python3

from ctypes import *
from platform import *

cdll_names = {
			'Darwin' : 'libc.dylib',
			'Linux'  : '../../../client_build/src/clients/pyqt/libpyqt_proxy.so',
			'Windows': 'msvcrt.dll'
		}

clib = cdll.LoadLibrary(cdll_names[system()])
clib.py_client_connect.argtypes = [POINTER(c_char), c_ushort]
clib.py_client_connect.restype = c_int

clib.py_on_frame.argtypes = []
clib.py_on_frame.restype = c_int

clib.py_client_init_config.argtypes = []
clib.py_client_init_config.restype = c_int

clib.py_client_close.argtypes = []
clib.py_client_close.restype = c_int

clib.py_mouse_move.argtypes = [c_int, c_int]
clib.py_mouse_move.restype = c_int

clib.py_mouse_click.argtypes = [c_int, c_int, c_int, c_int]
clib.py_mouse_click.restype = c_int

clib.py_client_resize.argtypes = [c_uint, c_uint]
clib.py_client_resize.restype = c_int

clib.py_client_regist_stream_size_cb.argtypes = [py_object, c_void_p] 
clib.py_client_regist_stream_size_cb.restype = c_int

def proxy():
	global clib
	return clib

def new_callback(callback, *params):
	cb = CFUNCTYPE(None, *params)
	return cb(callback)