"""
Micro Python driver for SD cards using esp-idf sd_emmc driver.

Example usage on ESP32, automount defaults to True:

    import sdcard, uos, esp
    sd = sdcard.SDCard(esp.SD_1LINE)
    uos.listdir('/sd')
    
If 'automount' is not used

    import sdcard, uos
    sd = sdcard.SDCard(esp.SD_4LINE, False)
    vfs = uos.VfsFat(sd)
    
Manual mount:
    uos.mount(vfs, '/sd')
    uos.chdir('/sd')
    uos.listdir()

"""

import esp
import uos

class SDCard:
    def __init__(self, mode, automount=True):
        if uos.vfstype() == 1:
            # Using ESP32 native VFS
            uos.mount(uos.VfsNative(1, mode), '/sd')
            # uos.chdir('/sd')
        else:
            # Using MicroPython VFS
            self.SD_FOUND = esp.sdcard_init(mode)
            self.SEC_COUNT = esp.sdcard_sect_count()
            self.SEC_SIZE = esp.sdcard_sect_size()
            self.SIZE = self.SEC_SIZE * self.SEC_COUNT
            # automount sdcard if requested
            if self.SD_FOUND == 0 and automount:
                vfs = uos.VfsFat(self)
                uos.mount(vfs, '/sd')
                # uos.chdir('/sd')

    # Used only in MicroPython VFS mode
    def count(self):
        return esp.sdcard_sect_count()

    def readblocks(self, block_num, buf):
        esp.sdcard_read(block_num, buf)
        return 0

    def writeblocks(self, block_num, buf):
        esp.sdcard_write(block_num, buf)
        return 0

