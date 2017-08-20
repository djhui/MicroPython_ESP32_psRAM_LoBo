import gc
import uos

try:
    # ## DO NOT CHANGE MOUNT POINT, IT MUST BE /flash ##
    uos.mount(uos.VfsNative(0), '/flash')
    uos.chdir('flash')
except OSError:
    print("Error mounting internal file system")

gc.collect()
