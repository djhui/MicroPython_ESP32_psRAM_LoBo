import uos
from flashbdev import bdev

def check_bootsec():
    buf = bytearray(bdev.SEC_SIZE)
    bdev.readblocks(0, buf)
    empty = True
    for b in buf:
        if b != 0xff:
            empty = False
            break
    if empty:
        return True
    fs_corrupted()

def fs_corrupted():
    import time
    n = 60
    while n > 0:
        print("FAT filesystem appears to be corrupted. If you had important data there,")
        print("you may want to make a flash snapshot to try to recover it.")
        print("Otherwise, wait " +  str(n) + " seconds for file system to be reformated.")
        print("")

        time.sleep(5)
        n = n - 5

def setup():
    check_bootsec()
    uos.VfsFat.mkfs(bdev)
    vfs = uos.VfsFat(bdev)
    uos.mount(vfs, '/flash')
    uos.chdir('/flash')
    with open("boot.py", "w") as f:
        f.write("""\
# This file is executed on every boot (including wake-boot from deepsleep)
""")
    return vfs
