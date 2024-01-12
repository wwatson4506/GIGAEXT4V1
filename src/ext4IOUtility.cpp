/* ext4IOutility.cpp
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

#include "ext4IOutility.h"

#ifdef USE_FAT32_DRIVE
mbed::FATFileSystem fatfs1("fat1");
mbed::FATFileSystem fatfs2("fat2");
mbed::FATFileSystem fatfs3("fat3");
mbed::FATFileSystem fatfs4("fat4");
mbed::FATFileSystem *fatfsp[] = {&fatfs1, &fatfs2, &fatfs3, &fatfs4};
#endif

//**********************************************************************
// Setup four instances of ext4FS (four mountable partitions).
//**********************************************************************
mbed::EXT4FileSystem ext1("sda1");
mbed::EXT4FileSystem ext2("sda2");
mbed::EXT4FileSystem ext3("sda3");
mbed::EXT4FileSystem ext4("sda4");
mbed::EXT4FileSystem *extfsp[] = {&ext1, &ext2, &ext3, &ext4};

struct statvfs vfsbuf;

// An array for two USB devices.
drvType_t msd[2];

// Return pointer to msd array.
drvType_t *getMSDinfo(uint8_t drv) {
  return &msd[drv];
}

// Return a pointer to EXT4FileSystem array.
mbed::EXT4FileSystem *getExt4fs(void) {
  return *extfsp;
}

#ifdef USE_FAT32_DRIVE
// Return a pointer to FATFileSystem array.
mbed::FATFileSystem *getFat32fs(void) {
  return *fatfsp;
}  
#endif
  
// Directory entry types.
const char *entry_to_str(uint8_t type)
{
	switch (type) {
	case DT_UNKNOWN:
		return "<UNKNOWN>";
	case DT_REG:
		return "<FILE>";
	case DT_DIR:
		return "<DIR>";
	case DT_CHR:
		return "<CHARDEV>";
	case DT_BLK:
		return "<BLOCKDEV>";
	case DT_FIFO:
		return "<FIFO>";
	case DT_SOCK:
		return "<SOCKET>";
	case DT_LNK:
		return "<SYMLINK>";
	default:
		break;
	}
	return "[???]";
}

// Check if file exists on a volume.
bool exists(const char * path) {
    struct stat buf;
    int res = stat(path, &buf);
    if(res == EOK) return true; else return false;
}

// Get partition id by volume name.
// volNames can be  (/sda1/ = 0.../sda4 = 3) or  (/fat1/ = 4.../fat4/ = 7)
int getMPid(const char * volName) {
  char str[7]  = {0};
  
  strncpy(str, volName, 6);
  str[6] = 0;

  for(int i = 0; i < TOTAL_LD; i++) {
    if(strcmp((const char *)str,mpID[i]) == 0) return i;
  }
  return -1;
};

// How much space currently used on a volume.
uint64_t usedSize(const char * ld) {
  int id = getMPid(ld);
  if(id < 4)
    extfsp[id]->statvfs(ld,&vfsbuf);
  else
    fatfsp[id-4]->statvfs(ld,&vfsbuf);
  return (uint64_t)((vfsbuf.f_blocks * vfsbuf.f_bsize) -
                   (vfsbuf.f_bfree * vfsbuf.f_bsize));
}

// Total space of a volume.
uint64_t totalSize(const char *ld) {
  int id = getMPid(ld);
  extfsp[id]->statvfs(ld,&vfsbuf);
  return (uint64_t)(vfsbuf.f_blocks * vfsbuf.f_bsize);
}

// How much space is left on a volume (Logical Drive).
uint64_t freeSize(const char *ld) {
  int id = getMPid(ld);
  extfsp[id]->statvfs(ld,&vfsbuf);
  return (uint64_t)(vfsbuf.f_bfree * vfsbuf.f_bsize);
}

// Check sda or sdb is connected and mounted.
// If not return false else return true.
bool checkAvailable(uint8_t drv) {
  if(msd[drv].drive->connected() && msd[drv].mounted) return true;
  else return false;
}
	
//-------------------------------------------------------
// Check if drive is connected. Get Filesystem Type and
// if type is EXT4_TYPE try to initialize. If FAT32_TYPE
// initalize struct and return.
//-------------------------------------------------------
bool connectInitialized(USBHostMSD *drv, uint8_t index) {
  uint32_t wait = millis();
  // Already connected and mounted Return.
  if(drv->connected() && msd[index].mounted) return true;
  // Check for USB device. If not there clear drvType struct and return
  // false. If previously connected drvType struct must be cleared.
  while (!drv->connected()) {
    drv->connect();
    if((millis() - wait) > CONNECT_TIMEOUT) {
      msd[index].drive = nullptr;
      msd[index].connected = false; // If previously connected, IT AINT NOW!!
      msd[index].initialized = false; // If previously initialized, IT AINT NOW!!
      msd[index].type = -1; // If previous type known, IT AINT NOW!!
      msd[index].mounted = false; // If previously mounted, IT AINT NOW!!
      return false;
    }
  } 
  // Found drive, initialize struct..
  msd[index].drive = drv;
  msd[index].type  = ext1.getfs()->get_fs_type(drv);//getFSType(drv);
  msd[index].connected = true;
  // If ext4 fs type, initialize block device (including partitions).
  if((msd[index].type == EXT4_TYPE) &&
     (extfsp[0]->getfs()->init_block_device(drv, 0) == EOK)) {
	msd[index].initialized = true;
	return true; 
  } else if(msd[index].type == FAT32_TYPE) { // If FAT32 type...
    return true;
  }	else { // Could not initialize. Clear struct.
	msd[index].drive = nullptr;
    msd[index].type  = -1;
    msd[index].connected = false;
    msd[index].initialized = false;
    msd[index].mounted = false;
  }
  return true;
}

// Dump info of a USB drive (sda or sdb).
void dumpDrvType(uint8_t drv) {
  printf("\nDrive instance: %p\n",msd[drv].drive);
  if(msd[drv].type == EXT4_TYPE) printf("Drive Type: EXT4\n");
  if(msd[drv].type == FAT32_TYPE) printf("Drive Type: FAT32\n");
  if(msd[drv].type == -1) printf("Drive Type: UnKnown\n");
  printf("Drive connected: %d\n",msd[drv].connected);
  printf("Drive initialized: %d\n",msd[drv].initialized);
  printf("Drive mounted: %d\n\n",msd[drv].mounted);
}

// Returns file size of a file.
uint32_t fileSize(const char * path) {
  int fd = 0;
  uint32_t fSize = 0;
  if((fd = open(path, O_RDONLY,0)) < 0) {
    printf("Open file failed: %d\n",-errno);
    return 0;
  }
  fSize = lseek(fd,0,SEEK_END); // Seek to current file position.
  lseek(fd,0,SEEK_SET);
  if(close(fd) < 0) {
    printf("Close file failed: %d\n",-errno);
    return 0;
  }
  return fSize;
}

// Mount all avaiable partitions on a USB MSD device.
int mountDrives(USBHostMSD *drv, uint8_t index) {
  // Get pointer to ext4 mount list.
  bd_mounts_t *ml = extfsp[0]->getfs()->get_mount_list();
  int result = EOK;

  if(!connectInitialized(drv, index)) return ENODEV; // not connected.

  // Check for ext4 filesystem type and mount available paritions.
  if((msd[index].type) == EXT4_TYPE) {
    for(uint8_t i = 0; i < NUM_USB_LOGICAL_DRVS; i++) {
      // If partition type is ext4 and is not mounted then mount partition.
      if(ml[i].pt == EXT4_TYPE && !ml[i].mounted) {
        result = extfsp[i]->mount(msd[index].drive); 
        if(result != EOK) {
          printf("ext4 mount %d failed: %d\n",i,result); 
          msd[index].mounted = false;
          return result;
        }
        msd[index].mounted = true;
      }
    }
  }
#ifdef USE_FAT32_DRIVE
  // Check for FAT32 filesystem type and mount available partitions.
  if((msd[index].type) == FAT32_TYPE) {
    result = fatfsp[0]->mount(drv);
    if(result == EOK) {
      msd[index].initialized = true;
      msd[index].mounted = true;
    } else {
      return ENOTSUP;
    }
  }
#endif
  // If at least one partitions is mounted, mark drive as mounted,
  // else return error: ENOTSUP.
  if(msd[index].mounted) return result;
  else return ENOTSUP;
}

//-------------------------------------------------------
// UnMount all mounted partitions on all available drives.
//-------------------------------------------------------
int umountDrives(uint8_t index) {
  // Get pointer to ext4 mount list.
  bd_mounts_t *ml = extfsp[0]->getfs()->get_mount_list();
  int result = EOK;

  if((msd[index].type) == EXT4_TYPE) {
  for(int i = 0; i < NUM_USB_LOGICAL_DRVS; i++) {
    if(ml[i].mounted == true) {
	  result = extfsp[i]->unmount(); // Unmount partition if was mounted.
      if(result != EOK) { // If device was removed before unmounting then:
        printf("ext4 partition %d unmount failed: %d...\n",i,result);
      } else {
		printf("ext4 partition %d successfully unmounted...\n",i);
	  }
    }
    extfsp[i]->getfs()->clr_ML_entry(i); // Clear mount list entry.
  }
  // Clear block device list (Only one device).
  extfsp[0]->getfs()->clr_BDL_entry(sda);
  }
#ifdef USE_FAT32_DRIVE
  if((msd[index].type) == FAT32_TYPE) {
    result = fatfsp[0]->unmount();
    if(result != EOK) {
      printf("Unmount FAT32 partition Failed: %d\n",result); 
    } else {
      printf("FAT32 partition successfully unmounted...\n");
    }	
  }	
#endif
  msd[index].drive = nullptr;
  msd[index].connected = false; // If previously connected, IT AINT NOW!!
  msd[index].initialized = false; // If previously initialized, IT AINT NOW!!
  msd[index].type = -1; // If previous type known, IT AINT NOW!!
  msd[index].mounted = false; // If previously mounted, IT AINT NOW!!
  
  return result;
}

// ---------------------------------------------------------
//Wildcard string compare.
//Written by Jack Handy - jakkhandy@hotmail.com
//http://www.codeproject.com/KB/string/wildcmp.aspx
// ---------------------------------------------------------
// Based on 'wildcmp()' (original) and modified for use with
// diskIO and GIGAext4FS.
// Found and taken from SparkFun's OpenLog library.
// ---------------------------------------------------------
// Check for filename match to wildcard pattern.
// ---------------------------------------------------------
bool wildcardMatch(const char *pattern, const char *filename) {
  const char *charPointer = 0;
  const char *matchPointer = 0;

  while(*filename && (*pattern != '*')) {
    if((*pattern != *filename) && (*pattern != '?')) return false;
    pattern++;
    filename++;
  }
  while(*filename) {
    if(*pattern == '*') {
      if(!(*(++pattern))) return true;
      matchPointer = pattern;
      charPointer = filename + 1;
    } else if((*pattern == *filename) || (*pattern == '?')) {
      pattern++;
      filename++;
    } else {
      pattern = matchPointer;
      filename = charPointer++;
    }
  }
  while(*pattern == '*') pattern++;
  return !(*pattern);
}

//---------------------------------------------------------------
// Get path spec and wildcard pattern
// params[in]
//	specs:   Pointer to path spec buffer.
//	pattern: Pointer to buffer to hold wild card pattern if there.
// params[out]
//	returns: true for wildcard pattern found else false.
//----------------------------------------------------------------
bool getWildCard(char *specs, char *pattern) {
	int index, count, len, i;
	len = strlen(specs);
	// If no '.' or len = 0 then no wildcards.
	if(!len || !strchr(specs,'.')) {
		return false;
	}
	index = len;
	// Start at end of string and walk backwards through it until '/' reached.
	while((specs[index] != '/') && (index != 0))
		index--;
	count = len - index; // Reduce length of string (count).
	for(i = 0; i < count; i++) {
			pattern[i] = specs[i+index+1]; // Copy wildcards to pattern string.
	}
	pattern[i] = '\0'; // Terminate strings.
	specs[index+1] = '\0';
	if(pattern[0])
		return true;
	else
		return false;
}

//-------------------------------------------------------------------------------
// List a directory from given path. Can include a volume name delimited with '/'
// '/name/' at the begining of path string.
//-------------------------------------------------------------------------------
bool lsDir(const char *dirPath) {
  char savePath[256];
  char pattern[256];
  pattern[0] = 0;
  int err = 0;
  bool wildcards = false;
  
  // Preserve original path spec. Should only change if chdir() is used.	
  strcpy(savePath,dirPath);
  if(dirPath[0] == 0) return false;

  // wildcards = true if any wildcards used else false.
  wildcards = getWildCard((char *)savePath,pattern);  

  // Make sure path name ends with a '/'.
  uint8_t len = strlen(savePath);
  if(len < 6) return false; // Insure correct partition name given.
  if(savePath[len-1] != '/') {
	savePath[len] = '/';
	savePath[len+1] = '\0';
  }

  // Path name MUST be prefixed with a logical drive specifier "/sdax/"
  DIR *dir = opendir(savePath); // or "/fatx" where 'x' is 1..4.
  if(!dir) {
    printf("Error %d: could not open directory %s\n", -errno, dirPath);
    return false;
  }

  printf("\nDirectory Listing:\n");
  // Show current logical drive name.
  if(msd[sda].type == EXT4_TYPE)
    printf("Volume Label: %s\r\n", extfsp[getMPid(dirPath)]->getVolumeLabel());
  printf("   Full Path: %s%s\r\n", savePath,pattern);

  // If wildcards given just list files that match (no sub directories) else
  // List sub directories first then files.
  if(wildcards) {
    lsFiles(dir, pattern, wildcards, savePath);
  } else {
    lsSubDir(dir);
    lsFiles(dir, pattern, wildcards, savePath);
  }

  err = closedir(dir);
  if (err < 0) {
    printf("Error %d: close directory failed\n",-errno); // Print out error info.
    return false;
  }

//  if(msd[sda].type == EXT4_TYPE)
//    id = getMPid(dirPath); // Get partition id from mount point name (/sdax/).
  printf("\nBytes Total: %llu, Bytes Used:%llu\n", totalSize(dirPath), usedSize(dirPath));
  printf("Bytes Free: %llu\n", freeSize(dirPath));
  return true;
}

//------------------------------------------------------------------
// List directories only.
//------------------------------------------------------------------
bool lsSubDir(void *dir) {
  struct dirent* fsDirEntry;

  DIR *fsDir = reinterpret_cast < DIR * > ( dir );

  rewinddir(fsDir); // Start at beginning of directory.
  // Find next available object to display in the specified directory.
  while (true) {
    fsDirEntry = readdir(fsDir);
    if(!fsDirEntry) { break; }
    if(fsDirEntry->d_type == DT_DIR) { // Only list sub directories.
      printf("%s",fsDirEntry->d_name); // Display SubDir filename.
      for(uint8_t i = 0; i <= (NUMSPACES-strlen(fsDirEntry->d_name)); i++) printf(" ");
      printf("<DIR>\r\n");
    }
  }
  return true;
}

//-----------------------------------------------------------------
// List files only. Proccess wildcards if specified.
//-----------------------------------------------------------------
bool lsFiles(void *dir, const char *pattern, bool wc, const char *path) {
//    DateTimeFields tm;
  char buf[512];
  uint8_t spacing = NUMSPACES;
  struct dirent *fsDirEntry;

  DIR *fsDir = reinterpret_cast < DIR * > ( dir );
  rewinddir(fsDir); // Start at beginning of directory.

  // Find next available object to display in the specified directory.
  while (true) {
    fsDirEntry = readdir(fsDir);
    if(!fsDirEntry) { break; }
    if (wc && !wildcardMatch(pattern, fsDirEntry->d_name)) continue;
    if(fsDirEntry->d_type != DT_DIR) {
      printf("%s",fsDirEntry->d_name); // Display filename.
      uint8_t fnlen = strlen(fsDirEntry->d_name);
      if(fnlen > spacing) {// Check for filename bigger the NUMSPACES.
        spacing += (fnlen - spacing); // Correct for buffer overun.
      } else {
        spacing = NUMSPACES;
      }
      for(int i = 0; i < (spacing-fnlen); i++) printf("%c",' ');
      sprintf(buf, "%s%s", path,fsDirEntry->d_name);
      printf("%-10ld\r\n",fileSize(buf)); // Then filesize.
//    fsDirEntry.getModifyTime(tm);
//    for(uint8_t i = 0; i <= 1; i++) Serial.printf(" ");
//    Serial.printf("%9s %02d %02d %02d:%02d:%02d\r\n", // Show date/time.
//                  months[tm.mon],tm.mday,tm.year + 1900, 
//                  tm.hour,tm.min,tm.sec);
    }
  }
  return true;
}
