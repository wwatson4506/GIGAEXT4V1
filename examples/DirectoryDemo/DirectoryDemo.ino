//***********************************************************************
// DirectoryDemo.ino for GIGA R1 using GIGAext4FS. 
// Based on lwext4 by: Grzegorz Kostka (kostka.grzegorz@gmail.com)
// GIGA - USBHostMSD  for LWext4.
// Demonstrates listing directories on a ext4 formatted USB stick.
// Relative path and wildcards are supported.
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

// Line buffer for readLine() function.
char sbuf[256];
// A simple read line function.
char *readLine(char *s) {
	char *s1 = s;
	int	c;
	for(;;) {
		while(!Serial.available());
		switch(c = Serial.read()) {
		case 0x08:
			if(s == s1)
				break;
			s1--;
			printf("%c%c%c",c,0x20,c);
			fflush(stdout);
			break;
		case '\n':
		case '\r':
			printf("%c",c);
			fflush(stdout);
            *s1 = 0;
			return s;
		default:
			if(c <= 0x7E)
				printf("%c",c);
			fflush(stdout);
            *s1++ = c;
			break;
		}
	}
}

void help(void){
  printf("\nUsage: All path specs must start with a device spec /sda'x'/ or /fat'x'/\n");
  printf("       where 'x' is a partition number 1 to 4.\n");
  printf("       Relative path specs and wildcards are supported: \"/sda2/sstsrc/*.c\",\n");
  printf("       \"/sda2/a/b/../*??*.cpp.\"\n\n");
  printf("\nmenu:\n\n");
  printf("h) to display this help menu\n");
  printf("l) list a directory\n");
  printf("q) to umount and quit\n");
}

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

  help();
}

void loop() {
  if(!checkAvailable(sda)) {
	umountDrives(sda);
	printf("USB device unpluggged!!! STOPING HERE...\n");
	while(1);
  }
  printf("\nPress a key (h, l, q) followed by <CR>: ");
  fflush(stdout);
  readLine(sbuf);
  printf("\r\n");
  switch(sbuf[0]) {
    case 'h':
      help();
      break;
    case 'l':
      printf("Enter a path spec: ");
      fflush(stdout);
      readLine(sbuf);
      printf("\r\n\r\n");
      if(lsDir(sbuf) == false) printf("lsdir(%s) failed...\n",sbuf);
      break;
    case 'q':
      umountDrives(sda);
      printf("finished...\n");
      while(1);
    default:  
     help();
     break;
  }
}

// Wait for user input on serial console.
void waitforInput()
{
  Serial.println("\nPress any key to continue");
  while (Serial.read() == -1) ;
  while (Serial.read() != -1) ;
}
