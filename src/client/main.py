#!/usr/bin/python3

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *
import sys
import configparser

global UserName
UserP = {}  #定义一个存储密码账号的元组
class LoginWindow(QMainWindow):
	def __init__(self, parent=None):
		super(LoginWindow, self).__init__(parent)
		self.setupUi()
		self.initUi()

	def setupUi(self):
		self.resize(720, 400)
		self.setAnimated(True)
		self.centralwidget = QWidget(self)
		self.centralwidget.setObjectName("centralwidget")
		self.layout = QGridLayout()
		self.centralwidget.setLayout(self.layout)
		self.setCentralWidget(self.centralwidget)
		
		self.ip_lable = QLabel()
		self.ip_lable.setText("IP:")
		self.layout.addWidget(self.ip_lable,0,0,1,1, Qt.AlignRight)
		self.ip_edit = QLineEdit()
		self.ip_edit.setText("127.0.0.1:9999")
		self.layout.addWidget(self.ip_edit,0,1,1,3)

		self.user_lable = QLabel()
		self.user_lable.setText("Name:")
		self.layout.addWidget(self.user_lable,1,0,1,1, Qt.AlignRight)
		self.user_edit = QLineEdit()
		self.user_edit.setText("")
		self.layout.addWidget(self.user_edit,1,1,1,3)

		self.passwd_lable = QLabel()
		self.passwd_lable.setText("Password:")
		self.layout.addWidget(self.passwd_lable,2,0,1,1, Qt.AlignRight)
		self.passwd_edit = QLineEdit()
		self.passwd_edit.setText("")
		self.passwd_edit.setEchoMode(QLineEdit.Password)
		self.layout.addWidget(self.passwd_edit,2,1,1,3)

		self.conn_button = QPushButton()
		self.conn_button.setText("连接")
		self.layout.addWidget(self.conn_button,3,1,1,2)
 
		# self.retranslateUi()
 
	def initUi(self):
		# self.IsRememberUser()
		self.ip_edit.setFocus()
		self.ip_edit.setPlaceholderText("请输入ip地址")
		self.user_edit.setPlaceholderText("请输入用户名")
		self.passwd_edit.setPlaceholderText("请输入密码")

		self.conn_button.clicked.connect(self.on_connect)  # 登录


	"""设置记住密码"""
	def IsRememberUser(self):
		config = configparser.ConfigParser()
		file = config.read('user.ini') #读取密码账户的配置文件
		config_dict = config.defaults() #返回包含实例范围默认值的字典
		self.account = config_dict['user_name']  #获取账号信息
		self.UserName.setText(self.account)  #写入账号上面
		if config_dict['remember'] == 'True':
			self.passwd = config_dict['password']
			self.PassWord.setText(self.passwd)
			self.RememberUser.setChecked(True)
		else:
			self.RememberUser.setChecked(False)

	"""设置配置文件格式"""
	def config_ini(self):
		self.account = self.UserName.text()
		self.passwd = self.PassWord.text()
		config = configparser.ConfigParser()
		if self.RememberUser.isChecked():
			config["DEFAULT"] = {
				"user_name":self.account,
				"password":self.passwd,
				"remember":self.RememberUser.isChecked()
			}
		else:
			config["DEFAULT"] = {
				"user_name": self.account,
				"password": "",
				"remember": self.RememberUser.isChecked()
			}
		with open('user.ini', 'w') as configfile:
			config.write((configfile))
		print(self.account, self.passwd)
		
	#注册
	def regist_button(self):
		#载入数据库
		# self.sql = Oper_Mysql()
		# self.sql.ZSGC_Mysql()
		self.re.show()
		w.close()

	#登录
	def on_connect(self):
		hprd.connect(self.ip_edit.text())

	def conn_success(self):
		w.show()
		self.close()

if __name__=="__main__":
	app = QApplication(sys.argv)
	w = LoginWindow()
	w.show()
	sys.exit(app.exec())