"""
Micro Python driver for SD cards using esp-idf sd_emmc driver.

Example usage on ESP32, automount defaults to True:

    import sdcard, uos, esp
    sd = sdcard.SDCard()
    uos.listdir('/sd')
    
    sd.umount()
    
Directory can be changed to SD card root automaticaly

    import sdcard, uos, esp
    sd = sdcard.SDCard(True)
    uos.listdir()
    
"""

import uos

class SDCard:
    VFS = None

    def __init__(self, chdir=False):
        # Using ESP32 native VFS
        self.VFS = uos.VfsNative(1)
        uos.mount(self.VFS, '/sd')
        if chdir:
            uos.chdir('/sd')

    def umount(self):
        uos.chdir('/flash')
        uos.umount(self.VFS)
        self.VFS = None

    def mount(self):
        uos.mount(self.VFS, '/sd')
