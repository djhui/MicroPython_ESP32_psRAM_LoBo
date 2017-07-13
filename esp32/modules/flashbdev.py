import esp

class FlashBdev:

    FS_SIZE = esp.flash_size()
    SEC_SIZE = esp.flash_sec_size()
    START_SEC = esp.flash_user_start() // SEC_SIZE
    FS_USE_WL = esp.flash_use_wl()

    def __init__(self, blocks):
        self.blocks = blocks
        print("FlashBdev init: Size=" + str(self.FS_SIZE) + ", Blocks=" + str(self.blocks))
        if self.FS_USE_WL > 0:
            print("Using wear leveling FAT file system")

    def readblocks(self, n, buf):
        #print("readblocks(%s, %x(%d))" % (n, id(buf), len(buf)))
        esp.flash_read((n + self.START_SEC) * self.SEC_SIZE, buf)

    def writeblocks(self, n, buf):
        #print("writeblocks(%s, %x(%d))" % (n, id(buf), len(buf)))
        #assert len(buf) <= self.SEC_SIZE, len(buf)
        esp.flash_erase(n + self.START_SEC)
        esp.flash_write((n + self.START_SEC) * self.SEC_SIZE, buf)

    def ioctl(self, op, arg):
        #print("ioctl(%d, %r)" % (op, arg))
        if op == 4:  # BP_IOCTL_SEC_COUNT
            return self.blocks
        if op == 5:  # BP_IOCTL_SEC_SIZE
            return self.SEC_SIZE

size = esp.flash_size()
if size < 523264:
    # flash too small for a filesystem
    print("FlashBdev: no space for file system")
    bdev = None
else:
    # init with number of sectors
    bdev = FlashBdev(size // FlashBdev.SEC_SIZE)
