#!/usr/bin/python3
import os
import sys
from ctypes import *
from platform import *

cdll_names = {
			'Darwin' : 'libc.dylib',
			'Linux'  : '../../../client_build/src/clients/pyqt/libpyqt_proxy.so',
			'Windows': 'msvcrt.dll'
		}

cdll_pkg = {
			'Darwin' : 'libc.dylib',
			'Linux'  : "whereis libpyqt_proxy.so | awk '{print $2}'",
			'Windows': 'msvcrt.dll'
		}


if os.path.exists(cdll_names[system()]):
	clib = cdll.LoadLibrary(cdll_names[system()])
else:
	cmd_ret = os.popen(cdll_pkg[system()])
	if cmd_ret:
		pkg_path = cmd_ret.read().replaec("\n", "")
		clib = cdll.LoadLibrary(pkg_path)
	else:
		print("not find libpyqt_proxy.so, exit...")
		sys.exit(-1)

clib.py_client_connect.argtypes = [POINTER(c_char), c_ushort]
clib.py_client_connect.restype = c_int

clib.py_kcp_connect.argtypes = [c_uint]
clib.py_kcp_connect.restype = c_int

clib.py_kcp_active.argtypes = []
clib.py_kcp_active.restype = c_int

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

clib.py_wheel_event.argtypes = [c_int]
clib.py_wheel_event.restype = c_int

clib.py_clip_event.argtypes = [POINTER(c_char), POINTER(c_char), c_ushort]
clib.py_clip_event.restype = c_int

clib.py_key_event.argtypes = [c_int, c_int]
clib.py_key_event.restype = c_int

clib.py_client_resize.argtypes = [c_uint, c_uint]
clib.py_client_resize.restype = c_int

clib.py_get_recv_count.argtypes = []
clib.py_get_recv_count.restype = c_uint

clib.py_get_queue_len.argtypes = []
clib.py_get_queue_len.restype = c_uint

clib.py_get_and_clean_frame.argtypes = []
clib.py_get_and_clean_frame.restype = c_uint

clib.py_get_frame_rate.argtypes = [py_object, c_void_p]
clib.py_get_frame_rate.restype = c_int

clib.py_get_bit_rate.argtypes = [py_object, c_void_p]
clib.py_get_bit_rate.restype = c_int

clib.py_set_frame_rate.argtypes = [py_object, c_uint, c_void_p]
clib.py_set_frame_rate.restype = c_int

clib.py_set_bit_rate.argtypes = [py_object, c_uint, c_void_p]
clib.py_set_bit_rate.restype = c_int

clib.py_set_share_clipboard.argtypes = [py_object, c_uint, c_void_p]
clib.py_set_share_clipboard.restype = c_int

clib.py_get_remote_fps.argtypes = [py_object, c_void_p]
clib.py_get_remote_fps.restype = c_int

clib.py_get_client_id.argtypes = [py_object, c_void_p]
clib.py_get_client_id.restype = c_int

clib.py_ping.argtypes = [py_object, c_uint, c_void_p]
clib.py_ping.restype = c_int

clib.py_client_regist_stream_size_cb.argtypes = [py_object, c_void_p] 
clib.py_client_regist_stream_size_cb.restype = c_int

clib.alloc_mutex.argtypes = [] 
clib.alloc_mutex.restype = c_void_p

clib.free_mutex.argtypes = [c_void_p] 

clib.mutex_lock.argtypes = [c_void_p] 

clib.mutex_unlock.argtypes = [c_void_p] 

clib.alloc_spinlock.argtypes = [] 
clib.alloc_spinlock.restype = c_void_p

clib.free_spinlock.argtypes = [c_void_p] 

clib.spinlock_lock.argtypes = [c_void_p] 

clib.spinlock_unlock.argtypes = [c_void_p] 

def proxy():
	global clib
	return clib

def new_callback(callback, *params):
	cb = CFUNCTYPE(None, *params)
	return cb(callback)