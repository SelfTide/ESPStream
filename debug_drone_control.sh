#!/usr/bin/env bash

# Check if app is already installed, if so remove
if [ -n `adb.exe shell pm list packages | grep org.yourorg.Drone_control` ]; then 
	echo "Uninstalling app!"
	adb.exe uninstall org.yourorg.Drone_control
fi

# Make target app and push to device, then run
make clean
make run

# A work around that was found to be able to make the gdbserver executable
adb.exe shell run-as org.yourorg.Drone_control cp /sdcard/gdbserver /data/data/org.yourorg.Drone_control/

adb.exe shell run-as org.yourorg.Drone_control chmod 777 /data/data/org.yourorg.Drone_control/gdbserver

# Forward ports for debugging
adb.exe forward tcp:8888 tcp:8888

sleep 5

# Get the pid of the current running app and attach gdbserver, allow for remote connection on localhost port 8888, run in background.
adb.exe shell run-as org.yourorg.Drone_control /data/data/org.yourorg.Drone_control/gdbserver --remote-debug 127.0.0.1:8888 --attach `adb.exe shell run-as org.yourorg.Drone_control ps -A|grep Drone_control|sed 's/ \{1,\}/ /g'|cut -d ' ' -f2` &

# Tail log
adb.exe logcat | grep Drone_control