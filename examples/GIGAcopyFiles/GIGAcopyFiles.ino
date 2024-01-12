//***********************************************************************
// GIGAcopyFiles.ino for GIGA R1 using GIGAext4FS. 
// Based on lwext4 by: Grzegorz Kostka (kostka.grzegorz@gmail.com)
// GIGA - USBHostMSD for LWext4.
// A sketch how to copy a file to another file on the same drive and
// onto another drive.
// (EXT4 drive /sda1/../sda4/ and FAT32 drive /fat1/../fat4/).
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

//************************************************
// Size of read/write buffer. Start with 4096.
// Have tried 4096, 8192, 16384, 32768 and 65536.
// Usually settle on 16384 
//************************************************
const size_t BUF_SIZE = 32768;

// File size in MB where MB = 1,024,000 bytes.
const uint32_t FILE_SIZE_MB = 32;
// File size in bytes.
const uint32_t FILE_SIZE = 1024000UL*FILE_SIZE_MB;
uint8_t* buf[BUF_SIZE];
uint32_t t;
uint32_t flSize = 0;
float MBs = 1.0f;

int result = 0;
FILE *myFile;
FILE *myFile1;

void setup()
{
  Serial.begin(115200);
  while (!Serial) ;
  
  delay(1000);
  printf("%cGIGA Copy Files\n\n",12);

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
   	  printf("Plug in a formatted USB stick and press a key\n");
      waitforInput();
    }
  }
  printf("Mounting drive sda Passed: %d\n",result);

  // Make sure user has a second USB drive plugged in.
  if(!connectInitialized(&msd1,sdb)) {
	Serial.println("DRIVE NOT PLUGGED IN!!");
	Serial.println("Plug in a formatted USB stick and press a key");
    waitforInput();
  }
  // Mount all available partitions on USB MSD device sda.
  while(1) {
    if(mountDrives(&msd2,sdb) == EOK) {
	  break;
	} else {
      umountDrives(sdb);
	  printf("Mounting drive sdb Failed: %d\n",result);
   	  printf("Plug in a formatted USB stick and press a key\n");
      waitforInput();
    }
  }
  printf("Mounting drive sdb Passed: %d\n",result);

  printf("Initializing GIGAext4FS...\n");
  printf("Please wait...\n");

waitforInput();
// DO stuff here...

   lsDir("/fat1/");  
   lsDir("/sda1/");  

  // fill buf with known data
  if (BUF_SIZE > 1) {
    for (size_t i = 0; i < (BUF_SIZE - 2); i++) {
      buf[i] = (uint8_t *)('A' + (i % 26));
    }
    buf[BUF_SIZE-2] = (uint8_t *)'\r';
  }
  buf[BUF_SIZE-1] = (uint8_t *)'\n';
  
  uint32_t n = FILE_SIZE/BUF_SIZE;

  if(exists("/sda1/test.txt")) remove("/sda1/test.txt");

  // open the file for writing.
  myFile = fopen("/sda1/test.txt", "w+");
  // if the file opened okay, write to it:
  if (myFile) {
    printf("\nWriting to test.txt...\n");
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      if(fwrite(buf, 1, BUF_SIZE, myFile) != BUF_SIZE) {
        printf("Write Failed: Stopping Here...\n");
        while(1);
      }
    }
    // Display some write timing stats.
    t = millis() - t;
    flSize = fileSize("/sda1/test.txt");
    MBs = flSize / t;
    printf("Wrote %lu bytes %f seconds. Speed : %f MB/s\n",
                        flSize, (1.0 * t)/1000.0f, MBs / 1000.0f);
    // close the file:
    fclose(myFile);
  } else {
    // if the file didn't open, print an error:
    printf("Error opening test.txt: Write Failed: Stopping Here...\n");
    while(1);
  }
  // re-open the file for reading:
  myFile = fopen("/sda1/test.txt", "r+");
  if (myFile) {
    // If the file exists then remove it.
	if(exists("/fat1/copy.txt"))
		remove("/fat1/copy.txt");
    // open the second file for writing. 
    myFile1 = fopen("/fat1/copy.txt", "w+");
    // if the file opened okay, write to it:
    if(myFile1) {
      printf("Copying /sda1/test.txt to /fat1/copy.txt...\n");
	  t = millis();
      while(fread(buf, 1, BUF_SIZE, myFile) == BUF_SIZE) {
        if(fwrite(buf, 1, BUF_SIZE, myFile1) != BUF_SIZE) {
          printf("Write Failed: Stoppiing Here...\n");
          while(1);
        }
      }
    }
    // Display some copy timing stats.
    t = millis() - t;
    flSize = fileSize("/fat1/copy.txt");
    MBs = flSize / t;
    printf("Copied %lu bytes %f seconds. Speed : %f MB/s\n",
                         flSize, (1.0 * t)/1000.0f, MBs/1000.0f);
    // close the files:
    fclose(myFile);
    fclose(myFile1);
  } else {
  	// if the file didn't open, print an error:
    printf("error opening /fat1/copy.txt\n");
  }
  // re-open the second file for reading:
  myFile1 = fopen("/fat1/copy.txt", "r+");
  if (myFile1) {
    printf("Reading File: /fat1/copy.txt...\n");
    t = millis();
    while(fread(buf, 1, BUF_SIZE, myFile1) == BUF_SIZE);
    // Display some read timing stats.
    t = millis() - t;
    flSize = fileSize("/fat1/copy.txt");
    MBs = flSize / t;
    printf("Read %lu bytes %f seconds. Speed : %f MB/s\n",
                       flSize, (1.0 * t)/1000.0f, MBs/1000.0f);
    // close the files:
    fclose(myFile1);
  } else {
  	// if the file didn't open, print an error:
    printf("Error opening copy.txt\n");
  }

waitforInput();
  // Unmount a USB device (all logical drives (partitions).
  umountDrives(sda);
  umountDrives(sdb);
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
