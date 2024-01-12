// Print ext4 volume info.

#include "ext4IOutility.h"

//USB devices
#define sda 0
#define sdb 1

REDIRECT_STDOUT_TO(Serial);

USBHostMSD msd1; // USB device 1 or 2 who knows!!
#ifdef USE_FAT32_DRIVE
USBHostMSD msd2; // USB device 2 or 1 who knows!!
#endif
// Get pointer to EXT4FileSystem partition array.
mbed::EXT4FileSystem *ext4p = getExt4fs();
#ifdef USE_FAT32_DRIVE
// Get pointer to FATFileSystem partition array.
mbed::FATFileSystem *fat32p = getFat32fs();
#endif
// Get pointer to USB drive sda filesystem type and info.
drvType_t *msd1Drv = getMSDinfo(sda);
// Get pointer to USB drive sdb filesystem type and info.
drvType_t *msd2Drv = getMSDinfo(sdb);

GIGAext4 myext4fs1;
GIGAext4 myext4fsp[4];

struct ext4_mount_stats stats;

// Grab a pointer to the mount list.
bd_mounts_t *ml = myext4fs1.get_mount_list();

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    yield(); // wait for serial port to connect.
  }

  int partCount = 0;
  int i = 0, result = 0;  
  delay(1000);

  printf("%cGIGA R1 lwext device info\n\n",12);

  // Make sure user has a USB drive plugged in.
  if(!connectInitialized(&msd1,sda)) {
	Serial.println("DRIVE NOT PLUGGED IN!!");
	Serial.println("Plug in a ext4 formatted USB stick and");
    waitforInput();
  }

  printf("Initializing GIGAext4FS...\n");
  printf("Please wait...\n");

  // Mount all available partitions on USB MSD device sda.
  result = mountDrives(&msd1,sda);
  if(result != EOK) {
	printf("Mounting drive sda Failed: %d\n",result);
    goto FAIL;
  } else {
	printf("Mounting drive sda Passed: %d\n",result);
  }
  
  if(msd1Drv->type != EXT4_TYPE) {
	printf("Wrong filesystem type plugged in. Need EXT4 filesystem!! Quitting...\n");
	goto FAIL;
  }
  
  printf("Now we will list all physical block devices.\n");
  waitforInput();  
  myext4fs1.dumpBDList();

  printf("Now we will list all mountable partitions.\n");
  waitforInput();  
  myext4fs1.dumpMountList();

  printf("\nNow we will show partition info for each mounted partition.\n");
  waitforInput();  

  for(i = 0; i < MAX_MOUNT_POINTS; i++) {
	if(ml[i].mounted) {
      printf("********************\n");
      printf("         Partition: %s\n", ml[i].pname);
      printf("           offeset: 0x%" PRIx64 ", %" PRIu64 "MB\n",
                                    ml[i].partbdev.part_offset,
                   ml[i].partbdev.part_offset / (1024 * 1024));
      printf("              size: 0x%" PRIx64 ", %" PRIu64 "MB\n",
                                      ml[i].partbdev.part_size,
                     ml[i].partbdev.part_size / (1024 * 1024));
      printf("Logical Block Size: %lu\n", ml[i].partbdev.lg_bsize);
      printf("       Block count: %" PRIu64 "\n", ml[i].partbdev.lg_bcnt);
      printf("           FS type: 0x%x\n", ml[i].pt);
      myext4fsp[i].getMountStats(ml[i].pname, &stats);
      printf("********************\n");
      printf("ext4_mount_point_stats\n");
      printf("     inodes_count = %" PRIu32 "\n", stats.inodes_count);
      printf("free_inodes_count = %" PRIu32 "\n", stats.free_inodes_count);
      printf("     blocks_count = %" PRIu32 "\n", (uint32_t)stats.blocks_count);
      printf("free_blocks_count = %" PRIu32 "\n",
	       (uint32_t)stats.free_blocks_count);
      printf("       block_size = %" PRIu32 "\n", stats.block_size);
      printf("block_group_count = %" PRIu32 "\n", stats.block_group_count);
      printf(" blocks_per_group = %" PRIu32 "\n", stats.blocks_per_group);
      printf(" inodes_per_group = %" PRIu32 "\n", stats.inodes_per_group);
      printf("      volume_name = %s\n", stats.volume_name);
      printf("********************\n\n");
    }  

  }
FAIL:
  printf("Unmounting All Mounted Drives\n");
  umountDrives(sda); // Unmount all mounted partitions on this USB device.
 
  printf("Done...");
}

void loop() {
}

void waitforInput()
{
  printf("Press any key to continue\n");
  while (Serial.read() == -1) ;
  while (Serial.read() != -1) ;
}
