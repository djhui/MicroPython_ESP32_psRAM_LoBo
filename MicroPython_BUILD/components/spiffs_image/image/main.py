# This file is executed on every boot (including wake-boot from deepsleep)

import machine, network, utime
from pye import pye

print("")
print("Starting WiFi ...")
sta_if = network.WLAN(network.STA_IF); sta_if.active(True)
sta_if.connect("<WiFi_SSID>", "<WiFi_pass>")
while not sta_if.isconnected():
    utime.sleep_ms(100)

print("WiFi started")
utime.sleep_ms(500)

rtc = machine.RTC()
print("Synchronize time from NTP server ...")
rtc.ntp_sync(server="hr.pool.ntp.org")
while not rtc.synced():
    utime.sleep_ms(100)

print("Time set")
utime.sleep_ms(500)
t = rtc.now()
print(str(t[3])+':'+str(t[4])+':'+str(t[5])+' '+str(t[2])+'/'+str(t[1])+'/'+str(t[0]))
print("")

