#!/usr/bin/expect --
spawn adb shell
expect "$" {
    sleep 0.1
    send "export PATH=/data/data/burrows.apps.busybox/app_busybox/:\$PATH\n"
}
interact