# GIGAEXT4V1

This is the second version of lwext4 adapted for use with the GIGA R1. 

This is the start of being able to use lwext4 with the GIGA. This version supports MSD USB drives. It has been tested with Arduino 1.8.19 and Arduino IDE 2.2.1.

## Not sure if more work will be done with this WIP library and is at a "USE AT YOUR OWN RISK" status!!!

You will need:
- This repository
- Arduino 1.8.19 or 2.2.1
- LibPrintf (Use library manager)
- Arduino_USBHostMbed5 from my GitHub found here: https://github.com/wwatson4506/Arduino_USBHostMbed5

#### Usage:

You will need a USB drive formatted as ext4 with a volume label to identify the drive. Compile any of the GIGAext4FS examples and upload to the GIGA R1.

There is a config file 'ext4_config.h' in the 'src' directory. The default settings are the best ones so far for the GIGA. Be aware that journaling is not working right now so that has been disabled.

For reference:

https://github.com/gkostka/lwext4 Original Version.

https://github.com/gkostka/stm32f429disco-lwext4 An example using USB drive as a block device.

https://github.com/autoas/as/tree/master/com/as.infrastructure/system/fs  

#### Example Sketches:
- ext4Usage.ino This sketch contains information on the general usage of GIGAEXT4 and is similar to LittleFS_Usage and SdFat_Usage.
- ext4DeviceInfo.ino Gives various stats about a mounted block device and it's ext4 partitions.
- DirectoryDemo.ino Gives a directory listing for a FAT32 and ext4 drive.
- CopyfilesUSB.ino Needs a FAT32 formatted USB device and a ext4 formated USB device plugged into a HUB. Demonstrates copying a file
  to another file on the same drive and also coping it to another drive
These are the example sketches available so far. They demonstrate alot of LWext4's capabilities on the GIGA.

#### WARNING: Not unmounting an ext4 formatted USB device before removing can cause data loss as information is written back to the drive when unmounted.

### Things to know:
- Each FAT32 or ext4 partition is treated as a logical drive. They are designated as
  /sda1/../sda4/ for ext4 partitions and /fat1/../fat4/ for FAT32 partitions. These must
  preceed any file or directory name. eg: "/sda1/file1.txt", "/fat3/file2.txt" etc...
  where /sda1/ would represent an ext4 partition 1 and /sda4/ would represnt partition 4.
  Same with /fat1/ to /fat4/ else error or sometimes a crash  with flashing red light.
- ext4IOutility.cpp contains several utility functions used for mounting and unmounting partitions
  as well as getting files sizes, directory listings using wildcards and relative directory paths
  and USB MSD device stats using statvfs.
- Hot plugging is supported and warnings are given for improper hot plugging.
- Volume names (labels) are supported with the ext4 filesystem only. Volume labels for FatFS has
  not been enabled w/o recompiling the Mbed core.
- There is  a minimal sketch that can be used as template for testing. It tests for a USB device
  being plugged in and if it is, tries to mount all available partitions. The it waits for user input
  before unmounting al mounted partitions.
  
#### TODO:
- Use symlinks.
- Proccess attributes.
- Proccess timestamps.
- Proccess directory indexing.
- Set and get user and group permissions.
- Set and get user and group ID's.
- Implement default drive usage.
- Implement chdir().

There is a lot of capability in lwext4 that has not yet been added. Some of it may not be practical to use in this application.

#### Limitations:
 Two physical drives with four partitions each are supported (one FAT32 drive and one ext4 drive).
 Note: lwext4 only supports four mounted partitions total
       at any one time.

#### TODO: NOT sure att this time.
 
 ## WARNING: ext4 MUST be cleanly un-mounted to avoid data loss.
 ##            'umountDrives(sdax or fatx);'
 
#### Fixes:
 - Added missing ext4_unregister() to lwext4_umount().

This is still very much work in progress # WIP

More to come, MAYBE...
