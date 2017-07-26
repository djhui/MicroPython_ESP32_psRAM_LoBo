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

#include "py/mpconfig.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "ff.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_log.h"
#include "spiffs_vfs.h"
#include "modesp.h"
#include "diskio.h"
#include <fcntl.h>
#include "diskio_spiflash.h"

#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "extmod/vfs_native.h"
#include "lib/timeutils/timeutils.h"
#include "sdkconfig.h"


STATIC const byte fresult_to_errno_table[20] = {
    [FR_OK] = 0,
    [FR_DISK_ERR] = MP_EIO,
    [FR_INT_ERR] = MP_EIO,
    [FR_NOT_READY] = MP_EBUSY,
    [FR_NO_FILE] = MP_ENOENT,
    [FR_NO_PATH] = MP_ENOENT,
    [FR_INVALID_NAME] = MP_EINVAL,
    [FR_DENIED] = MP_EACCES,
    [FR_EXIST] = MP_EEXIST,
    [FR_INVALID_OBJECT] = MP_EINVAL,
    [FR_WRITE_PROTECTED] = MP_EROFS,
    [FR_INVALID_DRIVE] = MP_ENODEV,
    [FR_NOT_ENABLED] = MP_ENODEV,
    [FR_NO_FILESYSTEM] = MP_ENODEV,
    [FR_MKFS_ABORTED] = MP_EIO,
    [FR_TIMEOUT] = MP_EIO,
    [FR_LOCKED] = MP_EIO,
    [FR_NOT_ENOUGH_CORE] = MP_ENOMEM,
    [FR_TOO_MANY_OPEN_FILES] = MP_EMFILE,
    [FR_INVALID_PARAMETER] = MP_EINVAL,
};

#if _MAX_SS == _MIN_SS
#define SECSIZE(fs) (_MIN_SS)
#else
#define SECSIZE(fs) ((fs)->ssize)
#endif

#define mp_obj_native_vfs_t fs_user_mount_t

STATIC const char *TAG = "vfs_native";

#if !MICROPY_USE_SPIFFS
STATIC wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
#endif
STATIC bool native_vfs_mounted[2] = {false, false};
STATIC sdmmc_card_t *sdmmc_card;


// esp-idf doesn't seem to have a cwd; create one.
char cwd[MICROPY_ALLOC_PATH_MAX + 1] = { 0 };

//-------------------------------------
int vfs_chdir(const char *path, int device)
{
	ESP_LOGD(TAG, "vfs_chdir() path: '%s'", path);

	int f = 1;
	if ((device == VFS_NATIVE_TYPE_SDCARD) && (strcmp(path,  VFS_NATIVE_SDCARD_MOUNT_POINT"/") == 0)) f = 0;
	else if (strcmp(path, VFS_NATIVE_MOUNT_POINT"/") == 0) f = 0;

	if (f) {
		struct stat buf;
		int res = stat(path, &buf);
		if (res < 0) {
			return -1;
		}
		if ((buf.st_mode & S_IFDIR) == 0)
		{
			errno = ENOTDIR;
			return -2;
		}
		if (strlen(path) >= sizeof(cwd))
		{
			errno = ENAMETOOLONG;
			return -3;
		}
	}

	strncpy(cwd, path, sizeof(cwd));

	ESP_LOGD(TAG, "cwd set to '%s' from path '%s'", cwd, path);
	return 0;
}

//----------------------------------
char *getcwd(char *buf, size_t size)
{
	if (size <= strlen(cwd))
	{
		errno = ENAMETOOLONG;
		return NULL;
	}
	strcpy(buf, cwd);
	return buf;
}

// Return absolute path un Flash filesystem
// It always starts with VFS_NATIVE_[xxx_]MOUNT_POINT (/spiflash/ | /spiffs/ | /sdcard/)
// with 'path' stripped of leading '/', './', '../', '..'
// On input 'path' DOES NOT contain MPY mount point ('/flash' or 'sd')
//-------------------------------------------------------------------------------------
const char *mkabspath(fs_user_mount_t *vfs, const char *path, char *absbuf, int buflen)
{
	ESP_LOGD(TAG, "abspath '%s' in cwd '%s'", path, cwd);

	if (path[0] == '/')
	{ // path is already absolute
		if (vfs->device == VFS_NATIVE_TYPE_SDCARD) sprintf(absbuf, "%s%s", VFS_NATIVE_SDCARD_MOUNT_POINT, path);
		else sprintf(absbuf, "%s%s", VFS_NATIVE_MOUNT_POINT, path);
		ESP_LOGD(TAG, " path '%s' is absolute `-> '%s'", path, absbuf);
		return absbuf;
	}

	int len;
	char buf[strlen(cwd) + 16];

	if (vfs->device == VFS_NATIVE_TYPE_SDCARD) {
		if (strstr(cwd, VFS_NATIVE_SDCARD_MOUNT_POINT) != cwd) {
			strcpy(buf, VFS_NATIVE_SDCARD_MOUNT_POINT);
			if (cwd[0] != '/') strcat(buf, "/");
			strcat(buf, cwd);
		}
		else strcpy(buf, cwd);
	}
	else {
		if (strstr(cwd, VFS_NATIVE_MOUNT_POINT) != cwd)	{
			strcpy(buf, VFS_NATIVE_MOUNT_POINT);
			if (cwd[0] != '/') strcat(buf, "/");
			strcat(buf, cwd);
		}
		else strcpy(buf, cwd);
	}
	if (buf[strlen(buf)-1] == '/') buf[strlen(buf)-1] = 0; // remove trailing '/' from cwd

	len = strlen(buf);
	while (1) {
		// handle './' and '../'
		if (path[0] == 0)
			break;
		if (path[0] == '.' && path[1] == 0) { // '.'
			path = &path[1];
			break;
		}
		if (path[0] == '.' && path[1] == '/') { // './'
			path = &path[2];
			continue;
		}
		if (path[0] == '.' && path[1] == '.' && path[2] == 0) { // '..'
			path = &path[2];
			while (len > 0 && buf[len] != '/') len--; // goto cwd parrent dir
			buf[len] = 0;
			break;
		}
		if (path[0] == '.' && path[1] == '.' && path[2] == '/') { // '../'
			path = &path[3];
			while (len > 0 && buf[len] != '/') len--; // goto cwd parrent dir
			buf[len] = 0;
			continue;
		}
		if (strlen(buf) >= buflen-1) {
			errno = ENAMETOOLONG;
			return NULL;
		}
		break;
	}

	if (strlen(buf) + strlen(path) >= buflen) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	ESP_LOGD(TAG, " cwd: '%s'  path: '%s'", buf, path);
	strcpy(absbuf, buf);
	if ((strlen(path) > 0) && (path[0] != '/')) strcat(absbuf, "/");
	strcat(absbuf, path);

	// If root is selected, add trailing '/'
	if ((vfs->device == VFS_NATIVE_TYPE_SDCARD) && (strcmp(absbuf, VFS_NATIVE_SDCARD_MOUNT_POINT) == 0)) strcat(absbuf, "/");
	else if (strcmp(absbuf, VFS_NATIVE_MOUNT_POINT) == 0) strcat(absbuf, "/");

	ESP_LOGD(TAG, " '%s' -> '%s'", path, absbuf);
	return absbuf;
}

//================================================================================================================
STATIC mp_obj_t native_vfs_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
	mp_arg_check_num(n_args, n_kw, 1, 2, false);

	mp_int_t dev_type = mp_obj_get_int(args[0]);
	if ((dev_type != VFS_NATIVE_TYPE_SPIFLASH) && (dev_type != VFS_NATIVE_TYPE_SDCARD)) {
		ESP_LOGD(TAG, "Unknown device type (%d)", dev_type);
		mp_raise_OSError(ENXIO);
	}

	// create new object
	fs_user_mount_t *vfs = m_new_obj(fs_user_mount_t);
	vfs->base.type = type;
	vfs->device = mp_obj_get_int(args[0]);
	if (vfs->device == VFS_NATIVE_TYPE_SDCARD) {
		vfs->mode = mp_obj_get_int(args[1]);
	}
	else {
		vfs->mode = 0;
	}

	return MP_OBJ_FROM_PTR(vfs);
}

//-------------------------------------------------
STATIC mp_obj_t native_vfs_mkfs(mp_obj_t bdev_in) {
	ESP_LOGE(TAG, "mkfs(): NOT SUPPORTED");
	// not supported
	mp_raise_OSError(ENOENT);
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(native_vfs_mkfs_fun_obj, native_vfs_mkfs);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(native_vfs_mkfs_obj, MP_ROM_PTR(&native_vfs_mkfs_fun_obj));

STATIC MP_DEFINE_CONST_FUN_OBJ_3(native_vfs_open_obj, nativefs_builtin_open_self);

//-----------------------------------------------------------------------------
STATIC mp_obj_t native_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(args[0]);

	bool is_str_type = true;
	const char *path;
	if (n_args == 2) {
		if (mp_obj_get_type(args[1]) == &mp_type_bytes) {
			is_str_type = false;
		}
		path = mp_obj_str_get_str(args[1]);
	} else {
		path = "";
	}

	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(self, path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return native_vfs_ilistdir2(self, path, is_str_type);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(native_vfs_ilistdir_obj, 1, 2, native_vfs_ilistdir_func);

//--------------------------------------------------------------------
STATIC mp_obj_t native_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_in) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(self, path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	int res = unlink(path);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_remove_obj, native_vfs_remove);

//-------------------------------------------------------------------
STATIC mp_obj_t native_vfs_rmdir(mp_obj_t vfs_in, mp_obj_t path_in) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(self, path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	int res = rmdir(path);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_rmdir_obj, native_vfs_rmdir);

//---------------------------------------------------------------------------------------
STATIC mp_obj_t native_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_in, mp_obj_t path_out) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *old_path = mp_obj_str_get_str(path_in);
	const char *new_path = mp_obj_str_get_str(path_out);

	char old_absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	old_path = mkabspath(self, old_path, old_absbuf, sizeof(old_absbuf));
	if (old_path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	char new_absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	new_path = mkabspath(self, new_path, new_absbuf, sizeof(new_absbuf));
	if (new_path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	int res = rename(old_path, new_path);
	/*
	// FIXME: have to check if we can replace files with this
	if (res < 0 && errno == EEXISTS) {
		res = unlink(new_path);
		if (res < 0) {
			mp_raise_OSError(errno);
			return mp_const_none;
		}
		res = rename(old_path, new_path);
	}
	*/
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(native_vfs_rename_obj, native_vfs_rename);

//------------------------------------------------------------------
STATIC mp_obj_t native_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_o) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_o);

	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(self, path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	int res = mkdir(path, 0755);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_mkdir_obj, native_vfs_mkdir);

/// Change current directory.
//-------------------------------------------------------------------
STATIC mp_obj_t native_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_in) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(self, path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	int res = vfs_chdir(path, self->device);
	if (res < 0) {
		ESP_LOGD(TAG, "chdir(): Error %d (%d)", res, errno);
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_chdir_obj, native_vfs_chdir);

/// Get the current directory.
//--------------------------------------------------
STATIC mp_obj_t native_vfs_getcwd(mp_obj_t vfs_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);

	char buf[MICROPY_ALLOC_PATH_MAX + 1];

	char *ch = getcwd(buf, sizeof(buf));
	if (ch == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_obj_new_str(buf, strlen(buf), false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(native_vfs_getcwd_obj, native_vfs_getcwd);

/// \function stat(path)
/// Get the status of a file or directory.
//------------------------------------------------------------------
STATIC mp_obj_t native_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_in) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(self, path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	struct stat buf;
	if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
		// stat root directory
		buf.st_size = 0;
		buf.st_atime = 946684800; // Jan 1, 2000
		buf.st_mode = MP_S_IFDIR;
	} else {
		int res = stat(path, &buf);
		if (res < 0) {
			mp_raise_OSError(errno);
			return mp_const_none;
		}
	}

	mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
	t->items[0] = MP_OBJ_NEW_SMALL_INT(buf.st_mode);
	t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // st_ino
	t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // st_dev
	t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // st_nlink
	t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // st_uid
	t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // st_gid
	t->items[6] = MP_OBJ_NEW_SMALL_INT(buf.st_size); // st_size
	t->items[7] = MP_OBJ_NEW_SMALL_INT(buf.st_atime); // st_atime
	t->items[8] = MP_OBJ_NEW_SMALL_INT(buf.st_atime); // st_mtime
	t->items[9] = MP_OBJ_NEW_SMALL_INT(buf.st_atime); // st_ctime

	return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_stat_obj, native_vfs_stat);


// Get the status of a VFS.
//---------------------------------------------------------------------
STATIC mp_obj_t native_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	ESP_LOGD(TAG, "statvfs('%s') device: %d", path, self->device);

	int f_bsize=0, f_blocks=0, f_bfree=0, maxlfn=0;
    FRESULT res=0;
	FATFS *fatfs;
    DWORD fre_clust;

	if (self->device == VFS_NATIVE_TYPE_SPIFLASH) {
		#if MICROPY_USE_SPIFFS
		uint32_t total, used;
		spiffs_fs_stat(&total, &used);
		f_bsize = SPIFFS_LOG_PAGE_SIZE;
		f_blocks = total / SPIFFS_LOG_PAGE_SIZE;
		f_bfree = (total-used) / SPIFFS_LOG_PAGE_SIZE;
		maxlfn = MAXNAMLEN;
		#else
		res = f_getfree(VFS_NATIVE_MOUNT_POINT, &fre_clust, &fatfs);
		goto is_fat;
		#endif
	}
	else if (self->device == VFS_NATIVE_TYPE_SDCARD) {
		res = f_getfree(VFS_NATIVE_SDCARD_MOUNT_POINT, &fre_clust, &fatfs);
#if !MICROPY_USE_SPIFFS
is_fat:
#endif
	    if (res != 0) {
	    	ESP_LOGD(TAG, "statvfs('%s') Error %d", path, res);
	        mp_raise_OSError(fresult_to_errno_table[res]);
	    }
	    f_bsize = fatfs->csize * SECSIZE(fatfs);
	    f_blocks = fatfs->n_fatent - 2;
	    f_bfree = fre_clust;
	    maxlfn = _MAX_LFN;
	}

	mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

    t->items[0] = MP_OBJ_NEW_SMALL_INT(f_bsize); // f_bsize
    t->items[1] = t->items[0]; // f_frsize
    t->items[2] = MP_OBJ_NEW_SMALL_INT(f_blocks); // f_blocks
    t->items[3] = MP_OBJ_NEW_SMALL_INT(f_bfree); // f_bfree
    t->items[4] = t->items[3]; // f_bavail
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // f_files
    t->items[6] = MP_OBJ_NEW_SMALL_INT(0); // f_ffree
    t->items[7] = MP_OBJ_NEW_SMALL_INT(0); // f_favail
    t->items[8] = MP_OBJ_NEW_SMALL_INT(0); // f_flags
    t->items[9] = MP_OBJ_NEW_SMALL_INT(maxlfn); // f_namemax

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_statvfs_obj, native_vfs_statvfs);


#if !MICROPY_USE_SPIFFS
//------------------------------------------------------------------------------------------------------------------------------------
STATIC esp_err_t vfs_fat_spiflash_mount(const char* base_path, const esp_vfs_fat_mount_config_t* mount_config, wl_handle_t* wl_handle)
{
    esp_err_t result = ESP_OK;
    const size_t workbuf_size = 4096;
    void *workbuf = NULL;

    result = wl_mount(&fs_part, wl_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount wear leveling layer. result = %i", result);
        return result;
    }
    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) {
        ESP_LOGD(TAG, "The maximum number of volumes is already mounted");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "Using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    result = ff_diskio_register_wl_partition(pdrv, *wl_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "ff_diskio_register_wl_partition failed pdrv=%i, error - 0x(%x)", pdrv, result);
        goto fail;
    }
    FATFS *fs;
    result = esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs);
    if (result == ESP_ERR_INVALID_STATE) {
        // it's okay, already registered with VFS
    } else if (result != ESP_OK) {
        ESP_LOGD(TAG, "esp_vfs_fat_register failed 0x(%x)", result);
        goto fail;
    }

    // Try to mount partition
    FRESULT fresult = f_mount(fs, drv, 1);
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        if (!(fresult == FR_NO_FILESYSTEM && mount_config->format_if_mount_failed)) {
            result = ESP_FAIL;
            goto fail;
        }
        workbuf = malloc(workbuf_size);
        ESP_LOGI(TAG, "** Formatting Flash FATFS partition **");
        fresult = f_mkfs(drv, FM_ANY | FM_SFD, workbuf_size, workbuf, workbuf_size);
        if (fresult != FR_OK) {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
            goto fail;
        }
        free(workbuf);
        ESP_LOGI(TAG, "** Mounting again **");
        fresult = f_mount(fs, drv, 0);
        if (fresult != FR_OK) {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
            goto fail;
        }
    }
    return ESP_OK;

fail:
    free(workbuf);
    esp_vfs_fat_unregister_path(base_path);
    ff_diskio_unregister(pdrv);
    return result;
}
#endif

//------------------------
STATIC void cleckBoot_py()
{
	FILE *fd;
	fd = fopen(VFS_NATIVE_MOUNT_POINT"/boot.py", "rb");
    if (fd == NULL) {
    	fd = fopen(VFS_NATIVE_MOUNT_POINT"/boot.py", "wb");
        if (fd != NULL) {
        	char buf[128] = {'\0'};
        	sprintf(buf, "# This file is executed on every boot (including wake-boot from deepsleep)\nimport sys\nsys.path[1] = '/flash/lib'\n");
        	int len = strlen(buf);
    		int res = fwrite(buf, 1, len, fd);
    		if (res != len) {
    			ESP_LOGE(TAG, "Error writing to 'boot.py'");
    		}
    		else {
    			ESP_LOGD(TAG, "** 'boot.py' created **");
    		}
    		fclose(fd);
        }
        else {
			ESP_LOGE(TAG, "Error creating 'boot.py'");
        }
    }
    else {
		ESP_LOGD(TAG, "** 'boot.py' found **");
    	fclose(fd);
    }
}

//------------------------------------------------------------------------------------
STATIC mp_obj_t native_vfs_mount(mp_obj_t self_in, mp_obj_t readonly, mp_obj_t mkfs) {
	fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

	if ((self->device != VFS_NATIVE_TYPE_SPIFLASH) && (self->device != VFS_NATIVE_TYPE_SDCARD)) {
		ESP_LOGE(TAG, "Unknown device type (%d)", self->device);
		mp_raise_OSError(ENXIO);
	}

	// we will do an initial mount only on first call
	// already mounted?
	if (native_vfs_mounted[self->device]) {
		ESP_LOGW(TAG, "Device %d already mounted.", self->device);
		return mp_const_none;
	}

	if (self->device == VFS_NATIVE_TYPE_SPIFLASH) {
		// spiflash device
		#if MICROPY_USE_SPIFFS
		vfs_spiffs_register();
	   	if (spiffs_is_mounted == 0) {
			ESP_LOGE(TAG, "Failed to mount Flash partition as SPIFFS.");
			mp_raise_OSError(MP_EIO);
	   	}
		native_vfs_mounted[self->device] = true;
		cleckBoot_py();
		#else
		const esp_vfs_fat_mount_config_t mount_config = {
			.max_files              = CONFIG_MICROPY_FATFS_MAX_OPEN_FILES,
			.format_if_mount_failed = true,
		};

		ESP_LOGD(TAG, "Mounting Flash partition %s [size: %d; Flash address: %8X]", fs_part.label, fs_part.size, fs_part.address);
		// Mount spi Flash filesystem using configuration from sdkconfig.h
		esp_err_t err = vfs_fat_spiflash_mount(VFS_NATIVE_MOUNT_POINT, &mount_config, &s_wl_handle);

		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Failed to mount Flash partition as FatFS(%d)", err);
			mp_raise_OSError(MP_EIO);
		}
		ESP_LOGD(TAG, "SPI Flash FATFS mounted.");
		native_vfs_mounted[self->device] = true;
		cleckBoot_py();
		#endif
	}
	else if (self->device == VFS_NATIVE_TYPE_SDCARD) {
	    // Configure sdmmc interface
	    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
	    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	    // Enable pull-ups on the SD card pins
	    // ** It is recommended to use external 10K pull-ups **
	    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);
	    gpio_set_pull_mode(14, GPIO_PULLUP_ONLY);
	    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);

		if (self->mode == 1) {
	        // Use 1-line SD mode
	        host.flags = SDMMC_HOST_FLAG_1BIT;
	        slot_config.width = 1;
	    }
	    else {
	        gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);
	        gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);
	        gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);
	    }

		esp_vfs_fat_sdmmc_mount_config_t mount_config = {
	        .format_if_mount_failed = false,
	        .max_files = CONFIG_MICROPY_FATFS_MAX_OPEN_FILES
	    };

	    esp_err_t ret = esp_vfs_fat_sdmmc_mount(VFS_NATIVE_SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &sdmmc_card);
	    if (ret != ESP_OK) {
	        if (ret == ESP_FAIL) {
	            ESP_LOGE(TAG, "Failed to mount filesystem on SDcard.");
	        } else {
	            ESP_LOGE(TAG, "Failed to initialize SDcard (%d).", ret);
	        }
			mp_raise_OSError(MP_EIO);
	    }
		ESP_LOGV(TAG, "SDCard FATFS mounted.");
        sdcard_print_info(sdmmc_card, self->mode);
		native_vfs_mounted[self->device] = true;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(native_vfs_mount_obj, native_vfs_mount);

//---------------------------------------------------
STATIC mp_obj_t native_vfs_umount(mp_obj_t self_in) {
	fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

	if ((self->device == VFS_NATIVE_TYPE_SDCARD) && (native_vfs_mounted[self->device])) {
		esp_err_t ret = esp_vfs_fat_sdmmc_unmount();
	    if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to unmount filesystem on SDcard (%d).", ret);
			mp_raise_OSError(MP_EIO);
	    }
        ESP_LOGD(TAG, "Filesystem on SDcard unmounted.");
		native_vfs_mounted[self->device] = false;
	}
	else if (self->device == VFS_NATIVE_TYPE_SPIFLASH) {
        ESP_LOGW(TAG, "Filesystem on Flash cannot be unmounted.");
		mp_raise_OSError(MP_EIO);
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(native_vfs_umount_obj, native_vfs_umount);

//===============================================================
STATIC const mp_rom_map_elem_t native_vfs_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_mkfs), MP_ROM_PTR(&native_vfs_mkfs_obj) },
	{ MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&native_vfs_open_obj) },
	{ MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&native_vfs_ilistdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&native_vfs_mkdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&native_vfs_rmdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&native_vfs_chdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&native_vfs_getcwd_obj) },
	{ MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&native_vfs_remove_obj) },
	{ MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&native_vfs_rename_obj) },
	{ MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&native_vfs_stat_obj) },
	{ MP_ROM_QSTR(MP_QSTR_statvfs), MP_ROM_PTR(&native_vfs_statvfs_obj) },
	{ MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&native_vfs_mount_obj) },
	{ MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&native_vfs_umount_obj) },
};
STATIC MP_DEFINE_CONST_DICT(native_vfs_locals_dict, native_vfs_locals_dict_table);

//========================================
const mp_obj_type_t mp_native_vfs_type = {
	{ &mp_type_type },
	.name = MP_QSTR_VfsNative,
	.make_new = native_vfs_make_new,
	.locals_dict = (mp_obj_dict_t*)&native_vfs_locals_dict,
};

