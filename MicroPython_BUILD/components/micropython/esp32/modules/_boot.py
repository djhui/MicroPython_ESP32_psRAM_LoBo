import gc
import uos

try:
    if uos.vfstype() == 1:
        # Using ESP32 native VFS
        uos.mount(uos.VfsNative(0), '/flash')
        uos.chdir('/flash')
    else:
        # Using MicroPython VFS
        from flashbdev import bdev
        if bdev:
            uos.mount(bdev, '/')
except OSError:
    if uos.vfstype() == 0:
        import inisetup
        vfs = inisetup.setup()

gc.collect()
