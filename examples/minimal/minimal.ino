//***********************************************************************
// minimal.ino for GIGA R1 using GIGAext4FS. 
// Based on lwext4 by: Grzegorz Kostka (kostka.grzegorz@gmail.com)
// GIGA - USBHostMSD for LWext4.
// A minimal sketch to be used as a template. 
//***********************************************************************

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


//**********************************************************************
// WARNING: ext4 MUST be cleanly un-mounted to avoid data loss.
//          'umountDrives(sda, or sdb);'
//**********************************************************************

int result = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial) ;
  
  delay(1000);
  printf("%cGIGA Directory Listing\n\n",12);

  // Make sure user has a USB drive plugged in.
  if(!connectInitialized(&msd1,sda)) {
	Serial.println("DRIVE NOT PLUGGED IN!!");
	Serial.println("Plug in a ext4 formatted USB stick and press a key");
    waitforInput();
  }

  printf("Initializing GIGAext4FS...\n");
  printf("Please wait...\n");
  // Mount all available partitions on USB MSD device sda.
  while(1) {
    if(mountDrives(&msd1,sda) == EOK) {
	  break;
	} else {
      umountDrives(sda);
	  printf("Mounting drive sda Failed: %d\n",result);
   	  printf("Plug in a ext4 formatted USB stick and press a key\n");
      waitforInput();
    }
  }
  printf("Mounting drive sda Passed: %d\n",result);

// DO stuff here...
  waitforInput();  
  // Unmount a USB device (all logical drives (partitions).
  umountDrives(sda);
  printf("finished...\n");

}

void loop() {
	
}

// Wait for user input on serial console.
void waitforInput()
{
  Serial.println("\nPress any key to continue");
  while (Serial.read() == -1) ;
  while (Serial.read() != -1) ;
}
