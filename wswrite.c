#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <strings.h>
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

bytes image;
int MDDFSec;
int bitmapSec;
uint16_t lastUsedHintIndex = 0xFFFB; //seems to be where these start

// ---------- Functions ----------

uint16_t readInt(bytes data, int offset) {
    return ((data[offset] & 0xFF) << 8) | ((data[offset+1] & 0xFF));
}

uint32_t readLong(bytes data, int offset) {
    return ((data[offset] & 0xFF) << 24) | ((data[offset+1] & 0xFF) << 16) | ((data[offset+2] & 0xFF) << 8) | ((data[offset+3] & 0xFF));
}

void readFile() {
    FILE *fileptr = fopen("WS_MASTER.dc42", "rb");     // Open the file in binary mode
    image = (bytes) malloc(FILE_LENGTH * sizeof(uint8_t)); // Enough memory for the file
    fread(image, FILE_LENGTH, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file

    printf("Read file: length = 0x%X\n", FILE_LENGTH);
}

bytes readSector(int sector) {
    bytes sec = (bytes) malloc(SECTOR_SIZE * sizeof(uint8_t));
    int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        sec[i] = image[startIdx + i];
    }

    return sec;
}

bytes read4Sectors(int sector) {
    bytes sec = (bytes) malloc(SECTOR_SIZE * sizeof(uint8_t) * 4);
    int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE * 4; i++) {
        sec[i] = image[startIdx + i];
    }

    return sec;
}

bytes readTag(int sector) {
    bytes tag = (bytes) malloc(TAG_SIZE * sizeof(uint8_t));
    int startIdx = DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE);
    for (int i = 0; i < TAG_SIZE; i++) {
        tag[i] = image[startIdx + i];
    }

    return tag;
}

void writeTag(int sector, int offset, uint8_t data) {
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = data;
}

void writeTagInt(int sector, int offset, uint16_t data) {
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = (data >> 8) & 0xFF;
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset + 1] = data & 0xFF;
}

// uses 3 LSB
void writeTag3Byte(int sector, int offset, uint32_t data) {
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = (data >> 16) & 0xFF;
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset + 1] = (data >> 8) & 0xFF;
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset + 2] = data & 0xFF;
}

void writeSector(int sector, int offset, uint8_t data) {
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = data;
}

void writeSectorInt(int sector, int offset, uint16_t data) {
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = (data >> 8) & 0xFF;
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 1] = data & 0xFF;
}

void writeSectorLong(int sector, int offset, uint32_t data) {
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = (data >> 24) & 0xFF;
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 1] = (data >> 16) & 0xFF;
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 2] = (data >> 8) & 0xFF;
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset + 3] = data & 0xFF;
}

void zeroSector(int sector) {
    // "tomorrow I want you to take that sector to Anchorhead and have its memory erased. It belongs to us now"
    for (int i = 0; i < SECTOR_SIZE; i++) {
        writeSector(sector, i, 0x00);
    }
}

bool isCatalogSector(int sec) {
    bytes tag = readTag(sec);
    return (tag[4] == 0x00 && tag[5] == 0x04);
}

void findMDDFSec() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        uint16_t type = readInt(tag, 4);
        if (type == 0x0001) {
            MDDFSec = i;
            return;
        }
    }
}

void findBitmapSec() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        uint16_t type = readInt(tag, 4);
        if (type == 0x0002) {
            bitmapSec = i;
            printf("Found Bitmap Sec = 0x%02X\n", bitmapSec);
            return;
        }
    }
}

void printSectorType(int sector) {
    bytes tag = readTag(sector);
    uint16_t type = readInt(tag, 4);
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

uint8_t bitmapByte(int sec) {
    int sectorToCorrect = (sec - MDDFSec);
    int freeBitmapSector = ((sectorToCorrect / 8) / SECTOR_SIZE) + bitmapSec; //8 sectors per byte
    //printf("BITMAP SEC = 0x%02X, sector to correct = 0x%02X\n", freeBitmapSector, sectorToCorrect);

    int byteIndex = (sectorToCorrect / 8) % SECTOR_SIZE;
    uint8_t previousByte = readSector(freeBitmapSector)[byteIndex];
    return previousByte;
}

bool isFreeSector(int sector) {
    //bytes tag = readTag(sector);
    return bitmapByte(sector) == 0x00;//TODO this is supremely cautious, for now. Fix this later
    /*(((tag[4] & 0xFF) == 0x00) && ((tag[5] & 0xFF) == 0x00)) || ((tag[4] & 0xFF) == 0x7F) && ((tag[5] & 0xFF) == 0xFF);*/
    //0x0000 is empty, 0x7FFF is deleted (?). Both seem to signify an empty sector
}

void decrementMDDFFreeCount() {
    bytes sec = readSector(MDDFSec);
    uint32_t freeCount = readLong(sec, 0xBA);
    freeCount--;
    writeSectorLong(MDDFSec, 0xBA, freeCount);
}

void fixFreeBitmap(int sec) {
    int sectorToCorrect = (sec - MDDFSec);
    int freeBitmapSector = ((sectorToCorrect / 8) / SECTOR_SIZE) + bitmapSec; //8 sectors per byte
    //printf("BITMAP SEC = 0x%02X, sector to correct = 0x%02X\n", freeBitmapSector, sectorToCorrect);

    int byteIndex = (sectorToCorrect / 8) % SECTOR_SIZE;

    //printf("sectorToCorrect div 8 = %d\n", sectorToCorrect / 8);
    //printf("sectorToCorrect mod 8 = %d\n", sectorToCorrect % 8);

    int baseSec = ((sectorToCorrect / 8) * 8) + MDDFSec;
    //printf("sec = 0x%02X, baseSec = 0x%02X\n", sectorToCorrect, baseSec);

    bool free7 = !isFreeSector(baseSec);
    bool free6 = !isFreeSector(baseSec+1); //todo might be wrong. Don't overdo it - hidden files?
    bool free5 = !isFreeSector(baseSec+2);
    bool free4 = !isFreeSector(baseSec+3);
    bool free3 = !isFreeSector(baseSec+4);
    bool free2 = !isFreeSector(baseSec+5);
    bool free1 = !isFreeSector(baseSec+6);
    bool free0 = !isFreeSector(baseSec+7);
    uint8_t byteToWrite = (free0 << 7) | (free1 << 6) | (free2 << 5) | (free3 << 4) | (free4 << 3) | (free5 << 2) | (free6 << 1) | (free7);
    uint8_t previousByte = readSector(freeBitmapSector)[byteIndex];
    if (previousByte != byteToWrite) {
        printf("Checking sector 0x%02X (= FB[0x%02X][0x%02X])\n", sec, freeBitmapSector, byteIndex);
        printf("Mismatching bitmap byte (was 0x%02X, now is 0x%02X).\n", previousByte, byteToWrite);
        printf("tags: \n");
        for (int j = 0; j < 8; j++) {
            bytes tag = readTag(baseSec + j);
            for (int j = 0; j < TAG_SIZE; j++) {
                printf("%02X ", tag[j] & 0xFF);
            }
            printf(" ");
            printSectorType(baseSec + j);
            printf("\n");
        }
    }
    //printf("sec=%d (0x%02X), bitvalue=0x%02X. Writing to sec=%d, %d\n", baseSec, baseSec, byteToWrite & 0xFF, bitmapSec + ((byteIndex / 8) / SECTOR_SIZE), (byteIndex / 8) % SECTOR_SIZE);
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

void writeHintEntry(int sector, int startSector, int sectorCount, int nameLength, char* name) {
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
    writeSectorLong(sector, 54, 0x9D27FACB); // modification date (should match file))

    writeSectorLong(sector, 100, 0x4E56FEFC); // standardized (?)
    writeSectorLong(sector, 104, 0x206E000C); // standardized (?)
    writeSectorLong(sector, 108, 0x00000001); // standardized (?)

    writeSectorInt(sector, 130, sectorCount & 0xFF); // number of sectors
    writeSectorLong(sector, 132, 0x00090001); // standardized (?)

    writeSectorInt(sector, 138, startSector - MDDFSec); // offset to first sector of data (-MDDFSec)
    writeSectorInt(sector, 140, sectorCount & 0xFF); // number of sectors, again (?)
}

void claimNextFreeHintSector(uint32_t sec, int startSector, int sectorCount, int nameLength, char* name) {
    printf("Claiming hint sector at sec=0x%08X\n", sec);
    // inscribe the ancient sigil 0x0001 (and other things) into the tags for this sector to claim it as a hint sector
    // TODO: sec 131 (0x10654) 0000 0100 FFC1 8000 00005D D3 0000 FFFFFF FFFFFF  type=FFC1: ????
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

    //TODO change file size of catalog file in S-records?
    decrementMDDFFreeCount();
}

int getSectorCount(int fileSize) {
    return (int) ceil((double) fileSize / SECTOR_SIZE);
}

//returns the index of the s-file (the file ID)
uint16_t claimNextFreeSFileIndex(int startSector, int fileSize, int sectorCount, int nameLength, char *name) {
    int responsibleSector = -1;
    int lastIdx = -1;
    int lastIdxWithinSector = -1;
    uint32_t lastHintAddr = -1;

    uint16_t idx = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        uint16_t fileId = readInt(tag, 4);
        if (fileId == 0x0003) {
            bytes data = readSector(i);
            for (int srec = 0; srec < SECTOR_SIZE; srec += 14) { //length of s-record
                uint32_t hintAddr = readLong(data, srec);
                //printf("IDX = 0x%02X: ", idx);
                //printf("hintAddr = 0x%08X, ", hintAddr);
                //printf("fileAddr = 0x%08X, ", readLong(data, srec + 4));
                //printf("fileSize = 0x%08X, ", readLong(data, srec + 8));
                //printf("version = 0x%04X\n", readInt(data, srec + 12));
                if (hintAddr != 0x00000000) { // claimed s-record
                    bytes hintTag = readTag(hintAddr + MDDFSec);
                    uint16_t index = readInt(hintTag, 4);
                    //printf("hint sector = 0x%02X, index=0x%04X\n", hintAddr + MDDFSec, index);
                    if (index < lastUsedHintIndex && index != 0x0000) { //index is 0x0000 for the 4 reserved S-file entries at the start of the listing
                        //printf("NEW LOWEST HINT INDEX: 0x%04X\n", index);
                        lastUsedHintIndex = index;
                    }

                    responsibleSector = i;
                    lastIdx = idx;
                    lastIdxWithinSector = srec;
                    lastHintAddr = hintAddr;
                }
                idx++;
            }
        }
    }

    if (lastHintAddr == -1) {
        return -1; //not found
    }

    //claim it and return it
    int sFileSectorToWrite = responsibleSector;
    int indexWithinSectorToWrite = lastIdxWithinSector + 14;
    for (int s = lastHintAddr + MDDFSec; s < SECTORS_IN_DISK; s++) {
        if (isFreeSector(s)) {
            printf("Claiming new s-file at index=0x%04X, hint sector=0x%08X, fileAddr=0x%08X, fileSize=0x%08X\n", lastIdx + 1, s, startSector, fileSize);
            //TODO responsibleSector could overflow to the next one if we're unlucky. For now, don't worry about it.
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite, s - MDDFSec); //location of our hint sector
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 4, startSector - MDDFSec); // fileAddr
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 8, sectorCount * SECTOR_SIZE); // fileSize TODO for now, use physical since it's likely safer
            writeSectorInt(sFileSectorToWrite, indexWithinSectorToWrite + 12, 0x0000); //version

            claimNextFreeHintSector(s, startSector, sectorCount, nameLength, name);
            return lastIdx; //TODO +1??? idk man, looks like an off-by-one to me but that's what the Lisa says so *shrug*
        }
    }
    return -1; // no space
}

void printSFile() {
    uint16_t idx = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        uint16_t fileId = readInt(tag, 4);
        if (fileId == 0x0003) {
            bytes data = readSector(i);
            for (int srec = 0; srec < SECTOR_SIZE; srec += 14) { //length of s-record
                uint32_t hintAddr = readLong(data, srec);
                if (hintAddr != 0x00000000) { // claimed s-record
                    //printf("hint sector = 0x%02X, index=0x%04X\n", hintAddr + MDDFSec, index);
                } else {
                    printf("FREE IDX = 0x%02X\n", idx);
                }
                idx++;
            }
        }
    }
}

uint8_t calculateChecksum(int sector) {
    uint8_t checksumByte = 0x00;
    bytes data = readSector(sector);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        checksumByte = checksumByte ^ (data[i] & 0xFF);
    }

    bytes tag = readTag(sector);
    for (int i = 0; i < TAG_SIZE; i++) {
        if (i != 11) { //the checksum byte isn't included
            checksumByte = checksumByte ^ (tag[i] & 0xFF);
        }
    }

    return checksumByte;
}

// returns the first sector of the 4
int claimNextFreeCatalogBlock() {
    for (int i = CATALOG_SEC_OFFSET; i < SECTORS_IN_DISK; i += 4) { //let's start looking after where the directories tend to begin
        if (isFreeSector(i) && isFreeSector(i + 1) && isFreeSector(i + 2) && isFreeSector(i + 3)) {
            for (int j = 0; j < 4; j++) {
                // inscribe the ancient sigil 0x0004 (and other common things) into the tags for these sectors to claim them as catalog sectors
                // TODO: sec 64  (0x8054) 0000 2500 0004 8200 00001A B1 0003 FFFFFF 000019  (catalog)
                //version
                writeTagInt(i+j, 0, 0x0000);

                //volid (TODO 0x2500 seems standard for this disk at least?)
                writeTagInt(i+j, 2, 0x2500);

                //fileid (always 0x0004 for directories)
                writeTagInt(i+j, 4, 0x0004);

                //dataused (0x8200 seems standard)
                writeTagInt(i+j, 6, 0x8200);

                //abspage
                int abspage = (i+j) - MDDFSec; //account for magic offset
                writeTag3Byte(i+j, 8, abspage);

                //index 11 is a checksum we'll in later

                //relpage
                writeTag(i+j, 12, 0x00);
                writeTag(i+j, 13, j);

                //fwdlink
                if (j == 3) {
                    writeTag3Byte(i+j, 14, 0xFFFFFF);
                } else {
                    int fwdlink = abspage + 1;
                    writeTag3Byte(i+j, 14, fwdlink);
                }

                //bkwdlink
                if (j == 0) {
                    writeTag3Byte(i+j, 17, 0xFFFFFF);
                } else {
                    int bkwdlink = abspage - 1;
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

            //TODO change file size of catalog file in S-records?
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
int ci_strncmp(char *a, int a_len, char *b, int b_len) {
    int i = 0;
    int min_len = (a_len < b_len) ? a_len : b_len;

    for (i = 0; i < min_len; i++) {
        int ac = tolower((unsigned char)a[i]);
        int bc = tolower((unsigned char)b[i]);
        if (ac != bc) {
            return ac - bc;
        }
    }

    // If equal so far, decide based on length
    return a_len - b_len;
}

void incrementMDDFFileCount() {
    bytes sec = readSector(MDDFSec);
    uint16_t fileCount = readInt(sec, 0xB0);
    fileCount++;
    writeSectorInt(MDDFSec, 0xB0, fileCount);

    // empty_files increments when you create an s-file. see new_sfile(). TODO seems wrong though.
    uint16_t empty_file = readInt(sec, 0x9E);
    empty_file++;
    writeSectorInt(MDDFSec, 0x9E, empty_file);
}

void writeCatalogEntry(int offset, int nextFreeSFileIndex, int fileSize, int sectorCount, int nameLength, char *name) {
    int physicalSize = sectorCount * SECTOR_SIZE;

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

    uint8_t restOfEntry[] = {
        0x03, 0x06, //we're a file (lisa says 0x0306)
        (nextFreeSFileIndex >> 8) & 0xFF, nextFreeSFileIndex & 0xFF, //sfile
        0x9D, 0x27, 0xFA, 0x88, //creation date (this one is random but I know it works)
        0x9D, 0x27, 0xFA, 0x8F, //modification date (this one is random but I know it works)
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

void claimNewCatalogEntry(uint16_t sfileid, int fileSize, int sectorCount, int nameLength, char *name) {
    bool first = true;
    for (int dirSec = 0; dirSec < SECTORS_IN_DISK; dirSec++) { //in all the possible catalogSectors. They come in 4s, always
        if (!isCatalogSector(dirSec)) {
            continue;
        }
        bytes sec = read4Sectors(dirSec);
        if (!(sec[0] == 0x24 && sec[1] == 0x00 && sec[2] == 0x00)) {
            continue; //invalid block
        }
        int offsetToFirstEntry = 0;
        if (first) {
            first = false;
            offsetToFirstEntry = 0x4E; //seems to be the case for the first catalog sector block only
        }

       uint8_t validEntryCount = sec[(SECTOR_SIZE * 4) - 11];
       if (validEntryCount < 0x1E) { //TODO dodgy. We have space
            for (int e = 0; e < ((SECTOR_SIZE * 4) / 0x40); e++) { //loop over all entries (0x40 long each, so up to 32 per catalog block)
                int entryOffset = offsetToFirstEntry + (e * 0x40); 
                if (entryOffset >= (SECTOR_SIZE * 4) - offsetToFirstEntry - 0x80) { //there's some standard padding (0x4A?) on the end of these blocks I'd like to leave in place
                    break; //we're past the end, so try again later
                }

                if (((e + 1) < validEntryCount) && sec[entryOffset + 0] == 0x24 && sec[entryOffset + 1] == 0x00 && sec[entryOffset + 2] == 0x00) { //the magic 0x240000 defines the start of a catalog entry
                    //printf("sec=%d,e=%d: used by file: ", dirSec, e);
                    for (int k = 0; k < 32; k++) { //number of bytes in a filename
                        printf("%c", sec[entryOffset + 3 + k]);
                    }
                    //printf(" - s-file = %02X%02X", sec[entryOffset+38] & 0xFF, sec[entryOffset+39] & 0xFF);
                    //printf(" - size = %02X%02X%02X%02X", sec[entryOffset+48] & 0xFF, sec[entryOffset+49] & 0xFF, sec[entryOffset+50] & 0xFF, sec[entryOffset+51] & 0xFF);
                    //printf(" - pSize = %02X%02X%02X%02X\n", sec[entryOffset+52] & 0xFF, sec[entryOffset+53] & 0xFF, sec[entryOffset+54] & 0xFF, sec[entryOffset+55] & 0xFF);


                    // compare my filename against the existing ones.
                    // write it when I can, then write the rest.
                    // if it doesn't match, continue onwards.
                    int cmp = ci_strncmp(name, nameLength, (char *) sec+entryOffset+3, 32);
                    if (cmp == 0) {
                        return; //illegal to have matching names
                    }
                    if (cmp < 0) {
                        writeSector(dirSec + 3, SECTOR_SIZE - 11, sec[(SECTOR_SIZE*4) - 11] + 1); //claim another valid entry in this sector
                        int catalogEntryOffset = DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset; //offset to the place to write in the file
                        //printf("Offset is 0x%02X\n", catalogEntryOffset);

                        //shift
                        for (int rest = e; rest < validEntryCount; rest++) {
                            int originalOffsetOfEntryWithinBlock = offsetToFirstEntry + (rest * 0x40);
                            int destinationOffsetOfEntryWithinBlock = originalOffsetOfEntryWithinBlock + 0x40;    
                            printf("Moving entry at offset 0x%02X to 0x%02X, beginning to write to sector %d: ", originalOffsetOfEntryWithinBlock, destinationOffsetOfEntryWithinBlock, (originalOffsetOfEntryWithinBlock / SECTOR_SIZE) + dirSec);
                            for (int k = 0; k < 32; k++) { //number of bytes in a filename
                                printf("%c", sec[originalOffsetOfEntryWithinBlock + 3 + k]);
                            }
                            printf("\n");
                            for (int eIdx = 0; eIdx < 64; eIdx++) { //length of catalog record
                                int off = originalOffsetOfEntryWithinBlock + eIdx;
                                //printf("SECTOR=%d, offset within sector=%d\n", (off / SECTOR_SIZE) + dirSec, (destinationOffsetOfEntryWithinBlock+eIdx) % SECTOR_SIZE);
                                image[DATA_OFFSET + (SECTOR_SIZE * dirSec) + destinationOffsetOfEntryWithinBlock + eIdx] = sec[off];
                            }
                        }

                        writeCatalogEntry(catalogEntryOffset, sfileid, fileSize, sectorCount, nameLength, name);
                        printf("Found space for a new catalog entry (shifting) at offset 0x%X\n", catalogEntryOffset);
                        return;
                    }

                    continue;
                } else {
                    bytes sec = readSector(dirSec + 3);
                    writeSector(dirSec + 3, SECTOR_SIZE - 11, sec[SECTOR_SIZE - 11] + 1); //claim another valid entry in this sector
                    writeCatalogEntry(DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset, sfileid, fileSize, sectorCount, nameLength, name);
                    printf("Found space for a new catalog entry (appending) at offset 0x%X\n", entryOffset);
                    return;
                }
            }
       }
        dirSec += 3; // skip past the rest in this block
    }

    //no space found, so let's make some
    printf("No space found for a new entry. Creating some...\n");
    int nextFreeBlock = claimNextFreeCatalogBlock();
    printf("Space to create new catalog block claimed at sector = %d\n", nextFreeBlock);
    bytes sec = readSector(nextFreeBlock + 3);
    writeSector(nextFreeBlock + 3, SECTOR_SIZE - 11, sec[SECTOR_SIZE - 11] + 1); //claim another valid entry in this sector
    writeCatalogEntry(DATA_OFFSET + (nextFreeBlock * SECTOR_SIZE), sfileid, fileSize, sectorCount, nameLength, name);
}

void fixAllTagChecksums() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        uint8_t checksum = calculateChecksum(i);
        if ((checksum & 0xFF) != (tag[11] & 0xFF)) {
            //printf("Checksum invalid for sector %d. Fixing...\n", i);
            writeTag(i, 11, (checksum & 0xFF));
        }
    }
}

void writeFileTagBytes(int startSector, int sectorCount, uint16_t sfileid) {
    for (int i = 0; i < sectorCount; i++) {
        int sectorToWrite = startSector + i;
        uint32_t abspage = sectorToWrite - MDDFSec;
        writeTagInt(sectorToWrite, 0, 0x0000); //version (2 bytes)
        writeTagInt(sectorToWrite, 2, 0x0000); //vol (2 bytes)
        writeTagInt(sectorToWrite, 4, sfileid); //file ID (2 bytes)
        writeTagInt(sectorToWrite, 6, 0x8200); //dataused (2 bytes. 0x8200, standard, it seems)
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

int findStartingSector(int contiguousSectors) {
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

int main (int argc, char *argv[]) {
    FILE *output = fopen("WS_new.dc42", "w");
    //initialize all global vars
    readFile();
    findMDDFSec();
    findBitmapSec();

    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        uint8_t calculatedChecksum = calculateChecksum(i);
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

    /*
    printSFile();
    for (int i = MDDFSec; i < SECTORS_IN_DISK; i++) {
        fixFreeBitmap(i);
    }
    fwrite(image, 1, FILE_LENGTH, output);
    fclose(output);


    return 0; // for now, test free bitmap
    */

    // get the file we want to write
    //TODO HARDCODED
    int nameLength = 13;
    char *name = "genedata.Text";

    FILE *fileptr;
    fileptr = fopen(name, "rb");     // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);
    int rawFileSize = ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);
    bytes filedata = (bytes) malloc(rawFileSize * sizeof(uint8_t)); // Enough memory for the file
    fread(filedata, rawFileSize, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file

    printf("Read data file: length = 0x%X\n", rawFileSize);

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
            b = 0x0D; //replace Mac style line breaks with Lisa style (TODO)
        }
        if (bytesWritten % BLOCK_SIZE == BLOCK_SIZE - 1) {
            printf("ERROR! There was no padding added here.\n");
        }
        if (bytesWritten % BLOCK_SIZE > (BLOCK_SIZE - 0x60) && justWroteNewline) {
            // TODO write some padding and account with the offsets
            int padding = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
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
    int remaining = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
    printf("    - (remaining = 0x%2X)\n", remaining);
    for (int i = 0; i < remaining; i++) {
        dataBuf[bytesWritten++] = 0x00;
    }
    printf("    - wrote end. bytesWritten=0x%2X\n", bytesWritten);

    printf("Final buffer length = 0x%2X\n", bytesWritten);

    // do the work
    int sectorCount = getSectorCount(bytesWritten);
    printf("sectorCount = 0x%2X\n", sectorCount);
    int startSector = findStartingSector(sectorCount); // allocate contiguously to be nice about it

    uint16_t sfileid = claimNextFreeSFileIndex(startSector, bytesWritten, sectorCount, nameLength, name);

    claimNewCatalogEntry(sfileid, bytesWritten, sectorCount, nameLength, name);

    // write the data from buffer
    for (int i = 0; i < sectorCount; i++) {
        for (int j = 0; j < SECTOR_SIZE; j++) {
            writeSector(startSector + i, j, dataBuf[(i * SECTOR_SIZE) + j]);
        }
    }

    writeFileTagBytes(startSector, sectorCount, sfileid);

    // cleanup and close

    fixAllTagChecksums();
    fwrite(image, 1, FILE_LENGTH, output);
    fclose(output);


    return 0;
}
