#
# Regular cron jobs for the wcm package
#
0 4	* * *	root	[ -x /usr/bin/wcm_maintenance ] && /usr/bin/wcm_maintenance
