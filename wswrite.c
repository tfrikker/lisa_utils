#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define bytes uint8_t*

// ---------- Constants ----------
// bytes
const int FILE_LENGTH = 0x4EF854;    // length of a standard 5MB ProFile image
const int SECTOR_SIZE = 0x200;       // bytes per sector
const int DATA_OFFSET = 0x54;        // length of BLU file header
const int TAG_SIZE = 0x14;           // for ProFile disks
//sectors
const int CATALOG_SEC_OFFSET = 61; // Which sector the catalog listing starts on
const int SECTORS_IN_DISK = 0x2600;  // for 5MB ProFile

bytes image;
int MDDFSec;
int bitmapSec;
uint16_t lastUsedHintIndex = 0xFFFB; //seems to be where these start

uint16_t readInt(bytes data, int offset) {
    return ((data[offset] & 0xFF) << 8) | ((data[offset+1] & 0xFF));
}

uint32_t readLong(bytes data, int offset) {
    return ((data[offset] & 0xFF) << 24) | ((data[offset+1] & 0xFF) << 16) | ((data[offset+2] & 0xFF) << 8) | ((data[offset+3] & 0xFF));
}

void readFile() {
    FILE *fileptr;
    fileptr = fopen("WS_MASTER.dc42", "rb");     // Open the file in binary mode
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

void writeToImg(int sector, int offset, int len, bytes dataToWrite) {
    int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE) + offset;
    for (int i = 0; i < len; i++) {
        image[startIdx + i] = dataToWrite[i];
    }
}

int findLastCatalogBlock() {
    int lastCatalogBlock = -1;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        if (tag[2] == 0x25 && tag[5] == 0x04) { //0x25 seems to only be set for valid ones here
            lastCatalogBlock = i;
            i += 3; //zoom past the rest in the block
        }
    }

    return lastCatalogBlock;
}

int findCatalogSectors(int* dataToWrite) {
    int count = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        if (tag[5] == 0x04) {
            dataToWrite[count++] = i;
        }
    }

    return count;
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
        printf("<free>");
    } else if (type == 0x7FFF) {
        printf("<erased>");
    } else if (type == 0x0001) {
        printf("(MDDF)");
    } else if (type == 0x0002) {
        printf("(free bitmap)");
    } else if (type == 0x0003) {
        printf("(tag_s_records)");
    } else if (type == 0x0004) {
        printf("(catalog)");
    } else if (type == 0x7FFF) {
        printf("(maxtag)");
    } else {
        printf("type=%X: ????", type);
    }
}

bool isFreeSector(int sector) {
    //TODO could check if the sfileid is in the s-map, still
    bytes tag = readTag(sector);
    return ((tag[4] & 0xFF) == 0x00) && ((tag[5] & 0xFF) == 0x00);
}

void decrementMDDFFreeCount() {
    bytes sec = readSector(MDDFSec);
    uint32_t freeCount = readLong(sec, 0xBA);
    freeCount--;
    writeSectorLong(MDDFSec, 0xBA, freeCount);
}

//TODO there is a bug here. Fix it!
void fixFreeBitmap(int sec) {
    bool free7 = !isFreeSector(sec);
    bool free6 = !isFreeSector(sec+1); //todo might be wrong. Don't overdo it - hidden files?
    bool free5 = !isFreeSector(sec+2);
    bool free4 = !isFreeSector(sec+3);
    bool free3 = !isFreeSector(sec+4);
    bool free2 = !isFreeSector(sec+5);
    bool free1 = !isFreeSector(sec+6);
    bool free0 = !isFreeSector(sec+7);
    uint8_t byteToWrite = (free0 << 7) | (free1 << 6) | (free2 << 5) | (free3 << 4) | (free4 << 3) | (free5 << 2) | (free6 << 1) | (free7);
    printf("sec=%d, bitvalue=0x%02X. Writing to sec=%d, %d\n", sec, byteToWrite & 0xFF, bitmapSec + (((sec - 0x26) / 8) / SECTOR_SIZE), ((sec - 0x26) / 8) % SECTOR_SIZE);
    writeSector(bitmapSec + (((sec - 0x26) / 8) / SECTOR_SIZE), ((sec - 0x26) / 8) % SECTOR_SIZE, byteToWrite & 0xFF);
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
! = Sector offset to first sector of data (- 0x26)
*/

void writeHintEntry(int sector) {
    for (int i = 0; i < SECTOR_SIZE; i++) {
        writeSector(sector, i, 0x00); //zero out
    }

    writeSector(sector, 0, 0x0D); //name length

    uint8_t name[] = {
        'g', 'e', 'n', 'e', 'd', 'a', 't', 'a', '.', 'T', 'e', 'x', 't', //filename
        0x00, //padding
        'T', 'e', 'x', 't', //file type
    }; 
    for (int i = 0; i < 18; i++) { //bytes we have to write
        writeSector(sector, i + 1, name[i]);
    }

    writeSectorLong(sector, 34, 0xA24A228C); // date (?)
    writeSectorLong(sector, 38, 0x01000000); // standardized (?)
    writeSectorLong(sector, 42, 0x00150E00); // standardized (?)
    writeSectorLong(sector, 46, 0x9D27FAC7); // creation date (should match file)
    writeSectorLong(sector, 50, 0xA24A22A2); // date (?)
    writeSectorLong(sector, 54, 0x9D27FACB); // modification date (should match file))

    writeSectorLong(sector, 100, 0x4E56FEFC); // standardized (?)
    writeSectorLong(sector, 104, 0x206E000C); // standardized (?)
    writeSectorLong(sector, 108, 0x00000001); // standardized (?)

    writeSectorInt(sector, 130, 0x0001); // number of sectors
    writeSectorLong(sector, 132, 0x00090001); // standardized (?)

    writeSectorInt(sector, 138, 0x2200); // offset to first sector of data (-0x26)
    writeSectorInt(sector, 140, 0x0001); // number of sectors, again (?)

    printf("Wrote hint sector at sec=0x%08X\n", sector);
}

void claimNextFreeHintSector(uint32_t sec) {
    printf("Claiming hint sector at sec=0x%08X\n", sec);
    // inscribe the ancient sigil 0x0001 (and other things) into the tags for this sector to claim it as a hint sector
    // TODO: sec 131 (0x10654) 0000 0100 FFC1 8000 00005D D3 0000 FFFFFF FFFFFF  type=FFC1: ????
    //version
    writeTagInt(sec, 0, 0x0000);

    //volid (TODO 0x0100 seems standard for this type of record at least?)
    writeTagInt(sec, 2, 0x0100);

    //fileid (seems to decrement)
    writeTagInt(sec, 4, lastUsedHintIndex--);

    //dataused (0x8000 seems standard)
    writeTagInt(sec, 6, 0x8000);

    //abspage
    int abspage = sec - 0x26; //account for magic offset
    writeTag(sec, 8, (abspage >> 16) & 0xFF);
    writeTag(sec, 9, (abspage >> 8) & 0xFF);
    writeTag(sec, 10, abspage & 0xFF);

    //index 11 is a checksum we'll in later

    //relpage (0x0000 always)
    writeTagInt(sec, 12, 0x0000);

    //fwdlink (0xFFFFFF always)
    writeTag(sec, 14, 0xFF);
    writeTag(sec, 15, 0xFF);
    writeTag(sec, 16, 0xFF);

    //bkwdlink (0xFFFFFF always)
    writeTag(sec, 17, 0xFF);
    writeTag(sec, 18, 0xFF);
    writeTag(sec, 19, 0xFF);
    // "tomorrow I want you to take that sector to Anchorhead and have its memory erased. It belongs to us now"
    for (int j = 0; j < SECTOR_SIZE; j++) {
        writeSector(sec, j, 0x00); 
    }

    writeHintEntry(sec);

    fixFreeBitmap(sec);

    //TODO change file size of catalog file in S-records?
    decrementMDDFFreeCount();
}

//returns the index of the s-file (the file ID)
uint16_t claimNextFreeSFileIndex() {
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
                uint32_t fileAddr = readLong(data, srec + 4);
                uint32_t fileSize = readLong(data, srec + 8);
                uint16_t version = readInt(data, srec + 12);
                printf("IDX = 0x%02X: ", idx);
                printf("hintAddr = 0x%08X, ", hintAddr);
                printf("fileAddr = 0x%08X, ", fileAddr);
                printf("fileSize = 0x%08X, ", fileSize);
                printf("version = 0x%04X\n", version);
                if (hintAddr != 0x00000000) { // claimed s-record
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
    for (int s = lastHintAddr + 0x26; s < SECTORS_IN_DISK; s++) {
        if (isFreeSector(s)) {
            printf("Claiming new s-file at index=0x%04X, hint sector=0x%08X\n", lastIdx + 1, s);
            //TODO responsibleSector could overflow to the next one if we're unlucky. For now, don't worry about it.
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite, s - (0x26)); //location of our hint sector
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 4, 0x00002200); //TODO fileAddr - for now, hardcoded as sector 0x2200 (+0x26))
            writeSectorLong(sFileSectorToWrite, indexWithinSectorToWrite + 8, 0x00000200); //TODO fileSize (hard-coded as 1 sector for now)
            writeSectorInt(sFileSectorToWrite, indexWithinSectorToWrite + 12, 0x0000); //version

            claimNextFreeHintSector(s);
            return lastIdx; //TODO +1??? idk man, looks like an off-by-one to me but that's what the Lisa says so *shrug*
        }
    }
    return -1; // no space
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
    int lastUsedDirSec = findLastCatalogBlock();
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
                int abspage = (i+j) - 0x26; //account for magic offset
                writeTag(i+j, 8, (abspage >> 16) & 0xFF);
                writeTag(i+j, 9, (abspage >> 8) & 0xFF);
                writeTag(i+j, 10, abspage & 0xFF);

                //index 11 is a checksum we'll in later

                //relpage
                writeTag(i+j, 12, 0x00);
                writeTag(i+j, 13, j);

                //fwdlink
                if (j == 3) {
                    writeTag(i+j, 14, 0xFF);
                    writeTag(i+j, 15, 0xFF);
                    writeTag(i+j, 16, 0xFF);
                } else {
                    int fwdlink = abspage + 1;
                    writeTag(i+j, 14, (fwdlink >> 16) & 0xFF);
                    writeTag(i+j, 15, (fwdlink >> 8) & 0xFF);
                    writeTag(i+j, 16, fwdlink & 0xFF);
                }

                //bkwdlink
                if (j == 0) {
                    writeTag(i+j, 17, 0xFF);
                    writeTag(i+j, 18, 0xFF);
                    writeTag(i+j, 19, 0xFF);
                } else {
                    int bkwdlink = abspage - 1;
                    writeTag(i+j, 17, (bkwdlink >> 16) & 0xFF);
                    writeTag(i+j, 18, (bkwdlink >> 8) & 0xFF);
                    writeTag(i+j, 19, bkwdlink & 0xFF);
                }
            }
            // "tomorrow I want you to take those sectors to Anchorhead and have their memory erased. They belong to us now"
            for (int j = 0; j < SECTOR_SIZE; j++) {
                writeSector(i, j, 0xFF); 
                writeSector(i+1, j, 0xFF);
                writeSector(i+2, j, 0xFF);
                writeSector(i+3, j, 0xFF);
            }
            // inscribe the ancient sigil 0x240000 into the start of the first sector to label it as a catalog sector
            writeSector(i, 0, 0x24);
            writeSector(i, 1, 0x00);
            writeSector(i, 2, 0x00);

            fixFreeBitmap(i);
            fixFreeBitmap(i+1);
            fixFreeBitmap(i+2);
            fixFreeBitmap(i+3);

            /* TODO not sure if this is quite right.
            //TODO for testing - forward link from the last used catalog sector
            printf("WRITING FORWARD LINK from sector %d (+3) to %d\n", lastUsedDirSec, i);
            int secToWrite = i - 0x26; //magic number
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

void findLastUsedHintIndex() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        if (tag[2] == 0x01) { //seems to be the way to identify these blocks
            uint16_t index = readInt(tag, 4);
            if (index < lastUsedHintIndex) {
                lastUsedHintIndex = index;
                return;
            }
        }
    }
}

int findNewCatalogEntryOffset(int numDirSecs, int *catalogSectors) {
    for (int i = 0; i < numDirSecs; i += 4) { //in all the possible catalogSectors. They come in 4s, always
        int dirSec = catalogSectors[i];
        bytes sec = read4Sectors(dirSec);
        if (!(sec[0] == 0x24 && sec[1] == 0x00 && sec[2] == 0x00)) {
            continue; //invalid block
        }
        int offsetToFirstEntry = 0;
        if (i == 0) {
            offsetToFirstEntry = 0x4E; //seems to be the case for the first catalog sector block only
        }

        for (int e = 0; e < ((SECTOR_SIZE * 4) / 0x40); e++) { //loop over all entries (0x40 long each, so up to 32 per catalog block)
            int entryOffset = offsetToFirstEntry + (e * 0x40); 
            if (entryOffset >= (SECTOR_SIZE * 4) - offsetToFirstEntry - 0x80) { //there's some standard padding (0x4A?) on the end of these blocks I'd like to leave in place
                break; //we're past the end, so try again later
            }
            if (sec[entryOffset + 0] == 0x24 && sec[entryOffset + 1] == 0x00 && sec[entryOffset + 2] == 0x00) { //the magic 0x240000 defines the start of a catalog entry
                printf("sec=%d,e=%d: used by file: ", dirSec, e);
                for (int k = 0; k < 32; k++) { //number of bytes in a filename
                    printf("%c", sec[entryOffset + 3 + k]);
                }
                printf(" - s-file = %02X%02X", sec[entryOffset+38] & 0xFF, sec[entryOffset+39] & 0xFF);
                printf(" - size = %02X%02X%02X%02X", sec[entryOffset+48] & 0xFF, sec[entryOffset+49] & 0xFF, sec[entryOffset+50] & 0xFF, sec[entryOffset+51] & 0xFF);
                printf(" - pSize = %02X%02X%02X%02X\n", sec[entryOffset+52] & 0xFF, sec[entryOffset+53] & 0xFF, sec[entryOffset+54] & 0xFF, sec[entryOffset+55] & 0xFF);
                continue;
            } else {
                return DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset; //offset to the place to write in the file
            }
        }
    }

    return -1; //no space found
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

/*
24000053 58726566 2E4F626A 00000000 00000000 00000000 00000000 00000000 00000000 03000060 9D27FA88 9D27FA8F 00003C00 00003C00 00010000 00000000

key - 36 bytes
24000053 58726566 2E4F626A 00000000 00000000 00000000 00000000 00000000 00000000

eType = 2 bytes
0300

sfile = 2 bytes
0060

fileDTC = 4 bytes
9D27FA88

fileDTM = 4 bytes
9D27FA8F

size = 4 bytes
00003C00

physSize = 4 bytes
00003C00

fsOvrhd = 2 bytes
0001

flags = 2 bytes
0000

fileUnused = 4 bytes
00000000
*/

void writeCatalogEntry(int offset, int nextFreeSFileIndex) {
    int len = 64; //length of a catalog entry
    uint8_t entry[] = {
        0x24, //length of name with padding
        0x00, 0x00, 'g', 'e', 'n', 'e', 'd', 'a', 't', 'a', '.', 'T', 'e', 'x', 't', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //filename
        0x03, 0x00, //we're a file
        (nextFreeSFileIndex >> 8) & 0xFF, nextFreeSFileIndex & 0xFF, //sfile
        0x9D, 0x27, 0xFA, 0x88, //creation date (this one is random but I know it works)
        0x9D, 0x27, 0xFA, 0x8F, //modification date (this one is random but I know it works)
        0x00, 0x00, 0x02, 0x00, //file size (TODO: I actually need to fix this one. For now: 1 sector, exactly)
        0x00, 0x00, 0x02, 0x00, // physical file size (?) (make it the same as the previous. For now: 1 sector, exactly)
        0x00, 0x01, //fsOvrhd (? - I know this one works so let's just use it for now),
        0x00, 0x00, //flags
        0x00, 0x00, 0x00, 0x00 //fileUnused
    }; 
    for (int i = 0; i < len; i++) {
        image[offset + i] = entry[i];
    }
    incrementMDDFFileCount();
}

void fixAllTagChecksums() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        bytes tag = readTag(i);
        uint8_t checksum = calculateChecksum(i);
        if ((checksum & 0xFF) != (tag[11] & 0xFF)) {
            printf("Checksum invalid for sector %d. Fixing...\n", i);
            writeTag(i, 11, (checksum & 0xFF));
        }
    }
}

int main (int argc, char *argv[]) {
    FILE *output = fopen("WS_new.dc42", "w");
    //initialize all global vars
    readFile();
    findMDDFSec();
    findBitmapSec();
    findLastUsedHintIndex();

    int *catalogSectors = (int *) malloc(24 * sizeof(int)); // for now, support up to 24 catalog sectors

    int numDirSecs = findCatalogSectors(catalogSectors);

    for (int i = 0; i < 0; i++) {
        uint8_t calculatedChecksum = calculateChecksum(i);
        printf("sec %d (0x%02X) with chksum 0x%02X:", i, DATA_OFFSET + (i * SECTOR_SIZE), (calculatedChecksum & 0xFF));
        bytes tag = readTag(i);
        int hintSector = 0;
        int catalogSector = 0;
        for (int j = 0; j < TAG_SIZE; j++) {
            printf("%02X ", tag[j] & 0xFF);
            if (j == 2 && tag[j] == 0x01) {
                hintSector = 1;
            }
            if (j == 5 && tag[j] == 0x04) {
                catalogSector = 1;
            }
        }
        printf(" ");
        printSectorType(i);
        printf("\n");
    }

    uint16_t sfileid = claimNextFreeSFileIndex();

    int offset = findNewCatalogEntryOffset(numDirSecs, catalogSectors);
    while (offset == -1) {
        printf("No space found for a new entry. Creating some...\n");
        int nextFreeBlock = claimNextFreeCatalogBlock();
        printf("Space to create new catalog block claimed at sector = %d\n", nextFreeBlock);
        int numDirSecs = findCatalogSectors(catalogSectors); //recalc this as we've just obtained some new ones
        offset = findNewCatalogEntryOffset(numDirSecs, catalogSectors);
    }
    printf("Found space for a new catalog entry at offset 0x%X\n", offset);

    writeCatalogEntry(offset, sfileid);

    //TODO write file data
    //TODO test writing tag entry for data block. If this works, write a function for it.
    writeTagInt(0x2226, 0, 0x0000); //version (2 bytes)
    writeTagInt(0x2226, 2, 0x0000); //vol (2 bytes)
    writeTagInt(0x2226, 4, sfileid); //file ID (2 bytes)
    writeTagInt(0x2226, 6, 0x8200); //dataused (2 bytes. 0x8200, standard, it seems)
    writeTag(0x2226, 8, 0x00); //abspage (3 bytes. The sector, 0x26? check this)
    writeTag(0x2226, 9, 0x22); 
    writeTag(0x2226, 10, 0x00); 
    writeTag(0x2226, 11, 0x00); // checksum (1 byte - will be fixed later)
    writeTagInt(0x2226, 12, 0x0000); // relpage (2 bytes. 0x00 in this case, since we're a 1 sector piece of data)
    writeTag(0x2226, 14, 0xFF); // fwdlink (3 bytes. 0xFFFFFF says none)
    writeTag(0x2226, 15, 0xFF);
    writeTag(0x2226, 16, 0xFF);
    writeTag(0x2226, 17, 0xFF); // bkwdlink (3 bytes. 0xFFFFFF says none)
    writeTag(0x2226, 18, 0xFF);
    writeTag(0x2226, 19, 0xFF);
    fixFreeBitmap(0x2226);

    //writeSector(64, SECTOR_SIZE - 11, 0x18); //for testing
    //writeSector(136, SECTOR_SIZE - 11, 0x18); //for testing

    fixAllTagChecksums();

    fwrite(image, 1, FILE_LENGTH, output);

    fclose(output);

    for (int i = 0; i < 200; i++) {
        uint8_t calculatedChecksum = calculateChecksum(i);
        printf("sec %d (0x%02X) with chksum 0x%02X:", i, DATA_OFFSET + (i * SECTOR_SIZE), (calculatedChecksum & 0xFF));
        bytes tag = readTag(i);
        int hintSector = 0;
        int catalogSector = 0;
        for (int j = 0; j < TAG_SIZE; j++) {
            printf("%02X ", tag[j] & 0xFF);
            if (j == 2 && tag[j] == 0x01) {
                hintSector = 1;
            }
            if (j == 5 && tag[j] == 0x04) {
                catalogSector = 1;
            }
        }
        printf(" ");
        printSectorType(i);
        printf("\n");
    }

    return 0;
}
