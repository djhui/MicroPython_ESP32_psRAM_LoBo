/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Originally created by SHA2017 Badge Team (https://github.com/SHA2017-badge/micropython-esp32)
 *
 * Modified by LoBo (https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo)
 *
 * Added support for SD Card and some changes to make it work (hopefully) better
 *
 */

#include "py/lexer.h"
#include "py/obj.h"
#include "py/objint.h"
#include "extmod/vfs.h"

#define VFS_NATIVE_MOUNT_POINT "/spiflash"
#define VFS_NATIVE_SDCARD_MOUNT_POINT "/sdcard"
#define VFS_NATIVE_TYPE_SPIFLASH 0
#define VFS_NATIVE_TYPE_SDCARD 1

typedef struct _fs_user_mount_t {
    mp_obj_base_t base;
    mp_int_t device;
    mp_int_t mode;
} fs_user_mount_t;

extern const mp_obj_type_t mp_native_vfs_type;

const char * mkabspath(fs_user_mount_t *vfs, const char *path, char *absbuf, int buflen);
mp_import_stat_t native_vfs_import_stat(struct _fs_user_mount_t *vfs, const char *path);
mp_obj_t nativefs_builtin_open_self(mp_obj_t self_in, mp_obj_t path, mp_obj_t mode);
MP_DECLARE_CONST_FUN_OBJ_KW(mp_builtin_open_obj);

mp_obj_t native_vfs_ilistdir2(struct _fs_user_mount_t *vfs, const char *path, bool is_str_type);
