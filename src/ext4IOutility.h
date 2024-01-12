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

#ifndef __EXT4IOUTILITY_H__
#define __EXT4IOUTILITY_H__
#include <Arduino_USBHostMbed5.h>
#include <FATFileSystem.h>
#include "EXT4FileSystem.h"
#include "MBRBlockDevice.h"
#include <errno.h>

// Uncomment to use a FAT32 USB Drive
#define USE_FAT32_DRIVE 1

//USB devices
#define sda 0
#define sdb 1

// EXT4 Partitions
#define sda1 0
#define sda2 1
#define sda3 2
#define sda4 3

// FAT32 Partitions
#define fat1 4
#define fat2 5
#define fat3 6
#define fat4 7

// Max number of partitions per USB drive.
#define NUM_USB_LOGICAL_DRVS 4
// Total number of Logical Drives.
#define TOTAL_LD 8
// Directory spaceing size.
#define NUMSPACES 50
// Adjust this to set how long we wait for a MSD device to connect.
#define CONNECT_TIMEOUT 30000

// USB drive and fs type struct.
typedef struct drvType {
  USBHostMSD *drive = nullptr;
  int type = -1;
  bool connected = false;
  bool initialized = false;
  bool mounted = false;    
} drvType_t;


drvType_t *getMSDinfo(uint8_t drv);

// Partition device names.
const char mpID[TOTAL_LD][7] = 
{	"/sda1/",
	"/sda2/",
	"/sda3/",
	"/sda4/",
	"/fat1/",
	"/fat2/",
	"/fat3/",
	"/fat4/"
};

mbed::EXT4FileSystem *getExt4fs(void);
mbed::FATFileSystem *getFat32fs(void);

bool connectInitialized(USBHostMSD *drv, uint8_t index);
bool checkAvailable(uint8_t drv);
bool exists(const char * path);
int getMPid(const char * volName);
uint64_t usedSize(const char * ld);
uint64_t totalSize(const char * ld);
uint64_t freeSize(const char *ld);
void dumpDrvType(uint8_t drv);
uint32_t fileSize(const char * path);
int mountDrives(USBHostMSD *drv, uint8_t index);
int umountDrives(uint8_t index);
const char *entry_to_str(uint8_t type);
bool wildcardMatch(const char *pattern, const char *filename);
bool getWildCard(char *specs, char *pattern);
bool lsDir(const char *dirPath);
bool lsSubDir(void *dir);
bool lsFiles(void *dir, const char *pattern, bool wc, const char *path);
#endif
