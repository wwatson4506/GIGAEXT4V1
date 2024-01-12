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
// Adapted
#include <Arduino_USBHostMbed5.h>
#include <LibPrintf.h>
#include <GIGAext4FS.h>

//**********************BLOCKDEV INTERFACE**************************************
static int ext4_bd_open(struct ext4_blockdev *bdev);
static int ext4_bd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt);
static int ext4_bd_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt);
static int ext4_bd_close(struct ext4_blockdev *bdev);
static int ext4_bd_lock(struct ext4_blockdev *bdev);
static int ext4_bd_unlock(struct ext4_blockdev *bdev);

//******************************************************************************
EXT4_BLOCKDEV_STATIC_INSTANCE(_ext4_bd,  EXT4_BLOCK_SIZE, 0, ext4_bd_open,
			      ext4_bd_bread, ext4_bd_bwrite, ext4_bd_close,
			      0, 0);
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 1
EXT4_BLOCKDEV_STATIC_INSTANCE(_ext4_bd1,  EXT4_BLOCK_SIZE, 0, ext4_bd_open,
			      ext4_bd_bread, ext4_bd_bwrite, ext4_bd_close,
			      0, 0);
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 2
EXT4_BLOCKDEV_STATIC_INSTANCE(_ext4_bd2,  EXT4_BLOCK_SIZE, 0, ext4_bd_open,
			      ext4_bd_bread, ext4_bd_bwrite, ext4_bd_close,
			      0, 0);
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 3
EXT4_BLOCKDEV_STATIC_INSTANCE(_ext4_bd3,  EXT4_BLOCK_SIZE, 0, ext4_bd_open,
			      ext4_bd_bread, ext4_bd_bwrite, ext4_bd_close,
			      0, 0);
#endif

// List of interfaces
static struct ext4_blockdev * const ext4_blkdev_list[CONFIG_EXT4_BLOCKDEVS_COUNT] =
{
	&_ext4_bd,
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 1
	&_ext4_bd1,
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 2
	&_ext4_bd2,
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 3
	&_ext4_bd3,
#endif
};

PlatformMutex _mutex;

static void mp_lock()
{
printf("lock() taken\n");
  _mutex.lock();
}

static void mp_unlock()
{
printf("unlock() taken\n");
  _mutex.unlock();
}

static struct ext4_lock mp_lock_func = {
	.lock	  = mp_lock,
	.unlock	  = mp_unlock
};

// A small hex dump function
void hexDmp(const void *ptr, uint32_t len) {
  uint32_t  i = 0, j = 0;
  uint8_t   c=0;
  const uint8_t *p = (const uint8_t *)ptr;

  printf("BYTE      00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
  printf("---------------------------------------------------------\n");
  for(i = 0; i <= (len-1); i+=16) {
   printf("%4.4lx      ",i);
   for(j = 0; j < 16; j++) {
      c = p[i+j];
      printf("%2.2x ",c);
    }
    printf("  ");
    for(j = 0; j < 16; j++) {
      c = p[i+j];
      if(c > 31 && c < 127)
        printf("%c",c);
      else
        printf(".");
    }
    printf("\n");
  }
}

//******************************************************************************
// Initial setup block device ID's;
//******************************************************************************
void init_device_id(void) {

	for(int i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; i++) {
		bd_list[i].connected = false;
		bd_list[i].dev_id = i;		
	}
}

//******************************************************************************
// Get low level block device index. This is up to three USB devices.
//******************************************************************************
int get_bdev(struct ext4_blockdev * bdev) {
	int index;
	int ret = -1;
	for (index = 0; index < CONFIG_EXT4_BLOCKDEVS_COUNT; index++)
	{
		if (bdev == ext4_blkdev_list[index]) {
			ret = index;
			break;
		}
	}
	return ret;
}

//******************************************************************************
// Get block device (partition) index.
//******************************************************************************
int get_device_index(struct ext4_blockdev *bdev) {
	int index;
	int ret = -1;
	for (index = 0; index < 16; index++)
	{
		if (bdev == (struct ext4_blockdev *)&mount_list[index].partbdev) {
			if(mount_list[index].parent_bd.connected) {
				ret = mount_list[index].parent_bd.dev_id;
			}
			break;
		}
	}
	return ret;
}

//******************************************************************************
// Open block device. Low level and partition.
//******************************************************************************
static int ext4_bd_open(struct ext4_blockdev *bdev) {
#ifdef EXT4_DBG
  printf("ext4_bd_open()\n");
#endif
	int index;
	index = get_bdev(bdev);
	if(index == -1)
		index = get_device_index(bdev);
	if(index <= 2) {
        int blkSize = bd_list[index].pDrive->get_read_size();
        uint64_t capacity = bd_list[index].pDrive->size();
		if(!bd_list[index].pDrive) return EIO;
		bd_list[index].pbdev->part_offset = 0;
		bd_list[index].pbdev->part_size = capacity;
		bd_list[index].pbdev->bdif->ph_bcnt = capacity/blkSize;
	}/* else {
		if(!bd_list[index].pSD) return EIO;
		bd_list[index].pbdev->part_offset = 0;
		bd_list[index].pbdev->part_size = bd_list[index].pSD->sectorCount() * EXT4_BLOCK_SIZE;
		bd_list[index].pbdev->bdif->ph_bcnt = bd_list[index].pSD->sectorCount();
	}
*/
    return EOK;
}

//******************************************************************************
// Low level block (sector) read.
//******************************************************************************
static int ext4_bd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
                          uint32_t blk_cnt) {
#ifdef EXT4_DBG
  printf("ext4_bd_bread()\n");
  printf("blk_id = %llu\n",blk_id);
  printf("blk_cnt = %lu\n",blk_cnt);
#endif
	int index;
//	uint8_t status;
	int status;
	index = get_bdev(bdev);
	if(index == -1)
		index = get_device_index(bdev);
	if(index <= 2) {
		if(!bd_list[index].pDrive) return EIO;
        // Added below to do multi block transfers. One mod made to USBHostMSD.h as well.
        status = bd_list[index].pDrive->dataTransfer((uint8_t *)buf, blk_id, (uint8_t)blk_cnt,USB_DEVICE_TO_HOST);
    //*****************************************************************************************************************************
    // This is the original xfer one block at a time call. 
    // status = bd_list[index].pDrive->read(buf, (uint64_t)(blk_id * bdev->bdif->ph_bsize), (uint64_t)(blk_cnt * bdev->bdif->ph_bsize));
    //*****************************************************************************************************************************
		if (status != EOK) return EIO;

	}/* else {
		status = bd_list[index].pSD->readSectors(blk_id,
								(uint8_t *)buf, blk_cnt);
		if (status == false)
			return EIO;
	} */
	return EOK;
}
//******************************************************************************

//******************************************************************************
// Low level block (sector) write.
//******************************************************************************
static int ext4_bd_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt) {
#ifdef EXT4_DBG
  printf("ext4_bd_write()\n");
  printf("blk_id = %llu\n",blk_id);
  printf("blk_cnt = %lu\n",blk_cnt);
#endif

	int index;
	uint8_t status = 0;

	index = get_bdev(bdev);
	if(index == -1)
		index = get_device_index(bdev);
	if(index <= 2) {
		if(!bd_list[index].pDrive) return EIO;
        // Added below to do multi block transfers. One mod made to USBHostMSD.h as well.
        status = bd_list[index].pDrive->dataTransfer((uint8_t *)buf, blk_id, (uint8_t)blk_cnt,USB_HOST_TO_DEVICE);
    //*****************************************************************************************************************************
    // This is the original xfer one block at a time call. 
    // status = bd_list[index].pDrive->program(buf, (uint64_t)(blk_id * bdev->bdif->ph_bsize), (uint64_t)(blk_cnt * bdev->bdif->ph_bsize));
    //*****************************************************************************************************************************
		if (status != 0) return EIO;
	}/* else {
		status = bd_list[index].pSD->writeSectors(blk_id,
								(uint8_t *)buf, blk_cnt);
		if (status == false)
			return EIO;
	}*/
	return EOK;
}

//******************************************************************************
// Low level block device close. (how to implement?).
//******************************************************************************
static int ext4_bd_close(struct ext4_blockdev *bdev)
{
#ifdef EXT4_DBG
  printf("ext4_bd_close()\n");
#endif
	(void)bdev;
	return EOK;
}

//******************************************************************************
// Not implemeted yet. TODO.
//******************************************************************************
static int ext4_bd_ctrl(struct ext4_blockdev *bdev, int cmd, void *args)
{
#ifdef EXT4_DBG
  printf("ext4_bd_ctrl()\n");
#endif
	(void)bdev;
	return EOK;
}

static int ext4_bd_lock(struct ext4_blockdev *bdev)
{
#ifdef EXT4_DBG
  printf("ext4_bd_lock()\n");
#endif
	return 0;
}

static int ext4_bd_unlock(struct ext4_blockdev *bdev)
{
#ifdef EXT4_DBG
  printf("ext4_bd_unlock()\n");
#endif
	return 0;
}

//******************************************************************************

GIGAext4::GIGAext4()
{
//  init();
//  checkConnectedInitialized	
}
	
int GIGAext4::init() {
  return 0;	
}

// Dump mount list. mounted/unmounted
void GIGAext4::dumpMountList(void) {
	for (int i = 0; i < 4; i++) {
    	if(mount_list[i].parent_bd.connected && (strcmp(mount_list[i].pname,"UnKnown") != 0)) {
			printf("mount_list[%d].pt = 0x%2.2x\n", i, mount_list[i].pt);
			printf("mount_list[%d].pname = %s\n", i, mount_list[i].pname);
			printf("mount_list[%d].available = %d\n", i, mount_list[i].available);
			printf("mount_list[%d].parent_bd.name = %s\n", i, mount_list[i].parent_bd.name);
			if(mount_list[i].mounted)
				printf("mount_list[%d].mounted = Mounted\n\n", i);
			else
				printf("mount_list[%d].mounted = UnMounted\n\n", i);
		}
	}
}


// Dump block device list.
void GIGAext4::dumpBDList(void) {
	for (int i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; i++) {
		if(mount_list[i].parent_bd.connected && bd_list[i].dev_id >= 0) {
			printf("bd_list[%d].dev_id = %d\n", i, bd_list[i].dev_id);
			printf("bd_list[%d].name = %s\n", i, bd_list[i].name);
			printf("bd_list[%d].*pDrive = %p (USB)\n", i, bd_list[i].pDrive);
//			printf("bd_list[%d].*pSD = %d (SD)\n", i, bd_list[i].pSD);
			printf("bd_list[%d].*pbdev = %p\n", i, bd_list[i].pbdev);
			if(bd_list[i].connected)
			printf("bd_list[%d].connected = true\n\n",i);
			else
			printf("bd_list[%d].connected = false\n\n",i);
		}
	}
}

int GIGAext4::get_fs_type(USBHostMSD *device) {
  int r = -1;

  r = device->init();  // Initialize this MSD.
  if(r != EOK) {
	printf("device->init() - Failed: %d\n",r);
	return -1;
  }
  bd_list[0].pDrive = device; // Setup pointer to this device.
  bd_list[0].pbdev = ext4_blkdev_list[0];
  r = ext4_mbr_scan(bd_list[0].pbdev, &bdevs);
  if (r != EOK) return -1;
  if (!bdevs.partitions[0].bdif) return ENODEV;
  else return bdevs.partitions[0].bdif->ph_bbuf[450];
  return r;
}	

//******************************************************************************
// Scan MBR on block device for partition info.
//******************************************************************************
int GIGAext4::scan_mbr(uint8_t dev) {
#ifdef EXT4_DBG
  printf("scan_mbr(%d)\n");
#endif
	int r = -1;
	int i;

	//Get partition list into mount_list.
	r = ext4_mbr_scan(bd_list[dev].pbdev, &bdevs);
	if (r != EOK) {
		printf("ext4_mbr_scan error %d\n",r);
		return r;
	}

	for (i = 0; i < 4; i++) {
		if (!bdevs.partitions[i].bdif) {
			strcpy(mount_list[i].pname, "UnKnown"); // UnKnown.
			mount_list[i].available = false;
			continue;
		}
		mount_list[i].pt = bdevs.partitions[i].bdif->ph_bbuf[450]; // Partition OS type. (0x83).
		mount_list[i].available = true;
		sprintf(mount_list[i].pname, "/%s/",mpName[i]); // Assign partition name sda1... from list.
		mount_list[i].partbdev = bdevs.partitions[i]; // Store partition info in mount list.
	}
	return r;
}

//******************************************************************************
// Helper function to clear a bd_list[] entry. (device was disconnected).
//******************************************************************************
bool GIGAext4::clr_BDL_entry(uint8_t dev) {
	if(dev > (CONFIG_EXT4_BLOCKDEVS_COUNT - 1)) return false;
	sprintf(bd_list[dev].name,"UnKnown");
	bd_list[dev].connected = false;
	bd_list[dev].pbdev = NULL;
	bd_list[dev].pDrive = NULL;
	bd_list[dev].dev_id = -1;
	return true;
}

//******************************************************************************
// Helper function to clear a bd_list[] entry. (device was disconnected).
//******************************************************************************
bool GIGAext4::clr_ML_entry(uint8_t dev) {
	if(dev > (CONFIG_EXT4_BLOCKDEVS_COUNT - 1)) return false;
      strcpy(mount_list[dev].pname, "UnKnown"); // UnKnown.
      mount_list[dev].pt = 0;
      mount_list[dev].available = false;
      mount_list[dev].mounted = false;
	return true;
}

//******************************************************************************
// Helper function to clear all bd_list's.
//******************************************************************************
void GIGAext4::clr_BD_List(void) {
  for(int i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; i++)
	clr_BDL_entry(i);
}

//******************************************************************************
// Helper function to clear all mount_list entries (device was disconnected).
//******************************************************************************
bool GIGAext4::clr_Mount_List(uint8_t dev) {
	if(dev > (CONFIG_EXT4_BLOCKDEVS_COUNT - 1)) return false;
	for(int i = 0; i  < MAX_MOUNT_POINTS - 1; i++)
		clr_ML_entry(i+dev);
  return true;
}

//******************************************************************************
// Init block device.
//******************************************************************************
int GIGAext4::init_block_device(void *drv, uint8_t dev) {
#ifdef EXT4_DBG
  printf("init_block_device()\n");
#endif
    int r = -1;
	if(dev >= 4) dev = 3;
	if(dev < (CONFIG_EXT4_BLOCKDEVS_COUNT - 1)) { // USB block devices.
		USBHostMSD *pDrv = reinterpret_cast < USBHostMSD * > ( drv );
        pDrv->init(); // Initialize this MSD.
		if(pDrv->connected() && bd_list[dev].connected == true) {
			return EOK; // No change needed.
		} else if(pDrv->connected() && bd_list[dev].connected == false) { 
			bd_list[dev].pDrive = pDrv; // Setup pointer to this device.
			bd_list[dev].dev_id = dev;
			bd_list[dev].connected = true;
			sprintf(bd_list[dev].name,"%s",deviceName);
			bd_list[dev].pbdev = ext4_blkdev_list[dev];
		} else if(!pDrv->connected() && bd_list[dev].connected == true) { // Device not connected. Clear bd_List and mount_list.
			clr_BDL_entry(dev);
            for(int i = 0; i < 4; i++) {
  			  clr_ML_entry(i);
            } 
			return ENODEV; // No block devices detected at all !!
		}
	} else { // To many connected devices (0-3).
		  return ENODEV;
	}
	if(bd_list[dev].connected) {
		for(int i = 0; i < 4; i++) {
			mount_list[+i].parent_bd = bd_list[dev]; //Assign device.
		}
		// Get all partition info for this device.
	}
    r = scan_mbr(dev);
	return r;
}

//******************************************************************************
// Stat function returns file type regular file or directory and file size if
// regular file in buf. (stat_t struct)
//******************************************************************************
int GIGAext4::lwext_stat(const char *filename, stat_t *buf) {
	int r = ENOENT;

	if(('\0' == TO_LWEXT_PATH(filename)[0])
		|| (0 == strcmp(TO_LWEXT_PATH(filename),"/")) ) {// just the root
		buf->st_mode = S_IFDIR;
		buf->st_size = 0;
		r = 0;
	} else {
		union {
			ext4_dir dir;
			ext4_file f;
		} var;
		r = ext4_dir_open(&(var.dir), TO_LWEXT_PATH(filename));
		if(0 == r) {
			(void) ext4_dir_close(&(var.dir));
			buf->st_mode = S_IFDIR;
			buf->st_size = 0;
		} else {
			r = ext4_fopen(&(var.f), TO_LWEXT_PATH(filename), "rb");
			if( 0 == r) {
				buf->st_mode = S_IFREG;
				buf->st_size = ext4_fsize(&(var.f));
				(void)ext4_fclose(&(var.f));
			}
		}
	}
	return r;
}
//******************************************************************************

//******************************************************************************
// Helper function to attach mount name to filename. /mp/sdxx/filename.
//******************************************************************************
const char * GIGAext4::get_mp_name(uint8_t id) {
	return mount_list[id].pname;
}

//******************************************************************************
// Return pointer to mount list.
//******************************************************************************
bd_mounts_t * GIGAext4::get_mount_list(void) {
	return &mount_list[0];
}

//******************************************************************************
// Return pointer to bd list.
//******************************************************************************
block_device_t * GIGAext4::get_bd_list(void) {
	return &bd_list[0];
}

//******************************************************************************
// Returns mount status for a device in mpInfo struct.
//******************************************************************************
int GIGAext4::getMountStats(const char *vol, struct ext4_mount_stats *mpInfo) {
	return ext4_mount_point_stats(vol, mpInfo);
}

//******************************************************************************
// Mount a partition. Called by begin or external call. Recovers file system errors.
//******************************************************************************
int GIGAext4::lwext_mount(uint8_t dev) {
#ifdef EXT4_DBG
  printf("lwext_mount(%d)\n",dev);
#endif
	int r;
	mount_list[dev].mounted = false;
	if(!mount_list[dev].available) return ENXIO;
	r = ext4_device_register(&mount_list[dev].partbdev, mount_list[dev].pname);
	if (r != EOK) {
		printf("ext4_device_register: rc = %d\n", r);
		return r;
	}
	r = ext4_mount(mount_list[dev].pname, mount_list[dev].pname, false);
	if (r != EOK) {
		printf("ext4_mount: rc = %d\n", r);
		(void)ext4_device_unregister(mount_list[dev].pname);
		return r;
	}
	r = ext4_recover(mount_list[dev].pname);
	if (r != EOK && r != ENOTSUP) {
		printf("ext4_recover: rc = %d\n", r);
		return r;
	}
// Journaling not working yet
//	r = ext4_journal_start(mount_list[dev].pname);
//	if (r != EOK) {
//		printf("ext4_journal_start: rc = %d\n", r);
//		return false;
//	}
	ext4_cache_write_back(mount_list[dev].pname, 1);
	mount_list[dev].mounted = true;
	return EOK;
}

//******************************************************************************
// Mount a device if valid. (this will change)
//******************************************************************************
int GIGAext4::mount(uint8_t device) {
#ifdef EXT4_DBG
  printf("mount(%d)\n",device);
#endif
	int r = 0;
	if(!mount_list[device].available) return ENODEV;
	// Device 0-15.
	// lwext4 only supports 4 partitions at a time.
	if((r = lwext_mount(device)) > 0) {
		printf("lwext_mount(%d) Failed: %d\n",device,r);
		return r;
	}
	id = device;
	if((r = ext4_mount_point_stats((const char *)mount_list[device].pname,&stats)) != EOK) { 
		printf("ext4_mount_point_stats(%d) Failed: %d\n",device,r);
		mount_list[device].mounted = false;
		return r;
	}
	strcpy(mount_list[device].volName, stats.volume_name);
	return EOK;
}

//******************************************************************************
// Return volume name.
//******************************************************************************
const char * GIGAext4::getVolumeLabel() {
	return stats.volume_name;
}

//**********************************************************************
// Cleanly unmount amd unregister partition.
// Needs to be called before removing device!!!
//**********************************************************************
int GIGAext4::lwext_umount(uint8_t dev) {
#ifdef EXT4_DBG
  printf("lwext_umount(%d)\n",dev);
#endif
	int r;
	ext4_cache_write_back(mount_list[dev].pname, 0);
// Journaling not working yet
//	r = ext4_journal_stop(mount_list[dev].pname);
//	if (r != EOK) {
//		printf("ext4_journal_stop: fail %d", r);
//		return false;
//	}
	r = ext4_umount(mount_list[dev].pname);
	if (r != EOK) {
		printf("ext4_umount: fail %d\n", r);
		return r;
	}
	// UnRegister partition by name.
	r = ext4_device_unregister(mount_list[dev].pname);
    mount_list[dev].mounted = false;
	stats.volume_name[0] = '\0'; // Clear volume label.
	return EOK;
}

//--------------------------------------------------
// UnMount all partitions of an ext4 device and
// eject it.
//--------------------------------------------------
int GIGAext4::umountFS(const char * device) {
#ifdef TalkToMe
  printf("umountFS(%s)\r\n", device);
#endif
  for(int i = 0; i < 4; i++) {
    if(mount_list[i].pt == EXT4_TYPE && mount_list[i].mounted) {
      if(lwext_umount(i) != EOK) {
		return ENODEV;
      }
      printf("Unmounting partition %s.\n", mount_list[i].pname);
      clr_ML_entry(i);
    }
  }
  printf("Drive %s can be safely removed now.\n", bd_list[0].name);
  clr_BDL_entry(0);
  return EOK;
}
//******************************************************************************
// Format a partition to ext4.
//******************************************************************************
int GIGAext4::lwext_mkfs (struct ext4_blockdev *bdev, const char *label)
{
	int ercd = 0;
	static struct ext4_fs fs1;
	static struct ext4_mkfs_info info1;

	info1.block_size = 4096;
	info1.journal = false;
	ercd = ext4_mkfs(&fs1, bdev, &info1, F_SET_EXT4, label);

	return ercd;
}
