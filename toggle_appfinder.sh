#!/bin/sh
if ! [ $(pgrep xfce4-appfinder) ]; then
	xfce4-appfinder &
fi
win_id=$(wmctrl -lx | grep xfce4-appfinder | awk '{print $1}')
if [ -z $win_id ]; then
	dbus-send --type=method_call --dest=org.xfce.Appfinder /org/xfce/Appfinder org.xfce.Appfinder.OpenWindow boolean:true string:''
else
	wmctrl -ic $win_id
fi
