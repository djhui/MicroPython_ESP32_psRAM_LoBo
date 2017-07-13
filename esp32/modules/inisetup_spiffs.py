import uos

def setup():
    uos.VfsSpiffs.init()
    vfs = uos.VfsSpiffs()
    uos.mount(vfs, '/spiffs')
    uos.chdir('/spiffs')
    with open("boot.py", "w") as f:
        f.write("""\
# This file is executed on every boot (including wake-boot from deepsleep)
""")
    return vfs
