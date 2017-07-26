import gc
import uos

try:
    uos.mount(uos.VfsNative(0), '/flash')
    uos.chdir('flash')
except OSError:
    print("Error mounting internal file system")

gc.collect()
