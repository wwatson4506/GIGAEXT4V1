//**************************************************************
// This program is a simple binary write/read benchmark.       *
// Loosely based on Bill Greimans Bench.ino sketch for SdFat.  *
// Modified to work with GIGAext4FS.                           *
//**************************************************************
  
#include "ext4IOutility.h"

//USB devices
#define sda 0 // USB device 1
#define sdb 1 // USB device 2


REDIRECT_STDOUT_TO(Serial);

USBHostMSD msd1; // USB device 1 or 2 who knows!!
USBHostMSD msd2; // USB device 2 or 1 who knows!!

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

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = false;

// Size of read/write buffer.
const size_t BUF_SIZE = 32*1024; // Experiment with this:)

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 5;

// Write pass count.
const uint8_t WRITE_COUNT = 3;

// Read pass count.
const uint8_t READ_COUNT = 3;
//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 1000000UL*FILE_SIZE_MB;
//const uint32_t FILE_SIZE = 1024000UL*FILE_SIZE_MB;

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3)/4];
uint8_t* buf = (uint8_t*)buf32;

FILE *file;

int result = 0;

void setup() {
   while (!Serial) {
    yield(); // wait for serial port to connect.
  }
  delay(1000);
  printf("%cGIGA R1 lwext4 file system bench test\n\n",12);

  // Make sure user has a USB drive plugged in (1 ext4 only).
  if(!connectInitialized(&msd1,sda)) {
	Serial.println("DRIVE NOT PLUGGED IN!!");
	Serial.println("Plug in a ext4 formatted USB stick and");
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
  printf("\nUse a freshly formatted Mass Storage drive for best performance.\n");
}


void loop() {
  uint8_t c = 0;
  float s;
  uint32_t t;
  uint32_t maxLatency;
  uint32_t minLatency;
  uint32_t totalLatency;
  bool skipLatency;

  if(!checkAvailable(sda)) {
	umountDrives(sda);
	printf("USB device unpluggged!!! STOPING HERE...\n");
	while(1);
  }

  // Discard any residual input.
  do {
    delay(10);
  } while (Serial.available() && Serial.read() >= 0);

  printf("Type any character to start, ('e' to end)\n");
  while (!Serial.available()) yield();
  c = Serial.read();
  while(Serial.available()) Serial.read(); // Get rid of CR and/or LF if there.
  if(c == 'e') failed();

  if(msd1Drv->type == EXT4_TYPE) {
	printf("Type is ext4 (0x%2.2x)\n",EXT4_TYPE);
  } else if(msd1Drv->type == FAT32_TYPE) {
	printf("Type is fat32 (0x%2.2x)\n",FAT32_TYPE);
  } else {
	printf("Wrong file system type. Must restart with ext4 file system type\n");
    printf("Halting here...");
    while(1);
  }
  printf("USB device total size: %llu", totalSize("/sda1/"));
  printf(" GB (GB = 1E9 bytes)\n");
  // open or create file - truncate existing file.
  if(msd1Drv->type == EXT4_TYPE) {
    file = fopen("/sda1/bench.dat", "w+");
  } else if(msd1Drv->type == FAT32_TYPE) {	  
    file = fopen("fat1/bench.dat", "w+");
  }
  if (!file) {
    printf("open failed\n");
    failed();
  }
  // fill buf with known data
  if (BUF_SIZE > 1) {
    for (size_t i = 0; i < (BUF_SIZE - 2); i++) {
      buf[i] = 'A' + (i % 26);
    }
    buf[BUF_SIZE-2] = '\r';
  }
  buf[BUF_SIZE-1] = '\n';

  printf("FILE_SIZE_MB = %lu\n",FILE_SIZE_MB);
  printf("BUF_SIZE = %d bytes\n", BUF_SIZE);
  printf("Starting write test, please wait.\n\n");

  // do write test
  uint32_t n = FILE_SIZE/BUF_SIZE;
  printf("write speed and latency\n");
  printf("speed,max,min,avg\n");
  printf("KB/Sec,usec,usec,usec\n");
  for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++) {
    fseek(file, 0, SEEK_SET);
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      uint32_t m = micros();
      if (fwrite(buf, 1, BUF_SIZE, file) != BUF_SIZE) {
        printf("write failed\n");
        fclose(file);
        failed(); // Give up when this happens.
      }
      m = micros() - m;
      totalLatency += m;
      if (skipLatency) {
        // Wait until first write to ext4 drive, not just a copy to the cache.
        skipLatency = ftell(file) < 512;
      } else {
        if (maxLatency < m) {
          maxLatency = m;
        }
        if (minLatency > m) {
          minLatency = m;
        }
      }
    }
    fflush(file);
    t = millis() - t;
    s = (float)(1.0 * ftell(file));
    printf("%f,%lu,%lu,%lu\n", (float)(s/t), maxLatency, minLatency, (totalLatency/n));
  }
  printf("\nStarting read test, please wait.\n");
  printf("\nread speed and latency\n");
  printf("speed,max,min,avg\n");
  printf("KB/Sec,usec,usec,usec\n");

  // do read test
  for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++) {
    fseek(file, 0, SEEK_SET);
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      buf[BUF_SIZE-1] = 0;
      uint32_t m = micros();
      int32_t nr = fread(buf, 1, BUF_SIZE, file);
      if (nr != BUF_SIZE) {
        error("read failed\n");
        fclose(file);
        failed(); // Give up when this happens.
      }
      m = micros() - m;
      totalLatency += m;
      if (buf[BUF_SIZE-1] != '\n') {
        error("data check error\n");
        fclose(file);
        failed(); // Give up when this happens.
      }
      if (skipLatency) {
        skipLatency = false;
      } else {
        if (maxLatency < m) {
          maxLatency = m;
        }
        if (minLatency > m) {
          minLatency = m;
        }
      }
    }
    t = millis() - t;
    s = (float)(1.0 * ftell(file));
    printf("%f,%lu,%lu,%lu\n", (float)(s/t), maxLatency, minLatency, (totalLatency/n));
  }
  printf("Filesize = %f\n",s);
  printf("Done\n");
  fclose(file);
}

// A fatal error occured. unmount and hang out...
void failed(void) {
  printf("Unmounting sda...");
  umountDrives(sda);
  while(1);	
}

// Wait for user input on serial console.
void waitforInput()
{
  Serial.println("\nPress anykey to continue");
  while (Serial.read() == -1) ;
  while (Serial.read() != -1) ;
}

// Simple directory listing function.
int ls(const char *path) {
  int id = 0, err = 0;
  char buf[512];
  uint32_t fSize = 0;

  // Path name MUST be prefixed with a logical drive specifier "/sdax/"
  DIR *d = opendir(path); // or "/fatx" where 'x' is 1..4.
  if (!d) {
    printf("Error %d: could not open directory %s\n", -errno, path);
    return -1;
  }
  printf("\nDirectory Path: %s\n",path);
  printf("Volume Label: %s\n\n",ext4p[getMPid(path)].getVolumeLabel());
  while (true) {
    struct dirent* e = readdir(d); if (!e) { break; }
    printf("%-50s     %10s  ",e->d_name, entry_to_str(e->d_type));
    if(e->d_type == DT_DIR) { putchar('\n'); // Don't print file size for DIR entries.
    } else {
      sprintf(buf, "%s%s", path,e->d_name);
      fSize = fileSize(buf);
	  printf("%10ld\n", fSize);
    }
  }
  err = closedir(d);
  if (err < 0) {
    printf("Error %d: close directory failed\n",-errno); // Print out error info.
  }
  printf("\nBytes Used: %llu, Bytes Total:%llu\n", usedSize(path), totalSize(path));
  printf("Bytes Free: %llu\n", freeSize(path));

  return 0;  
}
