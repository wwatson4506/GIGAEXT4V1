//***********************************************************************
// ext4Usage.ino  GIGA R1 lwext4 testing 
// Based on lwext4 by: Grzegorz Kostka (kostka.grzegorz@gmail.com)
// Modified version of LittlFS_Usage.ino for Teensy 4.1 LWext4 use and
// modiefied again for use with GIGAext4.
//
// Demonstrates usage of several Posix style file and directory functions
// with Mbed OS. Two USB MSD devices are supported. One ext4 type and one
// fat32 type. Each one supports 4 partions each. 
//
// NOTE: ALWAYS properly unmount ext4 logical drives. Data is written back
//       to the drive such as file accounting etc... If not, the next time
//       you mount the drive it will give you a warning.
//
// There are a few issues:
// All drive accesses must be prefixed with a partition name:
// (/sda1/, /sda2, /sda3/, /sda4/) for ext4 filesystem or fat filesystem
// (/fat1/, /fat2, /fat3/, /fat4/). See ext4IOUtility.cpp and .h.
// The mkdir() function MUST have a forward slash added to the end of the
// directory path/name like: "/sda1/testdir/". Possibly a bug in ext4.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^
//***********************************************************************

#include "ext4IOutility.h"

//USB devices
#define sda 0 // USB device 1
#define sdb 1 // USB device 2


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

FILE  *file, *file1;

int result = EOK;
int fd = 0;
unsigned int len = 0;

char msg[] = "\"Just some test data written to the file (by GIGA R1 ext4FS functions)\"\n\0";

void setup() {
   while (!Serial) {
    yield(); // wait for serial port to connect.
  }
  delay(1000);
  printf("%cGIGA R1 lwext4 file system usage\n\n",12);

  // Make sure user has a USB drive plugged in.
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

  // Now lets create a file and write some data.  Note: basically the same usage for 
  // creating and writing to a file using SD library.
  printf("\n-------------------------------------------------\n");
  printf("Now lets create a 800kb file with some data in it\n");
  printf("-------------------------------------------------\n");
  char someData[8192];
  memset( someData, 'z', 8192 );
  file = fopen("/sda1/bigfile.txt", "w+"); // Create new file.
  if(!file) {
	printf("Open file Failed!!, Unmounting drive...\n"); // Could not create file.
    goto FAIL;
  }
  fwrite(someData, sizeof(char), sizeof(someData), file);
  for (uint16_t j = 0; j < 100; j++)
  fwrite(someData, sizeof(char), sizeof(someData), file);
  fflush(file);
  // We can also get the size of the file just created.  Note we have to open and 
  // then close the file unless we do file size before we close it in the previous step.
  fseek(file,0,SEEK_END); // Seek to the end of the file.
  printf("File Size of bigfile.txt (bytes): %ld\n", ftell(file)); // get current position.
  fseek(file,0,SEEK_SET); // Seek back to beginning of file.
  result = fclose(file);
  if(result) printf("File close failed!!\n");
  // Now that we initialized the FS and created a file lets print the directory.
  // Note:  Since we are going to be doing file listing and getting disk useage
  // lets make it a function which can be copied and used in your own sketches.
  // See "ls()" below.
  ls("/sda1/"); // List root directory on partition 1 of device sda.
  waitforInput();

  // Now lets rename the file
  Serial.println("\n------------------------------------");
  Serial.println("Rename \"bigfile.txt\" to \"file10.txt\"");
  Serial.println("------------------------------------");
  rename("/sda1/bigfile.txt", "/sda1/file10.txt");
  ls("/sda1/");
  waitforInput();

  // To delete the file
  Serial.println("\n-------------------");
  Serial.println("Delete \"file10.txt\"");
  Serial.println("-------------------");
  remove("/sda1/file10.txt");
  ls("/sda1/");
  waitforInput();

  Serial.println("\n-----------------------------------");
  Serial.println("Create a directory (structureData1)");
  Serial.println("-----------------------------------");
  // Argument #2 (mode) not used at this time.
  result = mkdir("/sda1/structureData1/", 0);
  // Need foward slash here~~~~~~~~~~~^
  if(result != EOK) {
	Serial.print("mkdir Failed: result = ");
	Serial.println(result);
  }
  ls("/sda1/");
  waitforInput();

  Serial.println("\n-----------------------------------------------------");
  Serial.println("and a subfile in the above directory (temp_test.txt).");
  Serial.println("-----------------------------------------------------");
  file = fopen("/sda1/structureData1/temp_test.txt", "w+");
  fprintf(file, "%s\n", "SOME DATA TO TEST");
  fclose(file);
  ls("/sda1/structureData1/");
  waitforInput();

  Serial.println("\n----------------------------------------------------");
  Serial.println("Rename directory \"structureData1\" to \"structureData\"");
  Serial.println("----------------------------------------------------");
  rename("/sda1/structureData1", "/sda1/structureData");
  ls("/sda1/");
  waitforInput();

  Serial.println("\n-------------------------------------------");
  Serial.println("Lets remove them now. \"temp_test.txt\" first");
  Serial.println("then remove \"structureData\" last.");
  Serial.println("-------------------------------------------");
  //Note have to remove files first
  result = remove("/sda1/structureData/temp_test.txt");
  if(result != EOK) printf("Remove \"temp_test.txt\" FAIED: result = %d\n",result);
  // then the directory.
  result = remove("/sda1/structureData");
  if(result != EOK) printf("Remove \"structureData\" FAIED: result = %d\n",result);
  ls("/sda1/");
  waitforInput();

  Serial.println("\n------------------------------------------------");
  Serial.println("Now lets create a file and read the data back...");
  Serial.println("------------------------------------------------");
  // Let's demonstrate usage of lower level file functions using an int
  // file discriptor. 
  Serial.println();
  Serial.println("----------------------------------------------");
  Serial.println("Writing to datalog.bin using GIGA FS functions");
  Serial.println("----------------------------------------------");
  if((fd = open("/sda1/datalog.bin", O_WRONLY, O_CREAT)) < 0) perror("create() error");
    len = lseek(fd,0,SEEK_CUR); // Seek to current file position.
  Serial.print("datalog.bin started with ");
  Serial.print(len);
  Serial.println(" bytes");
  // reduce the file to zero if it already had data
  if (len > 0) { ftruncate(fd,0); }
  if ((result = write(fd, msg, strlen(msg))) == -1) perror("write() error");
  result = close(fd);
  if(result != EOK) Serial.println("File close FAILED!");

  // Now let's read back what we have written to "/sda1/datalog.bin".
  Serial.println();
  Serial.println("Reading from datalog.bin");
  file1 = fopen("/sda1/datalog.bin", "r+");
  if((file1 != NULL) && (errno == EOK)) {  
  char mybuffer[100]; //Read buffer.
  result = fread(mybuffer, sizeof(char), 100, file1); // One big gulp!!
  Serial.print("We read "); Serial.print(result);
  Serial.println(" bytes from datalog.bin\n");
  mybuffer[strlen(msg)] = '\0'; // Terminate string.
  Serial.print("  What was read from file: ");
  Serial.println(mybuffer); 
  } else {
    Serial.println("unable to open datalog.bin :(");
  }
  result = fclose(file1); // close file
  if(result != EOK) Serial.println("File close FAILED!");
FAIL:
  Serial.print("unmounting ");
  Serial.println("/sda1/");
  umountDrives(sda); // Unmount all mounted partitions on this USB device.
  
  Serial.println("\nBasic Usage Example Finished");
}

void loop() {

}

// Simple directory listing function.
int ls(const char *path) {
  int err = 0;
  char buf[512];
  uint32_t fSize = 0;

  // Path name MUST be prefixed with a logical drive specifier "/sdax/"
  DIR *d = opendir(path); // or "/fatx" where 'x' is 1..4.
  if (!d) {
    printf("Error %d: could not open directory %s\n", -errno, path);
    return -1;
  }
  printf("\nDirectory Path: %s\n",path);
  printf("Volume Label: %s\n\n",ext4p[sda1].getVolumeLabel());
  while (true) {
    struct dirent* e = readdir(d);
    if (!e) { break; }
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

// Wait for user input on serial console.
void waitforInput()
{
  Serial.println("\nPress anykey to continue");
  while (Serial.read() == -1) ;
  while (Serial.read() != -1) ;
}
