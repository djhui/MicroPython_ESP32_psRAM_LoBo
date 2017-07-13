"""
Micro Python driver for SD cards using sd_emmc driver.

Example usage on ESP32:

    import sdcard, uos
    sd = sdcard.SDCard()
    vfs = uos.VfsFat(sd)
    uos.mount(vfs, '/sd')
    uos.chdir('/sd')
    uos.listdir()

"""

import esp

class SDCard:
    def __init__(self, automount):
        self.SD_FOUND = esp.sdcard_init()
        self.SEC_COUNT = esp.sdcard_sect_count()
        self.SEC_SIZE = esp.sdcard_sect_size()
        self.SIZE = self.SEC_SIZE * self.SEC_COUNT
        if self.SD_FOUND == 0 and automount == 1:
            import uos
            vfs = uos.VfsFat(self)
            uos.mount(vfs, '/sd')
            uos.chdir('/sd')

    def count(self):
        return esp.sdcard_sect_count()

    def readblocks(self, block_num, buf):
        esp.sdcard_read(block_num, buf)
        return 0

    def writeblocks(self, block_num, buf):
        esp.sdcard_write(block_num, buf)
        return 0

"""
    def ioctl(self, op, arg):
        #print("ioctl(%d, %r)" % (op, arg))
        if op == 4:  # BP_IOCTL_SEC_COUNT
            return self.SEC_COUNT
        if op == 5:  # BP_IOCTL_SEC_SIZE
            return self.SEC_SIZE
"""
