#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>

#define bytes uint8_t*

// ---------- Constants ----------
// bytes
const int FILE_LENGTH = 0x4EF854;    // length of a standard 5MB ProFile image
const int SECTOR_SIZE = 0x200;       // bytes per sector
const int DATA_OFFSET = 0x54;        // length of BLU file header
const int TAG_SIZE = 0x14;           // for ProFile disks
// sectors
const int CATALOG_SEC_OFFSET = 61;   // Which sector the catalog listing starts on
const int SECTORS_IN_DISK = 0x2600;  // for 5MB ProFile

// ---------- Variables ----------

bytes image = NULL;
int MDDFSec;
int bitmapSec;
int sFileSec;
int sfileBlockCount;
uint16_t lastUsedHintIndex = 0xFFFB; //seems to be where these start
bool initialized = false;

// ---------- Functions ----------

bytes getImage() {
    assert (image != NULL);
    assert (initialized);
    return image;
}

uint16_t readInt(const bytes data, const int offset) {
    assert(data != NULL);
    return ((data[offset] & 0xFF) << 8) | ((data[offset+1] & 0xFF));
}

uint32_t readLong(const bytes data, const int offset) {
    assert(data != NULL);
    return ((data[offset] & 0xFF) << 24) | ((data[offset+1] & 0xFF) << 16) | ((data[offset+2] & 0xFF) << 8) | ((data[offset+3] & 0xFF));
}

void readFile() {
    FILE *fileptr = fopen("WS_MASTER.dc42", "rb");
    image = (bytes) malloc(FILE_LENGTH * sizeof(uint8_t));
    fread(image, FILE_LENGTH, 1, fileptr);
    fclose(fileptr);
    initialized = true;
}

bytes readSector(const int sector) {
    bytes sec = malloc(SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        sec[i] = 0x00;
    }
    const int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        sec[i] = getImage()[startIdx + i];
    }

    return sec;
}

bytes read4Sectors(const int sector) {
    bytes sec = malloc(SECTOR_SIZE * 4);
    const int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE * 4; i++) {
        sec[i] = getImage()[startIdx + i];
    }

    return sec;
}

bytes readTag(const int sector) {
    bytes tag = malloc(TAG_SIZE);
    const int startIdx = DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE);
    for (int i = 0; i < TAG_SIZE; i++) {
        tag[i] = getImage()[startIdx + i];
    }

    return tag;
}

uint8_t readMDDFByte(const int offset) {
    bytes sec = readSector(MDDFSec);
    const uint8_t b = sec[offset];
    free(sec);
    return b;
}

uint16_t readMDDFInt(const int offset) {
    bytes sec = readSector(MDDFSec);
    const uint16_t i = readInt(sec, offset);
    free(sec);
    return i;
}

uint32_t readMDDFLong(const int offset) {
    bytes sec = readSector(MDDFSec);
    const uint32_t i = readLong(sec, offset);
    free(sec);
    return i;
}

void writeTag(const int sector, const int offset, const uint8_t data) {
    getImage()[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = data;
}

void writeTagInt(const int sector, const int offset, const uint16_t data) {
    getImage()[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = (data >> 8) & 0xFF;
    getImage()[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset + 1] = data & 0xFF;
}

// uses 3 LSB
void writeTag3Byte(const int sector, const int offset, const uint32_t data) {
    getImage()[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = (data >> 16) & 0xFF;
    getImage()[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset + 1] = (data >> 8) & 0xFF;
    getImage()[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset + 2] = data & 0xFF;
}

void writeSector(const int sector, const int offset, const uint8_t data) {
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = data;
}

void writeSectorInt(const int sector, const int offset, const uint16_t data) {
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = (data >> 8) & 0xFF;
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 1] = data & 0xFF;
}

void writeSectorLong(const int sector, const int offset, const uint32_t data) {
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = (data >> 24) & 0xFF;
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 1] = (data >> 16) & 0xFF;
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 2] = (data >> 8) & 0xFF;
    getImage()[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 3] = data & 0xFF;
}

void zeroSector(const int sector) {
    // "tomorrow I want you to take that sector to Anchorhead and have its memory erased. It belongs to us now"
    for (int i = 0; i < SECTOR_SIZE; i++) {
        writeSector(sector, i, 0x00);
    }
}

bool isCatalogSector(const int sec) {
    bytes tag = readTag(sec);
    const bool isCatalogSector = (tag[4] == 0x00 && tag[5] == 0x04);
    free(tag);
    return isCatalogSector;
}

void findMDDFSec() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        const uint16_t type = readInt(tag, 4);
        free(tag);
        if (type == 0x0001) {
            MDDFSec = i;
            printf("mddfsec: 0x%02X\n", MDDFSec);
            return;
        }
    }
}

void findBitmapSec() {
    bitmapSec = (int) readMDDFLong(0x88) + MDDFSec;
}

void findSFileSec() {
    sFileSec = (int) readMDDFLong(0x94) + MDDFSec;
    sfileBlockCount = (int) readMDDFInt(0x9A);
    printf("emptyfile: 0x%02X\n", readMDDFInt(0x9E));
}

void printSectorType(const int sector) {
    bytes tag = readTag(sector);
    const uint16_t type = readInt(tag, 4);
    free(tag);
    // thanks, Ray
    if (type == 0xAAAA) {
        printf("(boot sector)");
    } else if (type == 0xBBBB) {
        printf("(OS loader)");
    } else if (type == 0x0000) {
        printf(""); // free
    } else if (type == 0x0001) {
        printf("(MDDF)");
    } else if (type == 0x0002) {
        printf("(free bitmap)");
    } else if (type == 0x0003) {
        printf("(s-file)");
    } else if (type == 0x0004) {
        printf("(catalog)");
    } else if (type == 0x7FFF) {
        printf("<deleted>");
    } else {
        printf("file 0x%02X", type);
    }
}

uint8_t bitmapByte(const int sec) {
    const int sectorToCorrect = (sec - MDDFSec);
    const int freeBitmapSector = ((sectorToCorrect / 8) / SECTOR_SIZE) + bitmapSec; //8 sectors per byte
    const int byteIndex = (sectorToCorrect / 8) % SECTOR_SIZE;
    bytes s = readSector(freeBitmapSector);
    const uint8_t previousByte = s[byteIndex];
    free(s);
    return previousByte;
}

bool isFreeSector(const int sector) {
    return bitmapByte(sector) == 0x00;//TODO this is supremely cautious, for now. Fix this later
}

void decrementMDDFFreeCount() {
    uint32_t freeCount = readMDDFLong(0xBA);
    freeCount--;
    writeSectorLong(MDDFSec, 0xBA, freeCount);
}

void fixFreeBitmap(const int sec) {
    const int sectorToCorrect = (sec - MDDFSec);
    const int freeBitmapSector = ((sectorToCorrect / 8) / SECTOR_SIZE) + bitmapSec; //8 sectors per byte
    const int byteIndex = (sectorToCorrect / 8) % SECTOR_SIZE;
    const int baseSec = ((sectorToCorrect / 8) * 8) + MDDFSec;

    uint8_t oldByte = bitmapByte(sec);
    uint8_t byteToWrite = oldByte | (1 << (sec - baseSec)); //TODO check for off-by-1 errors here

    bytes s = readSector(freeBitmapSector);
    free(s);
    writeSector(freeBitmapSector, byteIndex, byteToWrite & 0xFF);
}

/*
AABBBBBB BBBBBBBB BBBBBB00 CCCCCCCC ???????? / ________ ________ ________ ____@@@@ @@@@____ ________ ____DDDD DDDD@@@@ @@@@EEEE EEEE____ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ______## ________ ____!!!! __##____ / (0 until end of 0x200)
0A7B7B7B 546F6D2E 4F626A00 2E4F626A 00180000 / 002E0BF8 002E0C00 000000CC 5ED4A24A 228C0100 00000015 0E009D27 FAC7A24A 22A29D27 FACB0000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0C005407 54000C00 00000000 4E56FEFC 206E000C 00000001 00000000 00000000 00000000 00000000 0000000A 00090001 00001BF4 000A0000  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000

A = name length (bytes)
B = name 
C = type (".Obj")
D = creation date (aligns with catalog listing)
E = modification date (aligns with catalog listing)
_ = (standardized? Check more examples?)
@ = ascending? Looks like a date, maybe?
# = # of sectors (?)
! = Sector offset to first sector of data (- MDDFSec)
*/

void writeHintEntry(const int sector, const int startSector, const int sectorCount, const int nameLength, const char* name) {
    zeroSector(sector);
    writeSector(sector, 0, 0x0D); //name length
    for (int i = 0; i < nameLength; i++) { //bytes we have to write
        writeSector(sector, i + 1, name[i]);
    }
    writeSector(sector, nameLength + 1, 0x00); //padding
    for (int i = 0; i < 4; i++) { //bytes we have to write
        writeSector(sector, nameLength + i + 2, name[nameLength - (3 - i) - 1]);
    }

    writeSectorLong(sector, 34, 0xA24A228C); // date (?)
    writeSectorLong(sector, 38, 0x01000000); // standardized (?)
    writeSectorLong(sector, 42, 0x00150E00); // this may be the serial number of the Lisa, actually. http://www.applerepairmanuals.com/lisa/deserial/pg05.html
    writeSectorLong(sector, 46, 0x9D27FAC7); // creation date (should match file)
    writeSectorLong(sector, 50, 0xA24A22A2); // date (?)
    writeSectorLong(sector, 54, 0x9D27FACB); // modification date (should match file)

    writeSectorLong(sector, 100, 0x4E56FEFC); // standardized (?)
    writeSectorLong(sector, 104, 0x206E000C); // standardized (?)
    writeSectorLong(sector, 108, 0x00000001); // standardized (?)

    writeSectorInt(sector, 130, sectorCount & 0xFF); // number of sectors
    writeSectorLong(sector, 132, 0x00090001); // standardized (?)

    writeSectorInt(sector, 138, startSector - MDDFSec); // offset to first sector of data (-MDDFSec)
    writeSectorInt(sector, 140, sectorCount & 0xFF); // number of sectors, again (?)
}

void claimNextFreeHintSector(const int sec, const int startSector, const int sectorCount, const int nameLength, const char* name) {
    //version
    writeTagInt(sec, 0, 0x0000);

    //volid (TODO 0x0100 seems standard for this type of record at least?)
    writeTagInt(sec, 2, 0x0100);

    //fileid (seems to decrement)
    writeTagInt(sec, 4, --lastUsedHintIndex);

    //dataused (0x8000 seems standard)
    writeTagInt(sec, 6, 0x8000);

    //abspage
    writeTag3Byte(sec, 8, sec - MDDFSec);

    //index 11 is a checksum we'll in later

    //relpage (0x0000 always)
    writeTagInt(sec, 12, 0x0000);

    //fwdlink (0xFFFFFF always)
    writeTag3Byte(sec, 14, 0xFFFFFF);

    //bkwdlink (0xFFFFFF always)
    writeTag3Byte(sec, 17, 0xFFFFFF);

    writeHintEntry(sec, startSector, sectorCount, nameLength, name);

    fixFreeBitmap(sec);

    decrementMDDFFreeCount();
}

int getSectorCount(const int fileSize) {
    return (int) ceil((double) fileSize / SECTOR_SIZE);
}

int findNextFreeSFileIndex() {
    const uint16_t slist_packing = readMDDFInt(0x98); //number of s_entries per block in slist
    int lastIdx = readMDDFInt(0x9C); //the minimum sfile we can use per the MDDF
    uint16_t idx = 0;
    printf("SFILESEC = 0x%02X\n", sFileSec);
    for (int i = sFileSec; i < (sFileSec + sfileBlockCount); i++) {
        bytes data = readSector(i);
        for (int sfileIdx = 0; sfileIdx < slist_packing; sfileIdx++) {
            int srec = sfileIdx * 14; //length of srecord
            const uint32_t hintAddr = readLong(data, srec);
            printf("IDX = 0x%02X: ", idx);
            printf("hintAddr = 0x%08X, ", hintAddr);
            printf("fileAddr = 0x%08X, ", readLong(data, srec + 4));
            printf("fileSize = 0x%08X, ", readLong(data, srec + 8));
            printf("version = 0x%04X\n", readInt(data, srec + 12));
            if (hintAddr != 0x00000000) { // claimed s-record
                bytes hintTag = readTag((int) hintAddr + MDDFSec);
                const uint16_t index = readInt(hintTag, 4);
                free(hintTag);
                if (index < lastUsedHintIndex && index != 0x0000) { //index is 0x0000 for the 4 reserved S-file entries at the start of the listing
                    lastUsedHintIndex = index;
                }
                lastIdx = idx;
            }
            idx++;
        }
        free(data);
    }

    lastIdx++; //next one is free, then
    return lastIdx;
}


//returns the index of the s-file (the file ID)
uint16_t claimNextFreeSFileIndex(const int startSector, const int fileSize, const int sectorCount, const int nameLength, const char *name) {
    int emptyFile = readMDDFInt(0x9E);

    const int whereToStart = sFileSec + sfileBlockCount; // TODO start after this, roughly. Might need to be more stringent

    const uint16_t slist_packing = readMDDFInt(0x98); //number of s_entries per block in slist

    //claim it and return it
    const int sFileSectorToWrite = (emptyFile / slist_packing) + sFileSec;
    const int indexWithinSectorToWrite = (emptyFile - ((sFileSectorToWrite - sFileSec) * slist_packing)) * 14; //length of srecord
    for (int s = whereToStart; s < SECTORS_IN_DISK; s++) {
        if (isFreeSector(s)) {
            printf("Claiming new s-file at index=0x%04X, hint sector=0x%08X, fileAddr=0x%08X, fileSize=0x%08X\n", emptyFile, s, startSector, fileSize);
            //TODO responsibleSector could overflow to the next one if we're unlucky. For now, don't worry about it.
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite, s - MDDFSec); //location of our hint sector
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 4, startSector - MDDFSec); // fileAddr
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 8, sectorCount * SECTOR_SIZE); // fileSize TODO for now, use physical since it's likely safer
            writeSectorInt(sFileSectorToWrite, indexWithinSectorToWrite + 12, 0x0000); //version

            claimNextFreeHintSector(s, startSector, sectorCount, nameLength, name);

            int newEmptyFile = findNextFreeSFileIndex();
            printf("new empty file = 0x%02X\n", newEmptyFile);
            writeSectorInt(MDDFSec, 0x9E, newEmptyFile);

            return emptyFile;
        }
    }

    return -1; // no space
}

void printSFile() {
    const uint16_t slist_packing = readMDDFInt(0x98); //number of s_entries per block in slist
    uint16_t idx = 0;
    for (int i = sFileSec; i < (sFileSec + sfileBlockCount); i++) {
        bytes data = readSector(i);
        for (int sfileIdx = 0; sfileIdx < slist_packing; sfileIdx++) {
            int srec = sfileIdx * 14; //length of srecord
            const uint32_t hintAddr = readLong(data, srec);
            printf("IDX = 0x%02X: ", idx);
            printf("hintAddr = 0x%08X, ", hintAddr);
            printf("fileAddr = 0x%08X, ", readLong(data, srec + 4));
            printf("fileSize = 0x%08X, ", readLong(data, srec + 8));
            printf("version = 0x%04X\n", readInt(data, srec + 12));
            idx++;
        }
        free(data);
    }
}

uint8_t calculateChecksum(const int sector) {
    uint8_t checksumByte = 0x00;
    bytes data = readSector(sector);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        checksumByte = checksumByte ^ (data[i] & 0xFF);
    }
    free(data);

    bytes tag = readTag(sector);
    for (int i = 0; i < TAG_SIZE; i++) {
        if (i != 11) { //the checksum byte isn't included
            checksumByte = checksumByte ^ (tag[i] & 0xFF);
        }
    }
    free(tag);

    return checksumByte;
}

// returns the first sector of the 4
int claimNextFreeCatalogBlock() {
    for (int i = CATALOG_SEC_OFFSET; i < SECTORS_IN_DISK; i += 4) { //let's start looking after where the directories tend to begin
        if (isFreeSector(i) && isFreeSector(i + 1) && isFreeSector(i + 2) && isFreeSector(i + 3)) {
            for (int j = 0; j < 4; j++) {
                //version
                writeTagInt(i+j, 0, 0x0000);

                //volid (TODO 0x2500 seems standard for this disk at least?)
                writeTagInt(i+j, 2, 0x2500);

                //fileid (always 0x0004 for catalog sectors)
                writeTagInt(i+j, 4, 0x0004);

                //dataused (0x8200 seems standard)
                writeTagInt(i+j, 6, 0x8200);

                //abspage
                const int abspage = (i+j) - MDDFSec; //account for magic offset
                writeTag3Byte(i+j, 8, abspage);

                //index 11 is a checksum we'll fill in later

                //relpage
                writeTag(i+j, 12, 0x00);
                writeTag(i+j, 13, j);

                //fwdlink
                if (j == 3) {
                    writeTag3Byte(i+j, 14, 0xFFFFFF);
                } else {
                    const int fwdlink = abspage + 1;
                    writeTag3Byte(i+j, 14, fwdlink);
                }

                //bkwdlink
                if (j == 0) {
                    writeTag3Byte(i+j, 17, 0xFFFFFF);
                } else {
                    const int bkwdlink = abspage - 1;
                    writeTag3Byte(i+j, 17, bkwdlink);
                }
            }
            // "tomorrow I want you to take those sectors to Anchorhead and have their memory erased. They belong to us now"
            zeroSector(i);
            zeroSector(i + 1);
            zeroSector(i + 2);
            zeroSector(i + 3);
            // inscribe the ancient sigil 0x240000 into the start of the first sector to label it as a catalog sector
            writeSector(i, 0, 0x24);
            writeSector(i, 1, 0x00);
            writeSector(i, 2, 0x00);

            writeSector(i, SECTOR_SIZE - 11, 0x00); //0 valid entries here. TODO test this

            fixFreeBitmap(i);
            fixFreeBitmap(i+1);
            fixFreeBitmap(i+2);
            fixFreeBitmap(i+3);

            /* TODO not sure if this is quite right.
            //TODO for testing - forward link from the last used catalog sector
            printf("WRITING FORWARD LINK from sector %d (+3) to %d\n", lastUsedDirSec, i);
            int secToWrite = i - MDDFSec; //magic number
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 6, (secToWrite << 24) & 0xFF);
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 5, (secToWrite << 16) & 0xFF);
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 4, (secToWrite << 8) & 0xFF);
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 3, secToWrite & 0xFF);

            //TODO for testing - backward link to the last catalog sector start
            writeSector(i+3, SECTOR_SIZE - 10, 0x00);
            writeSector(i+3, SECTOR_SIZE - 9, 0x00);
            writeSector(i+3, SECTOR_SIZE - 8, 0x00);
            writeSector(i+3, SECTOR_SIZE - 7, 0x39);

            //0x00FF always ends it
            writeSector(i+3, SECTOR_SIZE - 2, 0x00);
            writeSector(i+3, SECTOR_SIZE - 1, 0x00);
            */

            decrementMDDFFreeCount();
            decrementMDDFFreeCount();
            decrementMDDFFreeCount();
            decrementMDDFFreeCount();

            return i;
        }
    }
    return -1;
}

// Returns 0 if equal, <0 if a < b, >0 if a > b (case-insensitive)
int ci_strncmp(const char *a, const int a_len, const char *b, const int b_len) {
    int i = 0;
    const int min_len = (a_len < b_len) ? a_len : b_len;

    for (i = 0; i < min_len; i++) {
        const int ac = tolower((unsigned char)a[i]);
        const int bc = tolower((unsigned char)b[i]);
        if (ac != bc) {
            return ac - bc;
        }
    }

    // If equal so far, decide based on length
    return a_len - b_len;
}

void incrementMDDFFileCount() {
    uint16_t fileCount = readMDDFInt(0xB0);
    fileCount++;
    writeSectorInt(MDDFSec, 0xB0, fileCount);
}

void writeCatalogEntry(const int offset, const int nextFreeSFileIndex, const int fileSize, const int sectorCount, const int nameLength, const char *name) {
    const int physicalSize = sectorCount * SECTOR_SIZE;

    assert(image != NULL);

    image[offset] = 0x24;

    // write name
    // 35 bytes total, padded with 0x00
    image[offset + 1] = 0x00;
    image[offset + 2] = 0x00;
    int idx = 3;
    for (int i = 0; i < nameLength; i++) {
        image[offset + idx] = name[i];
        idx++;
    }
    while (idx < 36) {
        image[offset + idx] = 0x00;
        idx++;
    }

    const uint8_t restOfEntry[] = {
        0x03, 0x06, //we're a file (lisa says 0x0306)
        (nextFreeSFileIndex >> 8) & 0xFF, nextFreeSFileIndex & 0xFF, //sfile
        0x9D, 0x27, 0xFA, 0x88, //creation date (this one is random, but I know it works)
        0x9D, 0x27, 0xFA, 0x8F, //modification date (this one is random, but I know it works)
        (fileSize >> 24) & 0xFF, (fileSize >> 16) & 0xFF, (fileSize >> 8) & 0xFF, fileSize & 0xFF, //file size
        (physicalSize >> 24) & 0xFF, (physicalSize >> 16) & 0xFF, (physicalSize >> 8) & 0xFF, physicalSize & 0xFF, //physical file size (plus extra to fit sector bounds)
        0x00, 0x01, //fsOvrhd (? - I know this one works so let's just use it for now),
        0x00, 0xF7, //flags (lisa says 0x00F7)
        0xCE, 0x06, 0x00, 0x00 //fileUnused (lisa says 0xCE060000)
    };
    // write the rest
    for (int i = 0; i < (64 - 36); i++) {
        image[offset + idx] = restOfEntry[i];
        idx++;
    }
    incrementMDDFFileCount();
}

void claimNewCatalogEntry(const uint16_t sfileid, const int fileSize, const int sectorCount, const int nameLength, const char *name) {
    bool first = true;
    for (int dirSec = 0; dirSec < SECTORS_IN_DISK; dirSec++) { //in all the possible catalogSectors. They come in 4s, always
        if (!isCatalogSector(dirSec)) {
            continue;
        }
        bytes dirBlock = read4Sectors(dirSec);
        if (!(dirBlock[0] == 0x24 && dirBlock[1] == 0x00 && dirBlock[2] == 0x00)) {
            continue; //invalid block
        }
        int offsetToFirstEntry = 0;
        if (first) {
            first = false;
            offsetToFirstEntry = 0x4E; //seems to be the case for the first catalog sector block only
        }

       const uint8_t validEntryCount = dirBlock[(SECTOR_SIZE * 4) - 11];
       if (validEntryCount < 0x1E) { //TODO dodgy. We have space
            for (int e = 0; e < ((SECTOR_SIZE * 4) / 0x40); e++) { //loop over all entries (0x40 long each, so up to 32 per catalog block)
                const int entryOffset = offsetToFirstEntry + (e * 0x40);
                if (entryOffset >= (SECTOR_SIZE * 4) - offsetToFirstEntry - 0x80) { //there's some standard padding (0x4A?) on the end of these blocks I'd like to leave in place
                    break; //we're past the end, so try again later
                }

                if (((e + 1) < validEntryCount) && dirBlock[entryOffset + 0] == 0x24 && dirBlock[entryOffset + 1] == 0x00 && dirBlock[entryOffset + 2] == 0x00) { //the magic 0x240000 defines the start of a catalog entry
                    // compare my filename against the existing ones.
                    // write it when I can, then write the rest.
                    // if it doesn't match, continue onwards.
                    const int cmp = ci_strncmp(name, nameLength, (char *) dirBlock+entryOffset+3, 32);
                    if (cmp == 0) {
                        return; //illegal to have matching names
                    }
                    if (cmp < 0) {
                        writeSector(dirSec + 3, SECTOR_SIZE - 11, dirBlock[(SECTOR_SIZE*4) - 11] + 1); //claim another valid entry in this sector
                        const int catalogEntryOffset = DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset; //offset to the place to write in the file
                        //shift
                        for (int rest = e; rest < validEntryCount; rest++) {
                            const int originalOffsetOfEntryWithinBlock = offsetToFirstEntry + (rest * 0x40);
                            const int destinationOffsetOfEntryWithinBlock = originalOffsetOfEntryWithinBlock + 0x40;
                            for (int eIdx = 0; eIdx < 64; eIdx++) { //length of catalog record
                                const int off = originalOffsetOfEntryWithinBlock + eIdx;
                                getImage()[DATA_OFFSET + (SECTOR_SIZE * dirSec) + destinationOffsetOfEntryWithinBlock + eIdx] = dirBlock[off];
                            }
                        }

                        writeCatalogEntry(catalogEntryOffset, sfileid, fileSize, sectorCount, nameLength, name);
                        printf("Found space for a new catalog entry (shifting) at offset 0x%X\n", catalogEntryOffset);
                        return;
                    }
                } else {
                    bytes sec = readSector(dirSec + 3);
                    writeSector(dirSec + 3, SECTOR_SIZE - 11, sec[SECTOR_SIZE - 11] + 1); //claim another valid entry in this sector
                    free(sec);
                    writeCatalogEntry(DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset, sfileid, fileSize, sectorCount, nameLength, name);
                    printf("Found space for a new catalog entry (appending) at offset 0x%X\n", entryOffset);
                    return;
                }
            }
       }
        free(dirBlock);
        dirSec += 3; // skip past the rest in this block
    }

    //no space found, so let's make some
    printf("No space found for a new entry. Creating some...\n");
    const int nextFreeBlock = claimNextFreeCatalogBlock();
    printf("Space to create new catalog block claimed at sector = %d\n", nextFreeBlock);
    bytes sec = readSector(nextFreeBlock + 3);
    writeSector(nextFreeBlock + 3, SECTOR_SIZE - 11, sec[SECTOR_SIZE - 11] + 1); //claim another valid entry in this sector
    free(sec);
    writeCatalogEntry(DATA_OFFSET + (nextFreeBlock * SECTOR_SIZE), sfileid, fileSize, sectorCount, nameLength, name);
}

void fixAllTagChecksums() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        const uint8_t checksum = calculateChecksum(i);
        if ((checksum & 0xFF) != (tag[11] & 0xFF)) {
            writeTag(i, 11, (checksum & 0xFF));
        }
        free(tag);
    }
}

void writeFileTagBytes(const int startSector, const int sectorCount, const uint16_t sfileid) {
    for (int i = 0; i < sectorCount; i++) {
        const int sectorToWrite = startSector + i;
        const uint32_t abspage = sectorToWrite - MDDFSec;
        writeTagInt(sectorToWrite, 0, 0x0000); //version (2 bytes)
        writeTagInt(sectorToWrite, 2, 0x0000); //vol (2 bytes)
        writeTagInt(sectorToWrite, 4, sfileid); //file ID (2 bytes)
        writeTagInt(sectorToWrite, 6, 0x8200); //dataused. 2 bytes. 0x8200, standard, it seems
        writeTag3Byte(sectorToWrite, 8, abspage);
        // index 11 is checksum (1 byte - will be fixed later)
        writeTagInt(sectorToWrite, 12, i); // relpage (2 bytes)
        if (i == (sectorCount - 1)) {
            writeTag3Byte(sectorToWrite, 14, 0xFFFFFF); // fwdlink (3 bytes. 0xFFFFFF says none)
        } else {
            writeTag3Byte(sectorToWrite, 14, abspage + 1); // fwdlink (3 bytes. 0xFFFFFF says none)
        }
        if (i == 0) {
            writeTag3Byte(sectorToWrite, 17, 0xFFFFFF); // bkwdlink (3 bytes. 0xFFFFFF says none)
        } else {
            writeTag3Byte(sectorToWrite, 17, abspage - 1); // bkwdlink (3 bytes. 0xFFFFFF says none)
        }
        fixFreeBitmap(sectorToWrite);
        decrementMDDFFreeCount();
    }
}

int findStartingSector(const int contiguousSectors) {
    for (int i = SECTORS_IN_DISK - contiguousSectors - 0x400; i > MDDFSec + 0x400; i--) { //TODO let's start a bit in to be safe. Also start at the end to avoid clobbering by Lisa
        bool free = true;
        for (int j = 0; j < contiguousSectors; j++) {
            if (!isFreeSector(i+j)) {
                free = false;
                break;
            }
        }
        if (free) {
            return i;
        }
    }
    return -1; // not found
}

void writeFile(const int nameLength, const char* name) {
    FILE *fileptr = fopen(name, "rb");     // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);
    const int rawFileSize = (int) ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);
    bytes filedata = (bytes) malloc(rawFileSize * sizeof(uint8_t)); // Enough memory for the file
    fread(filedata, rawFileSize, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file

    const int BLOCK_SIZE = SECTOR_SIZE * 2;

    // write the data to a buffer
    bytes dataBuf = (bytes) malloc(rawFileSize * 2 * sizeof(uint8_t)); // Enough memory for the file with some leeway
    int bytesWritten = 0;
    for (int i = 0; i < BLOCK_SIZE; i++) { //1KB of header on text files
        dataBuf[bytesWritten++] = 0x00;
    }
    printf("    - wrote header. bytesWritten=0x%02X\n", bytesWritten);
    bool justWroteSemi = false;
    bool justWroteNewline = false;
    for (int i = 0; i < rawFileSize; i++) { // for every byte of the input data
        uint8_t b = filedata[i];
        if (b == 0x0A) {
            b = 0x0D; //replace Mac style line breaks with Lisa style
        }
        if (bytesWritten % BLOCK_SIZE == BLOCK_SIZE - 1) {
            printf("ERROR! There was no padding added here.\n");
        }
        if (bytesWritten % BLOCK_SIZE > (BLOCK_SIZE - 0x120) && justWroteNewline) {
            const int padding = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
            printf("        - bytesWritten = 0x%02X, adding 0x%02X bytes of padding\n", bytesWritten, padding);
            for (int j = 0; j < padding; j++) {
                //write the footer to each sector
                dataBuf[bytesWritten++] = 0x00;
            }
            dataBuf[bytesWritten++] = b;
            justWroteNewline = false;
        } else {
            dataBuf[bytesWritten++] = b;
            if (b == 0x3B || b == 0x7D) { //; or }
                justWroteSemi = true;
                justWroteNewline = false;
            } else if (b == 0x0D) {
                if (justWroteSemi) {
                    justWroteNewline = true;
                } else {
                    justWroteSemi = false;
                    justWroteNewline = false;
                }
            } else {
                justWroteSemi = false;
                justWroteNewline = false;
            }
        }
    }
    printf("    - wrote data. bytesWritten=0x%2X\n", bytesWritten);
    const int remaining = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
    printf("    - (remaining = 0x%2X)\n", remaining);
    for (int i = 0; i < remaining; i++) {
        dataBuf[bytesWritten++] = 0x00;
    }
    printf("    - wrote end. bytesWritten=0x%2X\n", bytesWritten);

    printf("Final buffer length = 0x%2X\n", bytesWritten);

    // do the work
    const int sectorCount = getSectorCount(bytesWritten);
    printf("sectorCount = 0x%2X\n", sectorCount);
    const int startSector = findStartingSector(sectorCount); // allocate contiguously to be nice about it

    const uint16_t sfileid = claimNextFreeSFileIndex(startSector, bytesWritten, sectorCount, nameLength, name);

    claimNewCatalogEntry(sfileid, bytesWritten, sectorCount, nameLength, name);

    // write the data from buffer
    for (int i = 0; i < sectorCount; i++) {
        for (int j = 0; j < SECTOR_SIZE; j++) {
            writeSector(startSector + i, j, dataBuf[(i * SECTOR_SIZE) + j]);
        }
    }

    writeFileTagBytes(startSector, sectorCount, sfileid);
    free(dataBuf);
    free(filedata);
}

int main (int argc, char *argv[]) {
    FILE *output = fopen("WS_new.dc42", "w");
    //initialize all global vars
    readFile();
    findMDDFSec();
    findBitmapSec();
    findSFileSec();

    //printSFile();

    /*
    for (int i = 0; i < 0; i++) {
        const uint8_t calculatedChecksum = calculateChecksum(i);
        printf("sec %d (0x%02X) (offset=0x%02X) with chksum 0x%02X:", i, i, DATA_OFFSET + (i * SECTOR_SIZE), (calculatedChecksum & 0xFF));
        bytes tag = readTag(i);
        for (int j = 0; j < TAG_SIZE; j++) {
            printf("%02X ", tag[j] & 0xFF);
        }
        printf(" ");
        printSectorType(i);
        if (i > MDDFSec) {
            printf(" ");
            printf("0x%02X", bitmapByte(i));
        }
        printf("\n");
    }
    */

    // get the file we want to write
    writeFile(7, "g2.text");
    writeFile(7, "g3.text");

    // cleanup and close

    fixAllTagChecksums();
    fwrite(image, 1, FILE_LENGTH, output);
    fclose(output);


    return 0;
}
