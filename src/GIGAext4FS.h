/* GIGAext4FS library compatibility wrapper for use of lwext4 on GIGA R1 as adapted
 * from LWext4 Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 * Copyright (c) 2022-2024, Warren Watson.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __GIGA_EXT4FS_H__
#define __GIGA_EXT4FS_H__

#ifdef __cplusplus
  extern "C" {
#endif
#include "ext4/ext4_config.h"
#include "ext4/ext4_mbr.h"
#include "ext4/ext4.h"
#include "ext4/ext4_fs.h"
#include "ext4/ext4_mkfs.h"
#ifdef __cplusplus
}
#endif

#include "PlatformMutex.h"

//#define EXT4_DBG 1

// Ext2/3/4 File system type in MBR.
#define EXT4_TYPE 0x83
// FAT32 File system type in MBR.
#define FAT32_TYPE 0x0b

#define EXT4_BLOCK_SIZE 512

enum {USB_TYPE=0, SD_TYPE=CONFIG_EXT4_BLOCKDEVS_COUNT-1}; // what type of block device

/* controls for block devices */
#define DEVICE_CTRL_GET_SECTOR_SIZE  0
#define DEVICE_CTRL_GET_BLOCK_SIZE   1
#define DEVICE_CTRL_GET_SECTOR_COUNT 2
#define DEVICE_CTRL_GET_DISK_SIZE    3

#define TO_LWEXT_PATH(f) (&((f)[0]))

// Low level device name
#define deviceName "sda"

#define FMR "r+"
#define FMWC "w+"
#define FMWA "a+"

// Only 4 mount points avaialble at this time.
#define MAX_MOUNT_POINTS 4

// Partition device names.
const char mpName[MAX_MOUNT_POINTS][5] = 
{	"sda1",
	"sda2",
	"sda3",
	"sda4"
};

//******************************************************************************
// The following structures used for low level block devices and mount info.
//******************************************************************************
// Physical Block Device.
typedef struct block_device {
	int dev_id = -1;
	char name[32];
	USBHostMSD *pDrive;
	struct ext4_blockdev *pbdev; // &_ext4_bd ...&_ext4_bd3
	bool connected = false;
}block_device_t;

//  Mount Point Info.
typedef struct bd_mounts {
	bool available = false;
	char volName[32];
	char pname[32];
	struct ext4_blockdev partbdev;
    uint8_t pt = 0;
	block_device_t parent_bd;
	bool mounted = false;
}bd_mounts_t;

static block_device_t bd_list[CONFIG_EXT4_BLOCKDEVS_COUNT];
static bd_mounts_t mount_list[MAX_MOUNT_POINTS];
static bd_mounts_t fat_mount_list[MAX_MOUNT_POINTS];

// Stat struct. File type and size.
typedef struct {
	mode_t st_mode;     // File mode
	size_t   st_size;     // File size (regular files only)
} stat_t;


//******************************************************************************
// Non-ext4FS protos.
//******************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

int get_bdev(struct ext4_blockdev * bdev);
int get_device_index(struct ext4_blockdev *bdev);
void init_device_id(void);
#ifdef __cplusplus
}
#endif
//******************************************************************************

class GIGAext4 : public USBHostMSD
{
public:
	GIGAext4();
	~GIGAext4() {}
	
	virtual int init();
	virtual void clr_BD_List(void);
	virtual bool clr_BDL_entry(uint8_t dev);
    virtual bool clr_Mount_List(uint8_t dev);
	virtual bool clr_ML_entry(uint8_t dev);
    virtual int get_fs_type(USBHostMSD *device);
	virtual int init_block_device(void *drv, uint8_t dev);
	virtual int scan_mbr(uint8_t dev);
	virtual int mount(uint8_t device);
	virtual int umountFS(const char *device);
	virtual int lwext_mount(uint8_t dev);
	virtual int lwext_umount(uint8_t dev);
	virtual int getMountStats(const char * vol, struct ext4_mount_stats *mpInfo);
	virtual int lwext_mkfs (struct ext4_blockdev *bdev, const char *label = "");
	virtual const char *get_mp_name(uint8_t id);
	virtual block_device_t *get_bd_list(void);
	virtual bd_mounts_t *get_mount_list(void);
	virtual int lwext_stat(const char *filename, stat_t *buf);
	virtual const char * getVolumeLabel();
	virtual void dumpBDList(void);
	virtual void dumpMountList(void);

protected:
	uint8_t id = 0;
	char fullPath[512];
	char fullPath1[512];
	struct ext4_mbr_bdevs bdevs;
private:
	struct ext4_mount_stats stats;
};

#endif
