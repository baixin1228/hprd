#
# Regular cron jobs for the hprd package
#
0 4	* * *	root	[ -x /usr/bin/hprd_maintenance ] && /usr/bin/hprd_maintenance
