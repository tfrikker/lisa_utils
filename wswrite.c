#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define bytes uint8_t*

// ---------- Constants ----------
// bytes
const int FILE_LENGTH = 0x4EF854; // length of a standard 5MB ProFile image
const int SECTOR_SIZE = 0x200; // bytes per sector
const int DATA_OFFSET = 0x54; // length of BLU file header
const int TAG_SIZE = 0x14; // for ProFile disks
// sectors
const int CATALOG_SEC_OFFSET = 61; // Which sector the catalog listing starts on
const int SECTORS_IN_DISK = 0x2600; // for 5MB ProFile
// MDDF offsets
const uint8_t MDDF_BITMAP_ADDR = 0x88;
const uint8_t MDDF_SLIST_ADDR = 0x94;
const uint8_t MDDF_SLIST_PACKING = 0x98;
const uint8_t MDDF_SLIST_BLOCK_COUNT = 0x9A;
const uint8_t MDDF_FIRST_FILE = 0x9C;
const uint8_t MDDF_EMPTY_FILE = 0x9E;
const uint8_t MDDF_FILECOUNT = 0xB0;
const uint8_t MDDF_FREECOUNT = 0xBA;
const uint16_t MDDF_ROOT_PAGE = 0x12E;
// s-file
const int SFILE_RECORD_LENGTH = 14;
// catalog
const int CATALOG_RECORD_LENGTH = 64;
const int CATALOG_NONLEAF_RECORD_LENGTH = 0x28;
// file IDs
const uint16_t FREE_FILE_ID = 0x0000;
const uint16_t MDDF_FILE_ID = 0x0001;
const uint16_t BITMAP_FILE_ID = 0x0002;
const uint16_t SFILE_FILE_ID = 0x0003;
const uint16_t CATALOG_FILE_ID = 0x0004;
const uint16_t DELETED_FILE_ID = 0x7FFF;
const uint16_t BOOT_SEC_FILE_ID = 0xAAAA;
const uint16_t OS_LOADER_FILE_ID = 0xBBBB;
const uint16_t INITIAL_HINT_FILE_ID = 0xFFFB; //the file ID for a hint; seems to be where these start

// ---------- Variables ----------

bytes image = NULL;
int MDDFSec;
int bitmapSec;
int sFileSec;
int sfileBlockCount;
uint16_t lastUsedHintIndex = INITIAL_HINT_FILE_ID;
bool initialized = false;

// ---------- Functions ----------

bytes getImage() {
    assert(image != NULL);
    assert(initialized);
    return image;
}

uint16_t readInt(const bytes data, const int offset) {
    assert(data != NULL);
    return ((data[offset] & 0xFF) << 8) | ((data[offset + 1] & 0xFF));
}

uint32_t readLong(const bytes data, const int offset) {
    assert(data != NULL);
    return ((data[offset] & 0xFF) << 24) | ((data[offset + 1] & 0xFF) << 16) | ((data[offset + 2] & 0xFF) << 8) | ((data[offset + 3] & 0xFF));
}

void readFile() {
    FILE *fileptr = fopen("WS_MASTER.dc42", "rb");
    image = (bytes) malloc(FILE_LENGTH);
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
    const uint16_t fileId = readInt(tag, 4);
    free(tag);
    return fileId == CATALOG_FILE_ID;
}

void findMDDFSec() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        const uint16_t type = readInt(tag, 4);
        free(tag);
        if (type == MDDF_FILE_ID) {
            MDDFSec = i;
            printf("mddfsec: 0x%02X\n", MDDFSec);
            return;
        }
    }
}

void findBitmapSec() {
    bitmapSec = (int) readMDDFLong(MDDF_BITMAP_ADDR) + MDDFSec;
}

void findSFileSec() {
    sFileSec = (int) readMDDFLong(MDDF_SLIST_ADDR) + MDDFSec;
    sfileBlockCount = (int) readMDDFInt(MDDF_SLIST_BLOCK_COUNT);
    printf("emptyfile: 0x%02X\n", readMDDFInt(MDDF_EMPTY_FILE));
}

void printSectorType(const int sector) {
    bytes tag = readTag(sector);
    const uint16_t type = readInt(tag, 4);
    free(tag);
    // thanks, Ray
    if (type == BOOT_SEC_FILE_ID) {
        printf("(boot sector)");
    } else if (type == OS_LOADER_FILE_ID) {
        printf("(OS loader)");
    } else if (type == FREE_FILE_ID) {
        printf(""); // free
    } else if (type == MDDF_FILE_ID) {
        printf("(MDDF)");
    } else if (type == BITMAP_FILE_ID) {
        printf("(free bitmap)");
    } else if (type == SFILE_FILE_ID) {
        printf("(s-file)");
    } else if (type == CATALOG_FILE_ID) {
        printf("(catalog)");
    } else if (type == DELETED_FILE_ID) {
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
    return bitmapByte(sector) == 0x00; //TODO this is supremely cautious, for now. Fix this later
}

void decrementMDDFFreeCount() {
    uint32_t freeCount = readMDDFLong(MDDF_FREECOUNT);
    freeCount--;
    writeSectorLong(MDDFSec, MDDF_FREECOUNT, freeCount);
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

void writeHintEntry(const int sector, const int startSector, const int sectorCount, const int nameLength, const char *name) {
    zeroSector(sector);
    writeSector(sector, 0, nameLength); //name length
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

void claimNextFreeHintSector(const int sec, const int startSector, const int sectorCount, const int nameLength, const char *name) {
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
    const uint16_t slist_packing = readMDDFInt(MDDF_SLIST_PACKING); //number of s_entries per block in slist
    int lastIdx = readMDDFInt(MDDF_FIRST_FILE); //the minimum sfile we can use per the MDDF
    uint16_t idx = 0;
    for (int i = sFileSec; i < (sFileSec + sfileBlockCount); i++) {
        bytes data = readSector(i);
        for (int sfileIdx = 0; sfileIdx < slist_packing; sfileIdx++) {
            int srec = sfileIdx * SFILE_RECORD_LENGTH;
            const uint32_t hintAddr = readLong(data, srec);
            /*
            printf("IDX = 0x%02X: ", idx);
            printf("hintAddr = 0x%08X, ", hintAddr);
            printf("fileAddr = 0x%08X, ", readLong(data, srec + 4));
            printf("fileSize = 0x%08X, ", readLong(data, srec + 8));
            printf("version = 0x%04X\n", readInt(data, srec + 12));
            */
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
uint16_t claimNextFreeSFileIndex(const int startSector, const int sectorCount, const int nameLength, const char *name) {
    const int emptyFile = readMDDFInt(MDDF_EMPTY_FILE);

    const int whereToStart = sFileSec + sfileBlockCount; // TODO start after this, roughly. Might need to be more stringent

    const uint16_t slist_packing = readMDDFInt(MDDF_SLIST_PACKING); //number of s_entries per block in slist

    //claim it and return it
    const int sFileSectorToWrite = (emptyFile / slist_packing) + sFileSec;
    const int indexWithinSectorToWrite = (emptyFile - ((sFileSectorToWrite - sFileSec) * slist_packing)) * SFILE_RECORD_LENGTH; //length of srecord
    for (int s = whereToStart; s < SECTORS_IN_DISK; s++) {
        if (isFreeSector(s)) {
            //printf("Claiming new s-file at index=0x%04X, hint sector=0x%08X, fileAddr=0x%08X, fileSize=0x%08X\n", emptyFile, s, startSector, fileSize);
            //TODO responsibleSector could overflow to the next one if we're unlucky. For now, don't worry about it.
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite, s - MDDFSec); //location of our hint sector
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 4, startSector - MDDFSec); // fileAddr
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 8, sectorCount * SECTOR_SIZE); // fileSize TODO for now, use physical since it's likely safer
            writeSectorInt(sFileSectorToWrite, indexWithinSectorToWrite + 12, 0x0000); //version

            claimNextFreeHintSector(s, startSector, sectorCount, nameLength, name);

            int newEmptyFile = findNextFreeSFileIndex();
            writeSectorInt(MDDFSec, MDDF_EMPTY_FILE, newEmptyFile);

            return emptyFile;
        }
    }

    return -1; // no space
}

void printSFile() {
    const uint16_t slist_packing = readMDDFInt(MDDF_SLIST_PACKING); //number of s_entries per block in slist
    uint16_t idx = 0;
    for (int i = sFileSec; i < (sFileSec + sfileBlockCount); i++) {
        bytes data = readSector(i);
        for (int sfileIdx = 0; sfileIdx < slist_packing; sfileIdx++) {
            int srec = sfileIdx * SFILE_RECORD_LENGTH; //length of srecord
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
                writeTagInt(i + j, 0, 0x0000);

                //volid (TODO 0x2500 is used sometimes for this disk at least?)
                writeTagInt(i + j, 2, 0x0000);

                //fileid (always 0x0004 for catalog sectors)
                writeTagInt(i + j, 4, 0x0004);

                //dataused (0x8200 seems standard)
                writeTagInt(i + j, 6, 0x8200);

                //abspage
                const int abspage = (i + j) - MDDFSec; //account for magic offset
                writeTag3Byte(i + j, 8, abspage);

                //index 11 is a checksum we'll fill in later

                //relpage
                writeTag(i + j, 12, 0x00);
                writeTag(i + j, 13, j);

                //fwdlink
                if (j == 3) {
                    writeTag3Byte(i + j, 14, 0xFFFFFF);
                } else {
                    const int fwdlink = abspage + 1;
                    writeTag3Byte(i + j, 14, fwdlink);
                }

                //bkwdlink
                if (j == 0) {
                    writeTag3Byte(i + j, 17, 0xFFFFFF);
                } else {
                    const int bkwdlink = abspage - 1;
                    writeTag3Byte(i + j, 17, bkwdlink);
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

            writeSector(i, SECTOR_SIZE - 11, 0x00); //0 valid entries here.

            for (int j = 0; j < 31; j++) { //let's try 31
                writeSectorInt(i+3, SECTOR_SIZE - 14 - (j * 2), j * CATALOG_RECORD_LENGTH); // set up the special index entries (not sure of the actual name)
            }

            writeSectorLong(i + 3, SECTOR_SIZE - 10, 0xFFFFFFFF); //10-9-8-7
            writeSectorLong(i + 3, SECTOR_SIZE - 6, 0xFFFFFFFF); //6-5-4-3

            writeSectorInt(i + 3, SECTOR_SIZE - 2, 0x00FF); //2-1 standard

            fixFreeBitmap(i);
            fixFreeBitmap(i + 1);
            fixFreeBitmap(i + 2);
            fixFreeBitmap(i + 3);

            decrementMDDFFreeCount();
            decrementMDDFFreeCount();
            decrementMDDFFreeCount();
            decrementMDDFFreeCount();

            return i;
        }
    }
    return -1;
}

// Returns true if a < b, false otherwise (case insensitive)
bool ci_a_before_b(const char *a, const int a_len, const char *b, const int b_len) {
    int i = 0;
    const int min_len = (a_len < b_len) ? a_len : b_len;

    for (i = 0; i < min_len; i++) {
        const int ac = toupper((unsigned char) a[i]);
        const int bc = toupper((unsigned char) b[i]);
        if (ac != bc) {
            return ac < bc;
        }
    }

    // If equal so far, decide based on length
    return a_len < b_len;
}

void incrementMDDFFileCount() {
    uint16_t fileCount = readMDDFInt(MDDF_FILECOUNT);
    fileCount++;
    writeSectorInt(MDDFSec, MDDF_FILECOUNT, fileCount);
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
    for (int i = 0; i < (CATALOG_RECORD_LENGTH - 36); i++) {
        image[offset + idx] = restOfEntry[i];
        idx++;
    }
    incrementMDDFFileCount();
}

uint8_t getCatalogEntryCountForBlock(const int dirSec) {
    bytes sec = readSector(dirSec + 3);
    const uint8_t count = sec[SECTOR_SIZE - 11];
    free(sec);
    return count;
}

void claimNewCatalogEntrySpace(const int dirSec, const int entryOffset, const int sfileid, const int fileSize, const int sectorCount, const int nameLength, const char *name) {
    int entry = getCatalogEntryCountForBlock(dirSec) + 1;
    writeSector(dirSec + 3, SECTOR_SIZE - 11, entry); //claim another valid entry in this sector
    writeCatalogEntry(DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset, sfileid, fileSize, sectorCount, nameLength, name);
}

// given a filename, return the beginning sector number of the relevant catalog block.
// - if contained by an existing block, return that block regardless if it has space or not
// - if not contained by an existing block, return the block that ends closest (alphanumerically) to the filename, regardless if it has space or not
int findRelevantCatalogSector(const int nameLength, const char *name) {
    bool first = true;
    int closestDirSec = -1;
    char *closestDirName = NULL;
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
            closestDirSec = dirSec; // if we're the first entry, we'll need somewhere to go
            first = false;
            offsetToFirstEntry = 0x4E; //seems to be the case for the first catalog sector block only
        }

        const uint8_t validEntryCount = getCatalogEntryCountForBlock(dirSec);
        const int firstEntryOffset = offsetToFirstEntry;
        const int lastEntryOffset = offsetToFirstEntry + ((validEntryCount - 1) * CATALOG_RECORD_LENGTH);

        const bool nameBeforeFirst = ci_a_before_b(name, nameLength, (char *) dirBlock + firstEntryOffset + 3, 32);
        const bool nameBeforeLast = ci_a_before_b(name, nameLength, (char *) dirBlock + lastEntryOffset + 3, 32);

        if (!nameBeforeFirst && nameBeforeLast) {
            free(dirBlock);
            if (closestDirName != NULL) {
                free(closestDirName);
            }
            printf("Contained in a block!\n");
            return dirSec; // this block contains us
        }

        if (!nameBeforeLast) { //if this block ends before us
            if (closestDirName == NULL) { //the first time
                closestDirName = (char *) malloc(32 * sizeof(char));
                for (int i = 0; i < 32; i++) {
                    closestDirName[i] = (char) dirBlock[lastEntryOffset + 3 + i];
                }
                closestDirSec = dirSec;
            } else {
                // compare the endings to see who's closer. Keep a running count
                const bool lastAfterRunningClosest = ci_a_before_b(closestDirName, 32, (char *) dirBlock + lastEntryOffset + 3, 32);
                if (lastAfterRunningClosest) {
                    for (int i = 0; i < 32; i++) {
                        closestDirName[i] = (char) dirBlock[lastEntryOffset + 3 + i];
                    }
                    closestDirSec = dirSec;
                    printf("closestDirSec now = %d\n", closestDirSec);
                }
            }
        }

        free(dirBlock);
        dirSec += 3; // skip past the rest in this block
    }
    if (closestDirName != NULL) {
        free(closestDirName);
    }
    printf("Not contained in any sector\n");
    return closestDirSec; // we weren't contained in any, so return the closest one
}

int getEntryToMove(bytes dirBlock, int offsetToFirstEntry, int entryCount, char *name, int nameLength, int recordLength, int nameOffset) {
    for (int e = 0; e < entryCount; e++) {
        const int entryOffset = offsetToFirstEntry + (e * recordLength);
        const bool nameBeforeExistingEntry = ci_a_before_b(name, nameLength, (char *) dirBlock + entryOffset + nameOffset, 32);
        if (nameBeforeExistingEntry) {
            return e;
        }
    }
    return entryCount;
}

void claimNewCatalogEntry(const uint16_t sfileid, const int fileSize, const int sectorCount, const int nameLength, char *name) {
    const int firstCatalogSector = (int) readMDDFLong(MDDF_ROOT_PAGE);
    const int relevantCatalogSec = findRelevantCatalogSector(nameLength, name);
    uint8_t entryCount = getCatalogEntryCountForBlock(relevantCatalogSec);
    printf("The relevant catalog sec is: 0x%02X and the count is 0x%02X, and the variable is 0x%02X\n", relevantCatalogSec, getCatalogEntryCountForBlock(relevantCatalogSec), entryCount);
    int offsetToFirstEntry = 0;
    if (relevantCatalogSec == firstCatalogSector) { //TODO this is a hack to fix the fact there's a directory in the first catalog block.
        offsetToFirstEntry = 0x4E;
        entryCount--;
    }
    bytes dirBlock = read4Sectors(relevantCatalogSec);
    int entryToMove = getEntryToMove(dirBlock, offsetToFirstEntry, entryCount, name, nameLength, CATALOG_RECORD_LENGTH, 3);

    if (entryCount < 0x1D) { // TODO: should be 1E, if there's space to add the new entry to this block
        if (entryToMove != -1) { // if we fit in the middle
            const int entryOffset = offsetToFirstEntry + (entryToMove * CATALOG_RECORD_LENGTH);
            const int catalogEntryOffset = DATA_OFFSET + (relevantCatalogSec * SECTOR_SIZE) + entryOffset; //offset to the place to write in the file
            printf("Found space for a new catalog entry (shifting) at offset 0x%X\n", catalogEntryOffset);
            //shift
            for (int rest = entryToMove; rest < entryCount; rest++) {
                const int originalOffsetOfEntryWithinBlock = offsetToFirstEntry + (rest * CATALOG_RECORD_LENGTH);
                const int destinationOffsetOfEntryWithinBlock = originalOffsetOfEntryWithinBlock + CATALOG_RECORD_LENGTH;
                for (int eIdx = 0; eIdx < CATALOG_RECORD_LENGTH; eIdx++) {
                    const int off = originalOffsetOfEntryWithinBlock + eIdx;
                    getImage()[DATA_OFFSET + (SECTOR_SIZE * relevantCatalogSec) + destinationOffsetOfEntryWithinBlock + eIdx] = dirBlock[off];
                }
            }

            claimNewCatalogEntrySpace(relevantCatalogSec, entryOffset, sfileid, fileSize, sectorCount, nameLength, name);
            free(dirBlock);
            return;
        }
        //if we have space at the end
        printf("We fit at the end.\n");
        const int entryOffset = offsetToFirstEntry + (CATALOG_RECORD_LENGTH * entryCount);
        printf("Found space for a new catalog entry (appending) at offset 0x%X\n", entryOffset);
        claimNewCatalogEntrySpace(relevantCatalogSec, entryOffset, sfileid, fileSize, sectorCount, nameLength, name);
        free(dirBlock);
    } else {
        //no space found, so let's make some
        printf("No space found for a new entry (entryCount = 0x%02X). Creating some...\n", entryCount);
        const int nextFreeBlock = claimNextFreeCatalogBlock();
        printf("Space to create new catalog block claimed at sector = %d\n", nextFreeBlock);
        printf("Entry index to move from old block is: 0x%02X\n", entryToMove);
        //if we're placed into the middle of a sector that's full, move the entries after us to the new one...
        if (entryToMove == entryCount) {
            entryToMove = entryCount - 2; //...but if we're placed at the *end* of a block that's full, move some of the entries, arbitrarily
        }
        int movedEntries = 0;
        bool first = true;
        char *firstname = malloc(36);
        for (int e = entryToMove; e < entryCount; e++) { //todo move all entries after us
            const int entryOffsetInSource = offsetToFirstEntry + (e * CATALOG_RECORD_LENGTH);
            const int entryOffsetInDestination = (movedEntries * CATALOG_RECORD_LENGTH);
            if (first) {
                first = false;
                for (int i = 0; i < 36; i++) {
                    firstname[i] = getImage()[DATA_OFFSET + (relevantCatalogSec * SECTOR_SIZE) + entryOffsetInSource + 3 + i];
                }
            }
            for (int j = 0; j < CATALOG_RECORD_LENGTH; j++) {
                getImage()[DATA_OFFSET + (nextFreeBlock * SECTOR_SIZE) + entryOffsetInDestination + j] = getImage()[DATA_OFFSET + (relevantCatalogSec * SECTOR_SIZE) + entryOffsetInSource + j];
            }
            movedEntries++;
        }

        // for the first moved entry, fix the non-leaf
        for (int d = 0; d < SECTORS_IN_DISK; d++) {
            bool found = false;
            //in all the possible catalogSectors. They come in 4s, always
            if (!isCatalogSector(d)) {
                continue;
            }
            printf("CHECKING CATALOG FOR NON-LEAF: 0x%02X\n", d);
            bytes nonleaf = read4Sectors(d);
            for (int jjj = 0; jjj < 20; jjj++) {
                printf("%02X", nonleaf[jjj]);
            }
            printf("\n");
            if (nonleaf[0] == 0x24 && nonleaf[1] == 0x00 && nonleaf[2] == 0x00) {
                printf("THIS IS A REAL ONE.\n");
                free(nonleaf);
                nonleaf = NULL;
                d += 3;
                continue; //leaf
            }
            printf("WE FOUND OUR NON-LEAF!");

            int nonLeafEntryCount = getImage()[DATA_OFFSET + (SECTOR_SIZE * (d + 3 + 1)) - 11];
            int nonLeafEntryToMove = getEntryToMove(nonleaf, 0, nonLeafEntryCount, firstname, 32, CATALOG_NONLEAF_RECORD_LENGTH, 7);

            for (int rest = nonLeafEntryToMove; rest < nonLeafEntryCount; rest++) {
                printf("Moving entry at index = %d: ", rest);
                const int originalOffsetOfNonLeafEntryWithinBlock = (rest * CATALOG_NONLEAF_RECORD_LENGTH);
                const int destinationOffsetOfNonLeafEntryWithinBlock = originalOffsetOfNonLeafEntryWithinBlock + CATALOG_NONLEAF_RECORD_LENGTH;
                for (int eIdx = 0; eIdx < CATALOG_NONLEAF_RECORD_LENGTH; eIdx++) {
                    const int off = originalOffsetOfNonLeafEntryWithinBlock + eIdx;
                    printf("%02X", nonleaf[off]);
                    getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + destinationOffsetOfNonLeafEntryWithinBlock + eIdx] = nonleaf[off];
                }
                printf("\n");
            }

            int os = nonLeafEntryToMove * CATALOG_NONLEAF_RECORD_LENGTH;
            uint32_t newBk = (uint32_t) (nextFreeBlock - MDDFSec);
            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os] = (newBk >> 24) & 0xFF;
            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 1] = (newBk >> 16) & 0xFF;
            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 2] = (newBk >> 8) & 0xFF;
            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 3] = newBk & 0xFF;

            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 4] = 0x24;
            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 5] = 0x00;
            getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 6] = 0x00;
            for (int kk = 0; kk < 32; kk++) {
                getImage()[DATA_OFFSET + (SECTOR_SIZE * d) + os + 7 + kk] = firstname[kk];
            }
            getImage()[DATA_OFFSET + (SECTOR_SIZE * (d + 3 + 1)) - 11] = nonLeafEntryCount + 1; //increment entry count
            free(nonleaf);
            nonleaf = NULL;
            found = true;
            break;
        }
        free(firstname);

        // fix valid counts
        writeSector(relevantCatalogSec + 3, SECTOR_SIZE - 11, getCatalogEntryCountForBlock(relevantCatalogSec) - movedEntries);
        writeSector(nextFreeBlock + 3, SECTOR_SIZE - 11, movedEntries + 1);

        bytes srcSec = readSector(relevantCatalogSec + 3);
        const uint32_t forward = readLong(readSector(relevantCatalogSec + 3), SECTOR_SIZE - 6);
        free(srcSec);

        //fix linked list of blocks.
        writeSectorLong(relevantCatalogSec + 3, SECTOR_SIZE - 6, (uint32_t) (nextFreeBlock - MDDFSec));
        writeSectorLong(forward + MDDFSec + 3, SECTOR_SIZE - 10, (uint32_t) (nextFreeBlock - MDDFSec));

        writeSectorLong(nextFreeBlock + 3, SECTOR_SIZE - 10, (uint32_t) (relevantCatalogSec - MDDFSec));
        writeSectorLong(nextFreeBlock + 3, SECTOR_SIZE - 6, (uint32_t) forward);

        // recursively re-call this because we have more space now
        claimNewCatalogEntry(sfileid, fileSize, sectorCount, nameLength, name);
    }
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
            if (!isFreeSector(i + j)) {
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

void writeFile(const char *srcFileName, char *name, bool isPascal, bool isText) {
    const int nameLength = strlen(name);
    printf("_________________ Writing file: ");
    for (int i = 0; i < nameLength; i++) {
        printf("%c", name[i]);
    }
    printf(" ________________\n");
    char fullpath[256];
    fullpath[0] = '\0';
    strcat(fullpath, "toinsert/");
    strcat(fullpath, srcFileName);
    FILE *fileptr = fopen(fullpath, "rb"); // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);
    const int rawFileSize = (int) ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);
    bytes filedata = malloc(rawFileSize); // Enough memory for the file
    fread(filedata, rawFileSize, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file

    const int BLOCK_SIZE = SECTOR_SIZE * 2;

    // write the data to a buffer
    bytes dataBuf = malloc(rawFileSize * 4); // Enough memory for the file with some leeway
    int bytesWritten = 0;
    if (isText) {
        for (int i = 0; i < BLOCK_SIZE; i++) { //1KB of header on text files
            dataBuf[bytesWritten++] = 0x00;
        }
    }
    //printf("    - wrote header. bytesWritten=0x%02X\n", bytesWritten);
    bool justWroteSemi = false;
    bool justWroteNewline = false;
    for (int i = 0; i < rawFileSize; i++) { // for every byte of the input data
        uint8_t b = filedata[i];
        //printf("%c", b);
        if (b == 0x0A) {
            b = 0x0D; //replace Mac style line breaks with Lisa style
        }
        if (isText && (bytesWritten % BLOCK_SIZE == BLOCK_SIZE - 1)) {
            printf("ERROR! There was no padding added here.\n");
            assert(false);
        }
        if (bytesWritten % BLOCK_SIZE > (BLOCK_SIZE - 0x190) && justWroteNewline) {
            const int padding = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
            //printf("        - bytesWritten = 0x%02X, adding 0x%02X bytes of padding\n", bytesWritten, padding);
            for (int j = 0; j < padding; j++) {
                //write the footer to each sector
                dataBuf[bytesWritten++] = 0x00;
            }
            dataBuf[bytesWritten++] = b;
            justWroteNewline = false;
        } else {
            dataBuf[bytesWritten++] = b;
            if (b == ';' || b == '}' || b == ')') { //; or }
                justWroteSemi = true;
                justWroteNewline = false;
            } else if (b == 0x0D) {
                if (justWroteSemi || !isPascal) {
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
    //printf("    - wrote data. bytesWritten=0x%2X\n", bytesWritten);
    const int remaining = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
    //printf("    - (remaining = 0x%2X)\n", remaining);
    for (int i = 0; i < remaining; i++) {
        dataBuf[bytesWritten++] = 0x00;
    }
    //printf("    - wrote end. bytesWritten=0x%2X\n", bytesWritten);

    //printf("Final buffer length = 0x%2X\n", bytesWritten);

    // do the work
    const int sectorCount = getSectorCount(bytesWritten);
    const int startSector = findStartingSector(sectorCount); // allocate contiguously to be nice about it

    const uint16_t sfileid = claimNextFreeSFileIndex(startSector, sectorCount, nameLength, name);

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
    printf("\n");
}

int main(int argc, char *argv[]) {
    FILE *output = fopen("WS_new.dc42", "w");
    //initialize all global vars
    readFile();
    findMDDFSec();
    findBitmapSec();
    findSFileSec();

    //printSFile();


    for (int i = 0; i < 200; i++) {
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


    // get the file we want to write
    writeFile("angles.text", "libqd/angles.text", false, true);
    writeFile("arcs.text", "libqd/arcs.text", false, true);
    writeFile("bitblt.text", "libqd/bitblt.text", false, true);
    writeFile("bitmaps.text", "libqd/bitmaps.text", false, true);
    writeFile("drawarc.text", "libqd/drawarc.text", false, true);
    writeFile("drawline.text", "libqd/drawline.text", false, true);
    writeFile("drawtext.text", "libqd/drawtext.text", false, true);
    writeFile("fastline.text", "libqd/fastline.text", false, true);
    writeFile("fixmath.text", "libqd/fixmath.text", false, true);
    writeFile("grafasm.text", "libqd/grafasm.text", false, true);
    writeFile("lcursor.text", "libqd/lcursor.text", false, true);
    /*
    writeFile("line2.text", "libqd/line2.text", false, true);
    writeFile("lines.text", "libqd/lines.text", false, true);
    writeFile("ovals.text", "libqd/ovals.text", false, true);
    writeFile("packrgn.text", "libqd/packrgn.text", false, true);
    writeFile("pictures.text", "libqd/pictures.text", false, true);
    writeFile("polygons.text", "libqd/polygons.text", false, true);
    writeFile("putline.text", "libqd/putline.text", false, true);
    writeFile("putoval.text", "libqd/putoval.text", false, true);
    writeFile("putrgn.text", "libqd/putrgn.text", false, true);
    writeFile("rects.text", "libqd/rects.text", false, true);
    writeFile("regions.text", "libqd/regions.text", false, true);
    writeFile("rgnblt.text", "libqd/rgnblt.text", false, true);
    writeFile("rgnop.text", "libqd/rgnop.text", false, true);
    writeFile("rrects.text", "libqd/rrects.text", false, true);
    writeFile("seekrgn.text", "libqd/seekrgn.text", false, true);
    writeFile("sortpoints.text", "libqd/sortpoints.text", false, true);
    writeFile("stretch.text", "libqd/stretch.text", false, true);
    writeFile("text.text", "libqd/text.text", false, true);
    writeFile("util.text", "libqd/util.text", false, true);
    */






    // cleanup and close

    fixAllTagChecksums();
    fwrite(image, 1, FILE_LENGTH, output);
    fclose(output);


    return 0;
}
