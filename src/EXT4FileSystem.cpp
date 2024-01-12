/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * EXT4FileSystem.cpp adapted from FATFileSystem.cpp
 * Copyright (c) 2024, Warren Watson.
 */

#include "platform/mbed_debug.h"
#include "platform/mbed_critical.h"
#include "EXT4FileSystem.h"

#include <errno.h>
#include <stdlib.h>

namespace mbed {

using namespace mbed;

// Global access to block device from ext4 driver
static mbed::BlockDevice *_extfs[MAX_MOUNT_POINTS] = {0};
static SingletonPtr<PlatformMutex> _extfs_mutex;

// Remap directory entry types.
static  uint8_t remap_dir_type(uint8_t type)
{
	switch (type) {
	case EXT4_DE_UNKNOWN:
		return DT_UNKNOWN;
	case EXT4_DE_REG_FILE:
		return DT_REG;
	case EXT4_DE_DIR:
		return DT_DIR;
	case EXT4_DE_CHRDEV:
		return DT_CHR;
	case EXT4_DE_BLKDEV:
		return DT_BLK;
	case EXT4_DE_FIFO:
		return DT_FIFO;
	case EXT4_DE_SOCK:
		return DT_SOCK;
	case EXT4_DE_SYMLINK:
		return DT_LNK;
	default:
		break;
	}
	return DT_UNKNOWN;
}

// Helper class for deferring operations when variable falls out of scope
template <typename T>
class Deferred {
public:
    T _t;
    Callback<void(T)> _ondefer;

    Deferred(const Deferred &);
    Deferred &operator=(const Deferred &);

public:
    Deferred(T t, Callback<void(T)> ondefer = nullptr)
        : _t(t), _ondefer(ondefer)
    {
    }

    operator T()
    {
        return _t;
    }

    ~Deferred()
    {
        if (_ondefer) {
            _ondefer(_t);
        }
    }
};

static void dodelete(const char *data)
{
    delete[] data;
}

// Adds prefix needed internally by fatfs, this can be avoided for the first fatfs
// (id 0) otherwise a prefix of "id:/" is inserted in front of the string.
static Deferred<const char *> ext_path_prefix(int id, const char *path)
{
 
    // We can avoid dynamic allocation when only one fatfs is in use 
//    if (id == 0) {
//        return path;
//    } **************** Does not work with none fatfs mounts (ext4). ********************

    // Prefix path with id, will look something like /sda1/hi/hello/filehere.txt
    char *buffer = new char[strlen("/sda1/") + strlen(path) + 1];
    if (!buffer) {
        return NULL;
    }
    sprintf(buffer, "/%s/%s",mpName[id],path);
    return Deferred<const char *>(buffer, dodelete);
}

// Filesystem implementation (See EXT4FileSystem.h)
EXT4FileSystem::EXT4FileSystem(const char *name, BlockDevice *bd) : FileSystem(name), _fs(), _id(-1)
{
    if (bd) {
        mount(bd);
    }
}

EXT4FileSystem::~EXT4FileSystem()
{
    // nop if unmounted
    unmount();
}

int EXT4FileSystem::mount(BlockDevice *bd)
{
    // requires duplicate definition to allow virtual overload to work
    return mount(bd, true);
}

int EXT4FileSystem::mount(BlockDevice *bd, bool mount)
{
  int res = 0;

  lock();
  if(_id != -1) {
    unlock();
    return -EINVAL;
  }
  // Find an free entry in block device list (_extfs[]) and assign an
  // instance of 'bd' (USBHostMSD) to it.
  for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
    if(!_extfs[i]) { // If _extfs[i] == 0 then
      _id = i;       // assign index number to _id.
      _extfs[_id] = bd; // Pointer to an instance of USBHostMSD.
      strcpy(_fsid,getName()); // Get name (sda1, sda2...).
      debug_if(FFS_DBG, "Mounting [%s] on ext4fs drive [%s]\n", getName(), _fsid);
      res = _fs.mount(_id); // _fs is instance of GIGAext4FS driver.
      unlock();
      return res;
    }
  }
  unlock();
  return -ENOMEM;
}

int EXT4FileSystem::unmount()
{
  // Get pointer to ext4 mount list.
  bd_mounts_t *ml = _fs.get_mount_list();

  lock();
  if(_id == -1) {
    unlock();
    return -EINVAL;
  }

  int res = _fs.lwext_umount(_id);
  int err = _extfs[_id]->deinit();

  if(res != EOK) { // If device was removed before unmounting then:
    printf("ext4 partition %d unmount failed: %d...\n",_id,err);
    printf("\n**************** A BAD THING JUST HAPPENED!!! ****************\n");
    printf("When unmounting an EXT4 drive data is written back to the drive.\n");
    printf("To avoid losing cached data, unmount drive before removing device.\n");
    printf("Unmounting and removing drive (%d) partition (%d) from drive list. \n",0, _id);
    printf("*****************************************************************\n\n");
    // Manualy unmount and unregister EXT4 drive. Presumably was not done before removed.
	ext4_umount(ml[_id].pname); // Will return errors, but still needed.
	ext4_device_unregister(ml[_id].pname); // Ditto.
    _fs.clr_ML_entry(_id); // Clear mount list entries.
	_fs.clr_BDL_entry(0);  // Remove USB drive entries.
	_extfs[_id] = NULL;
    _id = -1;
    unlock();
    return res;
  }
  if(err) {
    _extfs[_id] = NULL;
    _id = -1;
    unlock();
    return err;
  }
  _extfs[_id] = NULL;
  _id = -1;
  unlock();
  return err;
}

/*
int EXT4FileSystem::mount(BlockDevice *bd, bool mount)
{
  int res = 0;

   lock();
   if (_id != -1) {
       unlock();
        return -EINVAL;
    }
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (!_extfs[i]) {
            _id = i;
            _extfs[_id] = bd;
			strcpy(_fsid,mpName[_id]);
            debug_if(FFS_DBG, "Mounting [%s] on ext4fs drive [%s]\n", getName(), _fsid);
            res = _fs.mount(_id);
			unlock();
            return res;
        }
    }
    unlock();
    return -ENOMEM;
}

int EXT4FileSystem::unmount()
{
    lock();

    if (_id == -1) {
        unlock();
        return -EINVAL;
    }
    int res = _fs.lwext_umount(_id);
    int err = _extfs[_id]->deinit();
    if (res != EOK) {
        _id = -1;
        unlock();
        return res;
    }
    if (err) {
        _id = -1;
        unlock();
        return err;
    }
    _extfs[_id] = NULL;
    _id = -1;
    unlock();
    return err;
}
*/
#if MBED_CONF_FAT_CHAN_FF_USE_MKFS
/* See http://elm-chan.org/fsw/ff/en/mkfs.html for details of f_mkfs() and
 * associated arguments. */
int EXT4FileSystem::format(BlockDevice *bd, bd_size_t cluster_size)
{
/*
    EXT4FileSystem fs;
    fs.lock();

    int err = bd->init();
    if (err) {
        fs.unlock();
        return err;
    }

    // erase first handful of blocks
    bd_size_t header = 2 * bd->get_erase_size();
    err = bd->erase(0, header);
    if (err) {
        bd->deinit();
        fs.unlock();
        return err;
    }

    if (bd->get_erase_value() < 0) {
        // erase is unknown, need to write 1s
        bd_size_t program_size = bd->get_program_size();
        void *buf = malloc(program_size);
        if (!buf) {
            bd->deinit();
            fs.unlock();
            return -ENOMEM;
        }

        memset(buf, 0xff, program_size);

        for (bd_addr_t i = 0; i < header; i += program_size) {
            err = bd->program(buf, i, program_size);
            if (err) {
                free(buf);
                bd->deinit();
                fs.unlock();
                return err;
            }
        }

        free(buf);
    }

    // trim entire device to indicate it is unneeded
    err = bd->trim(0, bd->size());
    if (err) {
        bd->deinit();
        fs.unlock();
        return err;
    }

    err = bd->deinit();
    if (err) {
        fs.unlock();
        return err;
    }

    err = fs.mount(bd, false);
    if (err) {
        fs.unlock();
        return err;
    }

    // Logical drive number, Partitioning rule, Allocation unit size (bytes per cluster)
    MKFS_PARM opt;
    int res;
    opt.fmt     = (FM_ANY | FM_SFD);
    opt.n_fat   = 0U;
    opt.align   = 0U;
    opt.n_root  = 0U;
    opt.au_size = (DWORD)cluster_size;

    res = f_mkfs((const TCHAR *)fs._fsid, &opt, NULL, FF_MAX_SS);
    if (res != EOK) {
        fs.unmount();
        fs.unlock();
        return res;
    }

    err = fs.unmount();
    if (err) {
        fs.unlock();
        return err;
    }

    fs.unlock();
*/
    return 0; // Not setup yet.
}

int EXT4FileSystem::reformat(BlockDevice *bd, int allocation_unit)
{
/*
    lock();
    if (_id != -1) {
        if (!bd) {
            bd = _extfs[_id];
        }

        int err = unmount();
        if (err) {
            unlock();
            return err;
        }
    }

    if (!bd) {
        unlock();
        return -ENODEV;
    }

    int err = EXT4FileSystem::format(bd, allocation_unit);
    if (err) {
        unlock();
        return err;
    }

    err = mount(bd);
    unlock();
    return err;
*/
return 0; // Not setup yet.
}
#endif

int EXT4FileSystem::remove(const char *path)
{
    Deferred<const char *> fpath = ext_path_prefix(_id, path);
	stat_t buf;
    int res = EOK;
    _fs.lwext_stat(fpath,&buf);
    
    lock();
    if(S_ISREG(buf.st_mode)) { // Dir or File?
      res = ext4_fremove(fpath); // Remove File.
    } else { 
	  res = ext4_dir_rm(fpath); // Remove Dir.
	}
    res = ext4_cache_flush(fpath);
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "ext4_unlink() failed: %d\n", res);
//        if (res == FR_DENIED) {
//            return -ENOTEMPTY;
//        }
    }
    return res;
}

#if !defined(TARGET_PORTENTA_H7_M7) || !defined(MCUBOOT_BOOTLOADER_BUILD)
int EXT4FileSystem::rename(const char *oldpath, const char *newpath)
{
    Deferred<const char *> oldfpath = ext_path_prefix(_id, oldpath);
    Deferred<const char *> newfpath = ext_path_prefix(_id, newpath);

    lock();
    int res = ext4_frename(oldfpath, newfpath);
    res = ext4_cache_flush(newfpath);
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "rename() failed: %d\n", res);
    }
    return res;
}

int EXT4FileSystem::mkdir(const char *path, mode_t mode)
{
    Deferred<const char *> fpath = ext_path_prefix(_id, path);
    lock();
    int res = ext4_dir_mk(fpath);
    res = ext4_cache_flush(fpath);
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "mkdir() failed: %d\n", res);
    }
    return res;
}
#endif

int EXT4FileSystem::stat(const char *path, struct stat *st)
{
    Deferred<const char *> fpath = ext_path_prefix(_id, path);

    stat_t buf;

    lock();
    int res = _fs.lwext_stat(fpath, &buf);
    if (res != EOK) {
        unlock();
        return -res;
    }

    st->st_size = buf.st_size;
    st->st_mode = 0;
    st->st_mode |= buf.st_mode;
    st->st_mode |= (S_IRWXU | S_IRWXG | S_IRWXO);
    unlock();

    return 0;
}

int EXT4FileSystem::statvfs(const char *path, struct statvfs *buf)
{

    memset(buf, 0, sizeof(struct statvfs));

    lock();
    int res = ext4_mount_point_stats(path, &stats);
    if (res != EOK) {
        unlock();
        return res;
    }

    buf->f_bsize = stats.block_size;
    buf->f_frsize = stats.block_size;
    buf->f_blocks = stats.blocks_count;
    buf->f_bfree = stats.free_blocks_count;
    buf->f_bavail = buf->f_bfree; // Same amount as super user.
//    buf->f_files = stats.inodes_count;
//    buf->f_ffree = stats.free_inodes_count;
//    buf->f_favail = buf->ffree; // Same amount as super user.
    buf->f_namemax = 256;

    unlock();
    return 0;
}

void EXT4FileSystem::lock()
{
    _extfs_mutex->lock();
}

void EXT4FileSystem::unlock()
{
    _extfs_mutex->unlock();
}


////// File operations //////
int EXT4FileSystem::file_open(fs_file_t *file, const char *path, int flags)
{

    debug_if(FFS_DBG, "open(%s) on filesystem [%s], drv [%d]\n", path, getName(), _id);

    ext4_file *fh = new ext4_file;
    Deferred<const char *> fpath = ext_path_prefix(_id, path);

    // POSIX flags -> FatFS open mode
    char openmode[3];
    if (flags & O_RDWR) {
        strcpy(openmode, "r+"); //FA_READ | FA_WRITE;
    } else if (flags & O_WRONLY) {
        strcpy(openmode, "w"); //FA_WRITE;
    } else {
        strcpy(openmode, "r"); //FA_READ;
    }

    if (flags & O_CREAT) {
        if (flags & O_TRUNC) {
        strcpy(openmode, "w+"); // openmode |= FA_CREATE_ALWAYS;
        } else {
        strcpy(openmode, "a"); // openmode |= FA_OPEN_ALWAYS;
        }
    }

    if (flags & O_APPEND) {
        strcpy(openmode, "a+"); // openmode |= FA_OPEN_APPEND;
    }

    lock();
    int res = ext4_fopen(fh, fpath, (const char *)openmode);

    if (res != EOK) {
        unlock();
        debug_if(FFS_DBG, "ex4_fopen('w') failed: %d\n", res);
        delete fh;
        return res;
    }

    unlock();

    *file = fh;

    return 0;
}

int EXT4FileSystem::file_close(fs_file_t file)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();
    int res = ext4_fclose(fh);
    unlock();

    delete fh;
    return res;
}

ssize_t EXT4FileSystem::file_read(fs_file_t file, void *buffer, size_t len)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();
    UINT n;
    int res = ext4_fread(fh, buffer, len, &n);
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "f_read() failed: %d\n", res);
        return res;
    } else {
        return n;
    }
}

ssize_t EXT4FileSystem::file_write(fs_file_t file, const void *buffer, size_t len)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();
    UINT n;
    int res = ext4_fwrite(fh, buffer, len, &n);
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "f_write() failed: %d", res);
        return res;
    } else {
        return n;
    }
}

int EXT4FileSystem::file_sync(fs_file_t file)
{
// Not setup yet.
//    ext4_file *fh = static_cast<ext4_file *>(file);
printf("file_sync()\n");
    lock();
    int res = EOK; //f_sync(fh);
//    int res 
    unlock();
    if (res != EOK) {
        debug_if(FFS_DBG, "f_sync() failed: %d\n", res);
    }
    return res;
}

off_t EXT4FileSystem::file_seek(fs_file_t file, off_t offset, int whence)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();
    int res = ext4_fseek(fh, offset,whence);
    off_t noffset = fh->fpos;
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "lseek failed: %d\n", res);
        return res;
    } else {
        return noffset;
    }
}

// This call seems to be deprecated!!!!!
// Now uses "file_seek()" above for ftell().
off_t EXT4FileSystem::file_tell(fs_file_t file)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();
   off_t res = ext4_ftell(fh);
    unlock();
    return res;
}

off_t EXT4FileSystem::file_size(fs_file_t file)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();
    off_t res = ext4_fsize(fh);
    unlock();

    return res;
}

int EXT4FileSystem::file_truncate(fs_file_t file, off_t length)
{
    ext4_file *fh = static_cast<ext4_file *>(file);

    lock();

    int res = ext4_ftruncate(fh, length);
    if (res) {
        unlock();
        return res;
    }
    unlock();
    return 0;
}


////// Dir operations //////
int EXT4FileSystem::dir_open(fs_dir_t *dir, const char *path)
{
    ext4_dir *dh = new ext4_dir;
    Deferred<const char *> fpath = ext_path_prefix(_id, path);
    lock();
    int res = ext4_dir_open(dh, fpath);
    unlock();

    if (res != EOK) {
        debug_if(FFS_DBG, "f_opendir() failed: %d\n", res);
        delete dh;
        return -res;  // Must be negative to show failure.
    }

    *dir = dh;
    return EOK;
}

int EXT4FileSystem::dir_close(fs_dir_t dir)
{

    ext4_dir *dh = static_cast<ext4_dir *>(dir);

    lock();
    int res = ext4_dir_close(dh);
    unlock();

    delete dh;
    return res;
}

//************************************************************
// This is the dirent struct found in platform/mbed_retarget.h
//************************************************************
//   struct dirent {
//     char d_name[NAME_MAX + 1]; ///< Name of file
//     uint8_t d_type;            ///< Type of file
//   };
//************************************************************
// This is the ext4_direntry struct found in ext4.h
//************************************************************
//   typedef struct ext4_direntry {
//     uint32_t inode;
//	   uint16_t entry_length;
//	   uint8_t name_length;
//	   uint8_t inode_type;
//	   uint8_t name[255];
//   } ext4_direntry;
//************************************************************
// We will need to copy two of ext4_direntry terms to dirent:
//  name[] --> d_name[] and inode_type --> d_type.
//************************************************************
ssize_t EXT4FileSystem::dir_read(fs_dir_t dir, struct dirent *ent)
{

    ext4_dir *dh = static_cast<ext4_dir *>(dir);
    const ext4_direntry *de = 0;

    lock();
#if DONT_SHOW_DOT_FILES
    do {
	  if ((de = ext4_dir_entry_next(dh)) == NULL) return 0; // No entries.
	} while (strcmp((const char *)de->name, ".") == 0 ||
	         strcmp((const char *)de->name, "..") == 0);
#else
	de = ext4_dir_entry_next(dh);
#endif
    unlock();

    if (de == NULL) {
        return 0;
    } else if (de->name[0] == 0) {
        return 0;
    }

    ent->d_type = remap_dir_type(de->inode_type);
	strcpy(ent->d_name, (const char *)de->name);
    return 1;
}

void EXT4FileSystem::dir_seek(fs_dir_t dir, off_t offset)
{
    ext4_dir *dh = static_cast<ext4_dir *>(dir);
    off_t dptr = static_cast<off_t>(dh->next_off);
    const ext4_direntry *de = 0;

    lock();
    if (offset < dptr) {
        ext4_dir_entry_rewind(dh);
    }
    while (dptr < offset) {
	  if ((de = ext4_dir_entry_next(dh)) == NULL) break; // Last entry.
      if (de->name[0] == 0) break;
      dptr = dh->next_off;
    }
    unlock();
}

off_t EXT4FileSystem::dir_tell(fs_dir_t dir)
{

    ext4_dir *dh = static_cast<ext4_dir *>(dir);

    lock();
    off_t offset = dh->next_off;
    unlock();

    return offset;
}

void EXT4FileSystem::dir_rewind(fs_dir_t dir)
{

    ext4_dir *dh = static_cast<ext4_dir *>(dir);

    lock();
    ext4_dir_entry_rewind(dh);
    unlock();

}

const char *EXT4FileSystem::getVolumeLabel(void) {
  return _fs.getVolumeLabel();
}

} // namespace mbed
