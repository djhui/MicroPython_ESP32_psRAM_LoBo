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
 * spiffs VFS operations
 *
 * Author: LoBo (loboris@gmail.com / https://github.com/loboris)
 *
 * Part of this code is copied from or inspired by LUA-RTOS_ESP32 project:
 *
 * https://github.com/whitecatboard/Lua-RTOS-ESP32
 * IBEROXARXA SERVICIOS INTEGRALES, S.L. & CSS IBÉRICA, S.L.
 * Jaume Olivé (jolive@iberoxarxa.com / jolive@whitecatboard.org)
 *
 */


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "esp_log.h"

#ifdef IDF_USEHEAP
#include "esp_heap_caps.h"
#else
#include "esp_heap_alloc_caps.h"
#endif

#include <sys/stat.h>

#include "esp_vfs.h"
#include "esp_attr.h"
#include <errno.h>
#include "esp_partition.h"

#include "extmod/vfs_native.h"
#include <spiffs_vfs.h>
#include <spiffs.h>
#include <spiffs_config.h>
#include <esp_spiffs.h>
#include <spiffs_nucleus.h>
#include "list.h"
#include <sys/fcntl.h>
#include <sys/dirent.h>
#include "sdkconfig.h"


int spiffs_is_registered = 0;
int spiffs_is_mounted = 0;

QueueHandle_t spiffs_mutex = NULL;

static const char *TAG = "[spiffs_vfs]";

static int IRAM_ATTR vfs_spiffs_open(const char *path, int flags, int mode);
static ssize_t IRAM_ATTR vfs_spiffs_write(int fd, const void *data, size_t size);
static ssize_t IRAM_ATTR vfs_spiffs_read(int fd, void * dst, size_t size);
static int IRAM_ATTR vfs_spiffs_fstat(int fd, struct stat * st);
static int IRAM_ATTR vfs_spiffs_close(int fd);
static off_t IRAM_ATTR vfs_spiffs_lseek(int fd, off_t size, int mode);

typedef struct {
	DIR dir;
	spiffs_DIR spiffs_dir;
	char path[MAXNAMLEN + 1];
	struct dirent ent;
	uint8_t read_mount;
} vfs_spiffs_dir_t;

typedef struct {
	spiffs_file spiffs_file;
	char path[MAXNAMLEN + 1];
	uint8_t is_dir;
} vfs_spiffs_file_t;

typedef struct {
	time_t mtime;
	time_t ctime;
	time_t atime;
	uint8_t spare[SPIFFS_OBJ_META_LEN - (sizeof(time_t)*3)];
} spiffs_metadata_t;

static spiffs fs;
static struct list files;

static uint8_t *my_spiffs_work_buf;
static uint8_t *my_spiffs_fds;
static uint8_t *my_spiffs_cache;


/*
 * ########################################
 * file names/paths passed to the functions
 * do not contain '/spiffs' prefix
 * ########################################
 */

//--------------------------------------------------------------
void IRAM_ATTR spiffs_fs_stat(uint32_t *total, uint32_t *used) {
	if (SPIFFS_info(&fs, total, used) != SPIFFS_OK) {
		*total = 0;
		*used = 0;
	}
}

/*
 * Test if path corresponds to a directory. Return 0 if is not a directory,
 * 1 if it's a directory.
 *
 */
//---------------------------------------------
static int IRAM_ATTR is_dir(const char *path) {
    spiffs_DIR d;
    char npath[PATH_MAX + 1];
    int res = 0;

	ESP_LOGD(TAG, "is_dir() path '%s'", path);
	struct spiffs_dirent e;

    // Add /. to path
    strlcpy(npath, path, PATH_MAX);
    if (strcmp(path,"/") != 0) {
        strlcat(npath,"/.", PATH_MAX);
    } else {
    	strlcat(npath,".", PATH_MAX);
    }

    SPIFFS_opendir(&fs, "/", &d);
    while (SPIFFS_readdir(&d, &e)) {
        if (strncmp(npath, (const char *)e.name, strlen(npath)) == 0) {
            res = 1;
            break;
        }
    }

    SPIFFS_closedir(&d);

    return res;
}

/*
 * This function translate error codes from SPIFFS to errno error codes
 *
 */
//-------------------------------------------
static int IRAM_ATTR spiffs_result(int res) {
    switch (res) {
        case SPIFFS_OK:
        case SPIFFS_ERR_END_OF_OBJECT:
            return 0;

        case SPIFFS_ERR_NOT_FOUND:
        case SPIFFS_ERR_CONFLICTING_NAME:
            return ENOENT;

        case SPIFFS_ERR_NOT_WRITABLE:
        case SPIFFS_ERR_NOT_READABLE:
            return EACCES;

        case SPIFFS_ERR_FILE_EXISTS:
            return EEXIST;

        default:
            return res;
    }
}

//-----------------------------------------------------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_getstat(spiffs_file fd, spiffs_stat *st, spiffs_metadata_t *metadata) {
    int res = SPIFFS_fstat(&fs, fd, st);
    if (res == SPIFFS_OK) {
        // Get file's time information from metadata
        memcpy(metadata, st->meta, sizeof(spiffs_metadata_t));
	}
    return res;
}

// ## path does not contain '/spiffs' prefix !
//---------------------------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_open(const char *path, int flags, int mode) {
	int fd, result = 0, exists = 0;
	spiffs_stat stat;
	spiffs_metadata_t meta;

	ESP_LOGD(TAG, "open() path '%s'", path);
	// Allocate new file
	vfs_spiffs_file_t *file = calloc(1, sizeof(vfs_spiffs_file_t));
	if (!file) {
		errno = ENOMEM;
		return -1;
	}

    // Add file to file list. List index is file descriptor.
    int res = list_add(&files, file, &fd);
    if (res) {
    	free(file);
    	errno = res;
    	return -1;
    }

    // Check if file exists
    if (SPIFFS_stat(&fs, path, &stat) == SPIFFS_OK) exists = 1;

    // Make a copy of path
	strlcpy(file->path, path, MAXNAMLEN);

    // Open file
    spiffs_flags spiffs_mode = 0;

    // Translate flags to SPIFFS flags
    if (flags == O_RDONLY)
    	spiffs_mode |= SPIFFS_RDONLY;

    if (flags & O_WRONLY)
    	spiffs_mode |= SPIFFS_WRONLY;

    if (flags & O_RDWR)
    	spiffs_mode = SPIFFS_RDWR;

    if (flags & O_EXCL)
    	spiffs_mode |= SPIFFS_EXCL;

    if (flags & O_CREAT)
    	spiffs_mode |= SPIFFS_CREAT;

    if (flags & O_TRUNC)
    	spiffs_mode |= SPIFFS_TRUNC;

    if (is_dir(path)) {
        char npath[PATH_MAX + 1];

        // Add /. to path
        strlcpy(npath, path, PATH_MAX);
        if (strcmp(path,"/") != 0) {
            strlcat(npath,"/.", PATH_MAX);
        } else {
        	strlcat(npath,".", PATH_MAX);
        }

        // Open SPIFFS file
        file->spiffs_file = SPIFFS_open(&fs, npath, spiffs_mode, 0);
        if (file->spiffs_file < 0) {
            result = spiffs_result(fs.err_code);
        }

    	file->is_dir = 1;
    } else {
        // Open SPIFFS file
        file->spiffs_file = SPIFFS_open(&fs, path, spiffs_mode, 0);
        if (file->spiffs_file < 0) {
            result = spiffs_result(fs.err_code);
        }
    }

    if (result != 0) {
    	list_remove(&files, fd, 1);
    	errno = result;
    	return -1;
    }

    res = vfs_spiffs_getstat(file->spiffs_file, &stat, &meta);
	if (res == SPIFFS_OK) {
		// update file's time information
		meta.atime = time(NULL); // Get the system time to access time
		if (!exists) meta.ctime = meta.atime;
		if (spiffs_mode != SPIFFS_RDONLY) meta.mtime = meta.atime;
		SPIFFS_fupdate_meta(&fs, file->spiffs_file, &meta);
	}

    return fd;
}

//--------------------------------------------------------------------------------
static ssize_t IRAM_ATTR vfs_spiffs_write(int fd, const void *data, size_t size) {
	vfs_spiffs_file_t *file;
	int res;

    res = list_get(&files, fd, (void **)&file);
    if (res) {
		errno = EBADF;
		return -1;
    }

    if (file->is_dir) {
		errno = EBADF;
		return -1;
    }

    // Write SPIFFS file
	res = SPIFFS_write(&fs, file->spiffs_file, (void *)data, size);
	if (res >= 0) {
		return res;
	} else {
		res = spiffs_result(fs.err_code);
		if (res != 0) {
			errno = res;
			return -1;
		}
	}

	return -1;
}

//-------------------------------------------------------------------------
static ssize_t IRAM_ATTR vfs_spiffs_read(int fd, void * dst, size_t size) {
	vfs_spiffs_file_t *file;
	int res;

    res = list_get(&files, fd, (void **)&file);
    if (res) {
		errno = EBADF;
		return -1;
    }

    if (file->is_dir) {
		errno = EBADF;
		return -1;
    }

    // Read SPIFFS file
	res = SPIFFS_read(&fs, file->spiffs_file, dst, size);
	if (res >= 0) {
		return res;
	} else {
		res = spiffs_result(fs.err_code);
		if (res != 0) {
			errno = res;
			return -1;
		}

		// EOF
		return 0;
	}

	return -1;
}

//---------------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_fstat(int fd, struct stat * st) {
	vfs_spiffs_file_t *file;
    spiffs_stat stat;
	int res;
	spiffs_metadata_t meta;

    res = list_get(&files, fd, (void **)&file);
    if (res) {
		errno = EBADF;
		return -1;
    }

    // Set block size for this file system
    st->st_blksize = SPIFFS_LOG_PAGE_SIZE;

    // Get file/directory statistics
    res = vfs_spiffs_getstat(file->spiffs_file, &stat, &meta);
    if (res == SPIFFS_OK) {
        // Set file's time information from metadata
        st->st_mtime = meta.mtime;
        st->st_ctime = meta.ctime;
        st->st_atime = meta.atime;

    	st->st_size = stat.size;

	} else {
        st->st_mtime = 0;
        st->st_ctime = 0;
        st->st_atime = 0;
		st->st_size = 0;
	    errno = spiffs_result(fs.err_code);
		//printf("SPIFFS_STAT: error %d\r\n", res);
    	return -1;
    }

    // Test if it's a directory entry
    if (file->is_dir) st->st_mode = S_IFDIR;
    else st->st_mode = S_IFREG;

    return 0;
}

//---------------------------------------------
static int IRAM_ATTR vfs_spiffs_close(int fd) {
	vfs_spiffs_file_t *file;
	int res;

    res = list_get(&files, fd, (void **)&file);
    if (res) {
		errno = EBADF;
		return -1;
    }

	res = SPIFFS_close(&fs, file->spiffs_file);
	if (res) {
		res = spiffs_result(fs.err_code);
	}

	if (res < 0) {
		errno = res;
		return -1;
	}

	list_remove(&files, fd, 1);

	return 0;
}

//---------------------------------------------------------------------
static off_t IRAM_ATTR vfs_spiffs_lseek(int fd, off_t size, int mode) {
	vfs_spiffs_file_t *file;
	int res;

    res = list_get(&files, fd, (void **)&file);
    if (res) {
		errno = EBADF;
		return -1;
    }

    if (file->is_dir) {
		errno = EBADF;
		return -1;
    }

	int whence = SPIFFS_SEEK_CUR;

    switch (mode) {
        case SEEK_SET: whence = SPIFFS_SEEK_SET;break;
        case SEEK_CUR: whence = SPIFFS_SEEK_CUR;break;
        case SEEK_END: whence = SPIFFS_SEEK_END;break;
    }

    res = SPIFFS_lseek(&fs, file->spiffs_file, size, whence);
    if (res < 0) {
        res = spiffs_result(fs.err_code);
        errno = res;
        return -1;
    }

    return res;
}

//-------------------------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_stat(const char * path, struct stat * st) {
	int fd;
	int res;
	ESP_LOGD(TAG, "stat() path '%s'", path);
	fd = vfs_spiffs_open(path, 0, 0);
	res = vfs_spiffs_fstat(fd, st);
	vfs_spiffs_close(fd);

	return res;
}

//--------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_unlink(const char *path) {
    char npath[PATH_MAX + 1];

	ESP_LOGD(TAG, "unlink() path '%s'", path);
    strlcpy(npath, path, PATH_MAX);

    if (is_dir(path)) {
        // Check if  directory is empty
    	int nument = 0;
    	sprintf(npath, "/spiffs");
    	strlcat(npath, path, PATH_MAX);

    	DIR *dir = opendir(npath);
        if (dir) {
            struct dirent *ent;
			// Read directory entries
			while ((ent = readdir(dir)) != NULL) {
				nument++;
			}
        }
        else {
        	errno = ENOTEMPTY;
        	return -1;
        }
        closedir(dir);

        if (nument > 0) {
        	// Directory not empty, cannot remove
        	errno = ENOTEMPTY;
        	return -1;
        }

        strlcpy(npath, path, PATH_MAX);
    	// Add /. to path
	    if (strcmp(path,"/") != 0) {
	        strlcat(npath,"/.", PATH_MAX);
	    }
	}

    // Open SPIFFS file
	spiffs_file FP = SPIFFS_open(&fs, npath, SPIFFS_RDWR, 0);
    if (FP < 0) {
    	errno = spiffs_result(fs.err_code);
    	return -1;
    }

    // Remove SPIFSS file
    if (SPIFFS_fremove(&fs, FP) < 0) {
        errno = spiffs_result(fs.err_code);
    	SPIFFS_close(&fs, FP);
    	return -1;
    }

	SPIFFS_close(&fs, FP);

	return 0;
}

//------------------------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_rename(const char *src, const char *dst) {
    if (SPIFFS_rename(&fs, src, dst) < 0) {
    	errno = spiffs_result(fs.err_code);
    	return -1;
    }

    return 0;
}

//----------------------------------------------------------
static DIR* IRAM_ATTR vfs_spiffs_opendir(const char* name) {
	struct stat st;

	ESP_LOGD(TAG, "opendir() name '%s'", name);
    if (strcmp(name, "/") != 0) {
    	// Not on root
    	if (vfs_spiffs_stat(name, &st)) {
    		// Not found
    		errno = ENOENT;
    		return NULL;
        }
    	if (!S_ISDIR(st.st_mode)) {
    		// Not a directory
    		errno = ENOTDIR;
    		return NULL;
        }
    }

	vfs_spiffs_dir_t *dir = calloc(1, sizeof(vfs_spiffs_dir_t));

	if (!dir) {
		errno = ENOMEM;
		return NULL;
	}

	if (!SPIFFS_opendir(&fs, name, &dir->spiffs_dir)) {
        free(dir);
        errno = spiffs_result(fs.err_code);
        return NULL;
    }

	strlcpy(dir->path, name, MAXNAMLEN);

	return (DIR *)dir;
}

//-------------------------------------------------------------
static struct dirent* IRAM_ATTR vfs_spiffs_readdir(DIR* pdir) {
    int res = 0, len = 0, entries = 0;

	vfs_spiffs_dir_t* dir = (vfs_spiffs_dir_t*) pdir;

    ESP_LOGD(TAG, "readdir() dir '%s'", dir->path);
	struct spiffs_dirent e;
    struct spiffs_dirent *pe = &e;

    struct dirent *ent = &dir->ent;

    char *fn;

    // Clear current dirent
    memset(ent,0,sizeof(struct dirent));

    if (!dir->read_mount) {
    	/*
		// If this is the first call to readdir for pdir, and
		// directory is the root path, return the mounted point if any
    	if (strcmp(dir->path,"/") == 0) {
			strlcpy(ent->d_name, "/spiffs", PATH_MAX);
			ent->d_type = DT_DIR;
			dir->read_mount = 1;

			return ent;
    	}
		*/
    	dir->read_mount = 1;
    }

    // Search for next entry
    for(;;) {
        // Read directory
        pe = SPIFFS_readdir(&dir->spiffs_dir, pe);
        if (!pe) {
            res = spiffs_result(fs.err_code);
            errno = res;
            break;
        }

        // Break condition
        if (pe->name[0] == 0) break;

        // Get name and length
        fn = (char *)pe->name;
        len = strlen(fn);

        // Get entry type and size
        ent->d_type = DT_REG;

        if (len >= 2) {
            if (fn[len - 1] == '.') {
                if (fn[len - 2] == '/') {
                    ent->d_type = DT_DIR;

                    fn[len - 2] = '\0';

                    len = strlen(fn);

                    // Skip root dir
                    if (len == 0) {
                        continue;
                    }
                }
            }
        }

        // Skip entries not belonged to path
        if (strncmp(fn, dir->path, strlen(dir->path)) != 0) {
            continue;
        }

        if (strlen(dir->path) > 1) {
            if (*(fn + strlen(dir->path)) != '/') {
                continue;
            }
        }

        // Skip root directory
        fn = fn + strlen(dir->path);
        len = strlen(fn);
        if (len == 0) {
            continue;
        }

        // Skip initial /
        if (len > 1) {
            if (*fn == '/') {
                fn = fn + 1;
                len--;
            }
        }

        // Skip subdirectories
        if (strchr(fn,'/')) {
            continue;
        }

        //ent->d_fsize = pe->size;

        strlcpy(ent->d_name, fn, MAXNAMLEN);

        entries++;

        break;
    }

    if (entries > 0) {
    	return ent;
    } else {
    	return NULL;
    }
}

//--------------------------------------------------
static int IRAM_ATTR vfs_piffs_closedir(DIR* pdir) {
	vfs_spiffs_dir_t* dir = (vfs_spiffs_dir_t*) pdir;
    ESP_LOGD(TAG, "closedir() dir '%s'", dir->path);
	int res;

	if (!pdir) {
		errno = EBADF;
		return -1;
	}

	if ((res = SPIFFS_closedir(&dir->spiffs_dir)) < 0) {
		errno = spiffs_result(fs.err_code);;
		return -1;
	}

	free(dir);

    return 0;
}

//--------------------------------------------------------------------
static int IRAM_ATTR vfs_spiffs_mkdir(const char *path, mode_t mode) {
    char npath[PATH_MAX + 1];
    int res;

    ESP_LOGD(TAG, "mkdir() dir '%s'", path);
    // Add /. to path
    strlcpy(npath, path, PATH_MAX);
    if ((strcmp(path,"/") != 0) && (strcmp(path,"/.") != 0)) {
        strlcat(npath,"/.", PATH_MAX);
    }

    spiffs_file fd = SPIFFS_open(&fs, npath, SPIFFS_CREAT, 0);
    if (fd < 0) {
        res = spiffs_result(fs.err_code);
        errno = res;
        return -1;
    }

    if (SPIFFS_close(&fs, fd) < 0) {
        res = spiffs_result(fs.err_code);
        errno = res;
        return -1;
    }

	spiffs_metadata_t meta;
	meta.atime = time(NULL); // Get the system time to access time
	meta.ctime = meta.atime;
	meta.mtime = meta.atime;
	SPIFFS_update_meta(&fs, npath, &meta);

    return 0;
}


static const char tag[] = "[SPIFFS]";

//==================
int spiffs_mount() {

	if (!spiffs_is_registered) return 0;
	if (spiffs_is_mounted) return 1;

	spiffs_config cfg;
    int res = 0;
    int retries = 0;
    int err = 0;

	#if MICROPY_SDMMC_SHOW_INFO
    printf("\nMounting SPIFFS files system\n");
	#endif

    uint32_t part_start = CONFIG_MICROPY_INTERNALFS_START;
    esp_partition_t *data_partition = (esp_partition_t *)esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "MicroPython");
    if (data_partition != NULL) {
    	if (part_start < (data_partition->address + data_partition->size)) {
    		printf("WARNING: SPIFFS partition start address configured in application space!\n");
    		printf("         Start address changed from 0x%04x to 0x%04x\n", part_start, data_partition->address + data_partition->size);
    		printf("         Change the start address using menuconfig\n");
    		part_start = data_partition->address + data_partition->size;
    	}
    }

    cfg.phys_addr 		 = part_start;
    cfg.phys_size 		 = CONFIG_MICROPY_INTERNALFS_SIZE*1024;
    cfg.phys_erase_block = SPIFFS_ERASE_SIZE;
    cfg.log_page_size    = SPIFFS_LOG_PAGE_SIZE;
    cfg.log_block_size   = SPIFFS_LOG_BLOCK_SIZE;

	cfg.hal_read_f  = (spiffs_read)low_spiffs_read;
	cfg.hal_write_f = (spiffs_write)low_spiffs_write;
	cfg.hal_erase_f = (spiffs_erase)low_spiffs_erase;

	#ifdef IDF_USEHEAP
	my_spiffs_work_buf = heap_caps_malloc(cfg.log_page_size * 8, MALLOC_CAP_DMA);
	#else
	my_spiffs_work_buf = pvPortMallocCaps(cfg.log_page_size * 8, MALLOC_CAP_DMA);
	#endif
    if (!my_spiffs_work_buf) {
    	err = 1;
    	goto err_exit;
    }

    int fds_len = sizeof(spiffs_fd) * SPIFFS_TEMPORAL_CACHE_HIT_SCORE;
	#ifdef IDF_USEHEAP
    my_spiffs_fds = heap_caps_malloc(fds_len, MALLOC_CAP_DMA);
	#else
    my_spiffs_fds = pvPortMallocCaps(fds_len, MALLOC_CAP_DMA);
	#endif
    if (!my_spiffs_fds) {
        free(my_spiffs_work_buf);
    	err = 2;
    	goto err_exit;
    }

    int cache_len = cfg.log_page_size * SPIFFS_TEMPORAL_CACHE_HIT_SCORE;
	#ifdef IDF_USEHEAP
    my_spiffs_cache = heap_caps_malloc(cache_len, MALLOC_CAP_DMA);
	#else
	my_spiffs_cache = pvPortMallocCaps(cache_len, MALLOC_CAP_DMA);
	#endif
    if (!my_spiffs_cache) {
        free(my_spiffs_work_buf);
        free(my_spiffs_fds);
    	err = 3;
    	goto err_exit;
    }

	#if MICROPY_SDMMC_SHOW_INFO
	printf("----------------\n");
	printf("  Start address: 0x%x; Size %d KB\n", cfg.phys_addr, cfg.phys_size / 1024);
	printf("    Work buffer: %4d B @ %p\n", cfg.log_page_size * 8, my_spiffs_work_buf);
	printf("     FDS buffer: %4d B @ %p\n", sizeof(spiffs_fd) * SPIFFS_TEMPORAL_CACHE_HIT_SCORE, my_spiffs_fds);
	printf("     Cache size: %4d B @ %p\n", cfg.log_page_size * SPIFFS_TEMPORAL_CACHE_HIT_SCORE, my_spiffs_cache);
	printf("----------------\n");
	#endif

    while (retries < 2) {
		res = SPIFFS_mount(
				&fs, &cfg, my_spiffs_work_buf, my_spiffs_fds,
				fds_len, my_spiffs_cache, cache_len, NULL
		);

		if (res < 0) {
			if (fs.err_code == SPIFFS_ERR_NOT_A_FS) {
				#if MICROPY_SDMMC_SHOW_INFO
				printf("** No file system detected, formating... **\n");
				#endif
				SPIFFS_unmount(&fs);
				res = SPIFFS_format(&fs);
				if (res < 0) {
			        free(my_spiffs_work_buf);
			        free(my_spiffs_fds);
			        free(my_spiffs_cache);
					#if MICROPY_SDMMC_SHOW_INFO
					printf("Format error\n");
					#endif
					goto exit;
				}
			}
			else {
		        free(my_spiffs_work_buf);
		        free(my_spiffs_fds);
		        free(my_spiffs_cache);
				#if MICROPY_SDMMC_SHOW_INFO
				printf("Error mounting fs (%d)\n", res);
				#endif
				goto exit;
			}
		}
		else break;
		retries++;
    }

    if (retries > 1) {
        free(my_spiffs_work_buf);
        free(my_spiffs_fds);
        free(my_spiffs_cache);
		#if MICROPY_SDMMC_SHOW_INFO
		printf("Can't mount\n");
		#endif
		goto exit;
    }

    list_init(&files, 0);

	#if MICROPY_SDMMC_SHOW_INFO
    printf("Mounted.\n");
	uint32_t total, used;
	spiffs_fs_stat(&total, &used);
	printf("----------------\n\n");
	printf("Filesystem size: %d B\n", total);
	printf("           Used: %d B\n", used);
	printf("           Free: %d B\n", total-used);
	printf("----------------\n\n");
	#endif

    spiffs_is_mounted = 1;
    return 1;

err_exit:
	#if MICROPY_SDMMC_SHOW_INFO
	printf("SPIFFS: Error allocating fs structures (%d)\n", err);
	#endif
exit:
	esp_vfs_unregister(VFS_NATIVE_MOUNT_POINT);
	spiffs_is_registered = 0;
	return 0;
}

//==========================
void vfs_spiffs_register() {

	if (spiffs_is_registered) return;

	if (spiffs_mutex == NULL) {
		spiffs_mutex = xSemaphoreCreateMutex();
		if (spiffs_mutex == NULL) {
			#if MICROPY_SDMMC_SHOW_INFO
			printf("Error creating SPIFFS mutex\n");
			#endif
			return;
		}
	}

	esp_vfs_t vfs = {
        .fd_offset = 0,
        .flags = ESP_VFS_FLAG_DEFAULT,
        .write = &vfs_spiffs_write,
        .open = &vfs_spiffs_open,
        .fstat = &vfs_spiffs_fstat,
        .close = &vfs_spiffs_close,
        .read = &vfs_spiffs_read,
        .lseek = &vfs_spiffs_lseek,
        .stat = &vfs_spiffs_stat,
        .link = NULL,
        .unlink = &vfs_spiffs_unlink,
        .rename = &vfs_spiffs_rename,
		.mkdir = &vfs_spiffs_mkdir,
		.opendir = &vfs_spiffs_opendir,
		.readdir = &vfs_spiffs_readdir,
		.closedir = &vfs_piffs_closedir,
    };

	#if MICROPY_SDMMC_SHOW_INFO
    printf("Registering SPIFFS file system to VFS\n");
	#endif
    esp_err_t res = esp_vfs_register(VFS_NATIVE_MOUNT_POINT, &vfs, NULL);
    if (res != ESP_OK) {
		#if MICROPY_SDMMC_SHOW_INFO
        printf("  Error, SPIFFS file system not registered\n");
		#endif
        return;
    }
	spiffs_is_registered = 1;

	spiffs_mount();
}

//=============================
int spiffs_unmount(int unreg) {

	if (!spiffs_is_mounted) return 0;

	SPIFFS_unmount(&fs);
    spiffs_is_mounted = 0;

    if (unreg) {
    	esp_vfs_unregister(VFS_NATIVE_MOUNT_POINT);
    	spiffs_is_registered = 0;
    }
	return 1;
}
