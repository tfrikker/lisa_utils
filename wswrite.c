#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// ---------- Constants ----------
// bytes
const int FILE_LENGTH = 0x4EF854;    // length of a standard 5MB ProFile image
const int SECTOR_SIZE = 0x200;       // bytes per sector
const int DATA_OFFSET = 0x54;        // length of BLU file header
const int TAG_SIZE = 0x14;           // for ProFile disks
//sectors
const int DIRECTORY_SEC_OFFSET = 61; // Which sector the directory listing starts on
const int SECTORS_IN_DISK = 0x2600;  // for 5MB ProFile

char *image;

void readFile() {
    FILE *fileptr;
    fileptr = fopen("WS_MASTER.dc42", "rb");     // Open the file in binary mode
    image = (char *) malloc(FILE_LENGTH * sizeof(char)); // Enough memory for the file
    fread(image, FILE_LENGTH, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file

    printf("Read file: length = 0x%X\n", FILE_LENGTH);
}

char* readSector(int sector) {
    char *sec = (char *) malloc(SECTOR_SIZE * sizeof(char));
    int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        sec[i] = image[startIdx + i];
    }

    return sec;
}

char* read4Sectors(int sector) {
    char *sec = (char *) malloc(SECTOR_SIZE * sizeof(char) * 4);
    int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE * 4; i++) {
        sec[i] = image[startIdx + i];
    }

    return sec;
}

char* readTag(int sector) {
    char *tag = (char *) malloc(TAG_SIZE * sizeof(char));
    int startIdx = DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE);
    for (int i = 0; i < TAG_SIZE; i++) {
        tag[i] = image[startIdx + i];
    }

    return tag;
}

void writeTag(int sector, int offset, char data) {
    image[DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE) + offset] = data;
}

void writeSector(int sector, int offset, char data) {
    image[DATA_OFFSET + (sector * SECTOR_SIZE) + offset] = data;
}

void writeToImg(int sector, int offset, int len, char* dataToWrite) {
    int startIdx = DATA_OFFSET + (sector * SECTOR_SIZE) + offset;
    for (int i = 0; i < len; i++) {
        image[startIdx + i] = dataToWrite[i];
    }
}

int findLastDirectoryBlock() {
    int lastDirectoryBlock = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        if (tag[2] == 0x25 && tag[5] == 0x04) { //0x25 seems to only be set for valid ones here
            printf("FOUND A DIR START AT %d. Skipping forward 3...\n", i);
            lastDirectoryBlock = i;
            i += 3; //zoom past the rest in the block
        }
    }

    return lastDirectoryBlock;
}

int findDirectorySectors(int* dataToWrite) {
    int count = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        if (tag[2] == 0x25 && tag[5] == 0x04) { //0x25 seems to only be set for valid ones here
            dataToWrite[count++] = i;
        }
    }

    return count;
}

int findMDDFSec() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        uint16_t type = ((tag[4] & 0xFF) << 8) | (tag[5] & 0xFF);
        if (type == 0x0001) {
            return i;
        }
    }

    return -1;
}

int findBitmapSec() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        uint16_t type = ((tag[4] & 0xFF) << 8) | (tag[5] & 0xFF);
        if (type == 0x0002) {
            return i;
        }
    }

    return -1;
}

int findFsSectors(int* dataToWrite) {
    int count = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        if (tag[2] == 0x01) {
            dataToWrite[count++] = i;
        }
    }

    return count;
}

void printSectorType(int sector) {
    char *tag = readTag(sector);
    uint16_t type = ((tag[4] & 0xFF) << 8) | (tag[5] & 0xFF);
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
        printf("(directory)");
    } else if (type == 0x7FFF) {
        printf("(maxtag)");
    } else {
        printf("type=%X: ????", type);
    }
}

bool isFreeSector(int sector) {
    char *tag = readTag(sector);
    return ((tag[4] & 0xFF) == 0x00) && ((tag[5] & 0xFF) == 0x00);
}

int countFreeSectors() {
    int freeCount = 0;
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        if (isFreeSector(i)) {
            freeCount++;
        }
    }
    return freeCount;
}

void listSFileEntries() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        if(((tag[4] & 0xFF) == 0x00) && ((tag[5] & 0xFF) == 0x03)) {
            printf("SECTOR = %d\n", i);
            char *data = readSector(i);
            for (int j = 0; j < (SECTOR_SIZE / 14); j++) { //length of s-record
                printf("hintAddr = 0x%02X%02X%02X%02X, ", data[(j*14)] & 0xFF, data[(j*14) + 1] & 0xFF, data[(j*14) + 2] & 0xFF, data[(j*14) + 3] & 0xFF);
                printf("fileAddr = 0x%02X%02X%02X%02X, ", data[(j*14) + 4] & 0xFF, data[(j*14) + 5] & 0xFF, data[(j*14) + 6] & 0xFF, data[(j*14) + 7] & 0xFF);
                printf("fileSize = 0x%02X%02X%02X%02X, ", data[(j*14) + 8] & 0xFF, data[(j*14) + 9] & 0xFF, data[(j*14) + 10] & 0xFF, data[(j*14) + 11] & 0xFF);
                printf("version = 0x%02X%02X\n", data[(j*14) + 12] & 0xFF, data[(j*14) + 13] & 0xFF);
            }
        }
    }
}

void decrementMDDFFreeCount(int MDDFSec) {
    char *sec = readSector(MDDFSec);
    uint32_t freeCount = ((sec[0xBA] & 0xFF) << 24) | ((sec[0xBB] & 0xFF) << 16) | ((sec[0xBC] & 0xFF) << 8) | ((sec[0xBD] & 0xFF));
    uint32_t fcm = freeCount - 1;
    printf("Decrementing free count. Was %u (0x%02X), now %u (0x%02X)\n", freeCount, freeCount, fcm, fcm);
    freeCount--;
    writeSector(MDDFSec, 0xBA, (freeCount >> 24) & 0xFF);
    writeSector(MDDFSec, 0xBB, (freeCount >> 16) & 0xFF);
    writeSector(MDDFSec, 0xBC, (freeCount >> 8) & 0xFF);
    writeSector(MDDFSec, 0xBD, freeCount & 0xFF);
}

char calculateChecksum(int sector) {
    char checksumByte = 0x00;
    char* data = readSector(sector);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        checksumByte = checksumByte ^ (data[i] & 0xFF);
    }

    char* tag = readTag(sector);
    for (int i = 0; i < TAG_SIZE; i++) {
        if (i != 11) { //the checksum byte isn't included
            checksumByte = checksumByte ^ (tag[i] & 0xFF);
        }
    }

    return checksumByte;
}

//TODO there is a bug here. Fix it!
void fixFreeBitmap(int bitmapSec, int sec) {
    bool free7 = !isFreeSector(sec);
    bool free6 = !isFreeSector(sec+1); //todo might be wrong. Don't overdo it - hidden files?
    bool free5 = !isFreeSector(sec+2);
    bool free4 = !isFreeSector(sec+3);
    bool free3 = !isFreeSector(sec+4);
    bool free2 = !isFreeSector(sec+5);
    bool free1 = !isFreeSector(sec+6);
    bool free0 = !isFreeSector(sec+7);
    char byteToWrite = (free0 << 7) | (free1 << 6) | (free2 << 5) | (free3 << 4) | (free4 << 3) | (free5 << 2) | (free6 << 1) | (free7);
    printf("sec=%d, bitvalue=0x%02X. Writing to sec=%d, %d\n", sec, byteToWrite & 0xFF, bitmapSec + (((sec - 0x26) / 8) / SECTOR_SIZE), ((sec - 0x26) / 8) % SECTOR_SIZE);
    writeSector(bitmapSec + (((sec - 0x26) / 8) / SECTOR_SIZE), ((sec - 0x26) / 8) % SECTOR_SIZE, byteToWrite & 0xFF);
}

// returns the first sector of the 4
int claimNextFreeDirectoryBlock(int bitmapSec, int MDDFSec) {
    int lastUsedDirSec = findLastDirectoryBlock();
    for (int i = DIRECTORY_SEC_OFFSET; i < SECTORS_IN_DISK; i += 4) { //let's start looking after where the directories tend to begin
        if (isFreeSector(i) && isFreeSector(i + 1) && isFreeSector(i + 2) && isFreeSector(i + 3)) {
            for (int j = 0; j < 4; j++) {
                // inscribe the ancient sigil 0x0004 (and other common things) into the tags for these sectors to claim them as directory sectors
                // TODO: sec 64  (0x8054) 0000 2500 0004 8200 00001A B1 0003 FFFFFF 000019  (directory)
                //version
                writeTag(i+j, 0, 0x00);
                writeTag(i+j, 1, 0x00);

                //volid (TODO 0x2500 seems standard for this disk at least?)
                writeTag(i+j, 2, 0x25);
                writeTag(i+j, 3, 0x00);

                //fileid (always 0x0004 for directories)
                writeTag(i+j, 4, 0x00);
                writeTag(i+j, 5, 0x04);

                //dataused (0x8200 seems standard)
                writeTag(i+j, 6, 0x82);
                writeTag(i+j, 7, 0x00);

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
            // inscribe the ancient sigil 0x240000 into the start of the first sector to label it as a directory sector
            writeSector(i, 0, 0x24);
            writeSector(i, 1, 0x00);
            writeSector(i, 2, 0x00);

            fixFreeBitmap(bitmapSec, i);
            fixFreeBitmap(bitmapSec, i+1);
            fixFreeBitmap(bitmapSec, i+2);
            fixFreeBitmap(bitmapSec, i+3);

            /* TODO not sure if this is quite right.
            //TODO for testing - forward link from the last used directory sector
            printf("WRITING FORWARD LINK from sector %d (+3) to %d\n", lastUsedDirSec, i);
            int secToWrite = i - 0x26; //magic number
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 6, (secToWrite << 24) & 0xFF);
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 5, (secToWrite << 16) & 0xFF);
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 4, (secToWrite << 8) & 0xFF);
            writeSector(lastUsedDirSec+3, SECTOR_SIZE - 3, secToWrite & 0xFF);

            //TODO for testing - backward link to the last directory sector start
            writeSector(i+3, SECTOR_SIZE - 10, 0x00);
            writeSector(i+3, SECTOR_SIZE - 9, 0x00);
            writeSector(i+3, SECTOR_SIZE - 8, 0x00);
            writeSector(i+3, SECTOR_SIZE - 7, 0x39);

            //0x00FF always ends it
            writeSector(i+3, SECTOR_SIZE - 2, 0x00);
            writeSector(i+3, SECTOR_SIZE - 1, 0x00);
            */

            //TODO change file size of FS file in S-records?
            decrementMDDFFreeCount(MDDFSec);
            decrementMDDFFreeCount(MDDFSec);
            decrementMDDFFreeCount(MDDFSec);
            decrementMDDFFreeCount(MDDFSec);

            return i;
        }
    }
    return -1;
}

//returns the sector index
int claimNextFreeFSSector(int bitmapSec, int MDDFSec, int lastUsedFSIndex) {
    for (int i = DIRECTORY_SEC_OFFSET; i < SECTORS_IN_DISK; i += 1) { //let's start looking after where the directories tend to begin
        if (isFreeSector(i)) {
            int index = lastUsedFSIndex - 1;
            // inscribe the ancient sigil 0x0001 (and other things) into the tags for this sector to claim it as an FS sector
            // TODO: sec 131 (0x10654) 0000 0100 FFC1 8000 00005D D3 0000 FFFFFF FFFFFF  type=FFC1: ????
            //version
            writeTag(i, 0, 0x00);
            writeTag(i, 1, 0x00);

            //volid (TODO 0x0100 seems standard for this type of record at least?)
            writeTag(i, 2, 0x01);
            writeTag(i, 3, 0x00);

            //fileid (seems to decrement)
            writeTag(i, 4, (index >> 8) & 0xFF);
            writeTag(i, 5, index & 0xFF);

            //dataused (0x8000 seems standard)
            writeTag(i, 6, 0x80);
            writeTag(i, 7, 0x00);

            //abspage
            int abspage = i - 0x26; //account for magic offset
            writeTag(i, 8, (abspage >> 16) & 0xFF);
            writeTag(i, 9, (abspage >> 8) & 0xFF);
            writeTag(i, 10, abspage & 0xFF);

            //index 11 is a checksum we'll in later

            //relpage (0x0000 always)
            writeTag(i, 12, 0x00);
            writeTag(i, 13, 0x00);

            //fwdlink (0xFFFFFF always)
            writeTag(i, 14, 0xFF);
            writeTag(i, 15, 0xFF);
            writeTag(i, 16, 0xFF);

            //bkwdlink (0xFFFFFF always)
            writeTag(i, 17, 0xFF);
            writeTag(i, 18, 0xFF);
            writeTag(i, 19, 0xFF);
            // "tomorrow I want you to take that sector to Anchorhead and have its memory erased. It belongs to us now"
            for (int j = 0; j < SECTOR_SIZE; j++) {
                writeSector(i, j, 0x00); 
            }

            fixFreeBitmap(bitmapSec, i);

            //TODO change file size of directory file in S-records?
            decrementMDDFFreeCount(MDDFSec);
            return i;
        }
    }
    return -1;
}

int findLastUsedFSIndex() {
    int lastUsedIndex = 0xFFFB; //seems to be where these start
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        if (tag[2] == 0x01) { //seems to be the way to identify these blocks
            int index = ((tag[4] & 0xFF) << 8) | (tag[5] & 0xFF);
            if (index < lastUsedIndex) {
                lastUsedIndex = index;
            }
        }
    }
    return lastUsedIndex;
}

int findNewDirectoryEntryOffset(int numDirSecs, int *directorySectors) {
    for (int i = 0; i < numDirSecs; i += 4) { //in all the possible directorySectors. They come in 4s, always
        int dirSec = directorySectors[i];
        char *sec = read4Sectors(dirSec);
        if (!(sec[0] == 0x24 && sec[1] == 0x00 && sec[2] == 0x00)) {
            continue; //invalid block
        }
        int offsetToFirstEntry = 0;
        if (i == 0) {
            offsetToFirstEntry = 0x4E; //seems to be the case for the first directory sector block only
        }

        int e = 0;
        for (; e < ((SECTOR_SIZE * 4) / 0x40); e++) { //loop over all entries (0x40 long each, so up to 32 per directory block)
            int entryOffset = offsetToFirstEntry + e * 0x40; 
            if (entryOffset >= (SECTOR_SIZE * 4) - offsetToFirstEntry - 0x80) { //there's some standard padding on the end of these blocks I'd like to leave in place
                break; //we're past the end, so try again later
            }
            if (sec[entryOffset + 0] == 0x24 && sec[entryOffset + 1] == 0x00 && sec[entryOffset + 2] == 0x00) { //the magic 0x240000 defines the start of a directory entry
                bool hasName = false;
                for (int n = 0; n < 32; n++) {
                    if ((sec[entryOffset + 3 + n] & 0xFF) != 0xFF) {
                        hasName = true;
                        break;
                    }
                }
                if (hasName) {
                    //printf("sec=%d,e=%d: used by file: ", dirSec, e);
                    for (int k = 0; k < 32; k++) { //number of bytes in a filename
                        //printf("%c", sec[entryOffset + 3 + k]);
                    }
                    //printf(" - s-file = %02X%02X", sec[entryOffset+38] & 0xFF, sec[entryOffset+39] & 0xFF);
                    //printf(" - size = %02X%02X%02X%02X", sec[entryOffset+48] & 0xFF, sec[entryOffset+49] & 0xFF, sec[entryOffset+50] & 0xFF, sec[entryOffset+51] & 0xFF);
                    //printf(" - pSize = %02X%02X%02X%02X\n", sec[entryOffset+52] & 0xFF, sec[entryOffset+53] & 0xFF, sec[entryOffset+54] & 0xFF, sec[entryOffset+55] & 0xFF);
                    continue;
                } else {
                    return DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset; //offset to the place to write in the file
                }
            } else {
                return DATA_OFFSET + (dirSec * SECTOR_SIZE) + entryOffset; //offset to the place to write in the file
            }
        }
    }

    return -1; //no space found
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

void writeDirectoryEntry(int offset) {
    int len = 64; //length of a directory entry
    char entry[] = {
        0x24, //length of name with padding
        0x00, 0x00, 'g', 'e', 'n', 'e', 'd', 'a', 't', 'a', '.', 'T', 'e', 'x', 't', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //filename
        0x03, 0x00, //we're a file
        0x00, 0x5E, //sfile (TODO: verify I have a unique one here), and check the S-file map (?)
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
}

/*
AABBBBBB BBBBBBBB BBBBBB00 CCCCCCCC ???????? / ________ ________ ________ ____@@@@ @@@@____ ________ ____DDDD DDDD@@@@ @@@@EEEE EEEE____ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ________ ______## ________ ____!!!! __##____ / (0 until end of 0x200)
0A7B7B7B 546F6D2E 4F626A00 2E4F626A 00180000 / 002E0BF8 002E0C00 000000CC 5ED4A24A 228C0100 00000015 0E009D27 FAC7A24A 22A29D27 FACB0000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0C005407 54000C00 00000000 4E56FEFC 206E000C 00000001 00000000 00000000 00000000 00000000 0000000A 00090001 00001BF4 000A0000  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000

A = name length (bytes)
B = name 
C = type (".Obj")
D = creation date (aligns with directory listing)
E = modification date (aligns with directory listing)
_ = (standardized? Check more examples?)
@ = ascending? Looks like a date, maybe?
# = # of sectors (?)
! = Sector offset to first sector of data (- 0x26)
*/

void writeFSEntry(int sector) {
    char entry[] = {
        0x0D, //name length
        'g', 'e', 'n', 'e', 'd', 'a', 't', 'a', '.', 'T', 'e', 'x', 't', //filename
        0x00, //padding
        'T', 'e', 'x', 't', //file type
        0x00, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//padding to get to 34 bytes
        0xA2, 0x4A, 0x22, 0x8C, //date (?)
        0x01, 0x00, 0x00, 0x00, 0x00, 0x15, 0x0E, 0x00, //standardized (?)
        0x9D, 0x27, 0xFA, 0xC7, //creation date (should match file)
        0xA2, 0x4A, 0x22, 0xA2, //date (?),
        0x9D, 0x27, 0xFA, 0xCB, //modification date (should match file)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//standardized 0x00 times 44 (?)
        0x4E, 0x56, 0xFE, 0xFC, 0x20, 0x6E, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x01, //standardized (?)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //standardized 0x00 (?) times 18
        0x00, 0x01, //number of sectors
        0x00, 0x09, 0x00, 0x01, 0x00, 0x00, //standardized (?)
        0x22, 0x00, //offset to first sector of data (-0x26)
        0x00, 0x01 //number of sectors (again?)
    }; 
    for (int i = 0; i < 142; i++) { //bytes we have to write
        writeSector(sector, i, entry[i]);
    }
}

void incrementMDDFFileCount(int MDDFSec) {
    char *sec = readSector(MDDFSec);
    int fileCount = ((sec[0xB0] & 0xFF) << 8) | (sec[0xB1] & 0xFF);
    fileCount++;
    writeSector(MDDFSec, 0xB0, (fileCount >> 8) & 0xFF);
    writeSector(MDDFSec, 0xB1, fileCount & 0xFF);

    // this is a mystery byte - but it looks like it increments, so we'll do that too.
    char c = sec[0x9F] & 0xFF;
    writeSector(MDDFSec, 0x9F, (++c & 0xFF));
}

void fixAllTagChecksums() {
    for (int i = 0; i < SECTORS_IN_DISK; i++) {
        char *tag = readTag(i);
        char checksum = calculateChecksum(i);
        if ((checksum & 0xFF) != (tag[11] & 0xFF)) {
            printf("Checksum invalid for sector %d. Fixing...\n", i);
            writeTag(i, 11, (checksum & 0xFF));
        }
    }
}

int main (int argc, char *argv[]) {
    FILE *output = fopen("WS_new.dc42", "w");

    readFile();

    int *directorySectors = (int *) malloc(24 * sizeof(int)); // for now, support up to 24 directory sectors
    int *fsSectors = (int *) malloc(200 * sizeof(int)); // for now, support up to 200 fs sectors

    int numDirSecs = findDirectorySectors(directorySectors);
    int numFsSecs = findFsSectors(fsSectors);

    int freeSectors = countFreeSectors();
    printf("Free sectors in disk: %d\n", freeSectors);

    for (int i = 0; i < 0; i++) {
        char calculatedChecksum = calculateChecksum(i);
        printf("sec %d (0x%02X) with chksum 0x%02X:", i, DATA_OFFSET + (i * SECTOR_SIZE), (calculatedChecksum & 0xFF));
        char *tag = readTag(i);
        int fsSector = 0;
        int directorySector = 0;
        for (int j = 0; j < TAG_SIZE; j++) {
            printf("%02X ", tag[j] & 0xFF);
            if (j == 2 && tag[j] == 0x01) {
                fsSector = 1;
            }
            if (j == 5 && tag[j] == 0x04) {
                directorySector = 1;
            }
        }
        printf(" ");
        printSectorType(i);
        printf("\n");
    }

    //listSFileEntries();

    printf("Found the following directory sectors:\n");
    for (int i = 0; i < numDirSecs; i++) {
        printf("%d, ", directorySectors[i]);
    }
    printf("\n");

    printf("Found the following FS sectors:\n");
    for (int i = 0; i < numFsSecs; i++) {
        printf("%d, ", fsSectors[i]);
    }
    printf("\n");

    int MDDFSec = findMDDFSec();
    int bitmapSec = findBitmapSec();

    printf("Found the MDDF at sector = %d\n", MDDFSec);

    int offset = findNewDirectoryEntryOffset(numDirSecs, directorySectors);
    while (offset == -1) {
        printf("No space found for a new entry. Creating some...\n");
        int nextFreeBlock = claimNextFreeDirectoryBlock(bitmapSec, MDDFSec);
        printf("Space to create new directory block claimed at sector = %d\n", nextFreeBlock);
        int numDirSecs = findDirectorySectors(directorySectors); //recalc this as we've just obtained some new ones
        offset = findNewDirectoryEntryOffset(numDirSecs, directorySectors);
    }
    printf("Found space for a new directory entry at offset 0x%X\n", offset);

    int lastUsedFSIndex = findLastUsedFSIndex();

    writeDirectoryEntry(offset);

    int sector = claimNextFreeFSSector(bitmapSec, MDDFSec, lastUsedFSIndex);
    lastUsedFSIndex = findLastUsedFSIndex();
    //printf("Space to create new FS sector claimed at = %d\n", sector);
    writeFSEntry(sector);
    //TODO fix tags for file data
    //TODO write file data

    incrementMDDFFileCount(MDDFSec);

    //TODO test writing s-file entries. If this works, write a function for it
    char toWriteTest[] = {
        0x00, 0x00, 0x00, 0x5E, 
        0x00, 0x00, 0x22, 0x00, //sector 0x2200 (+0x26)
        0x00, 0x00, 0x02, 0x00,
        0x00, 0x00,
    }; 
    writeToImg(43, 0x188, 14, toWriteTest);

    //TODO test writing tag entry for data block. If this works, write a function for it.
    writeTag(0x2226, 0, 0x00); //version (2 bytes)
    writeTag(0x2226, 1, 0x00); 
    writeTag(0x2226, 2, 0x00); //vol (2 bytes)
    writeTag(0x2226, 3, 0x00); 
    writeTag(0x2226, 4, 0x00); //file ID (2 bytes)
    writeTag(0x2226, 5, 0x5E);
    writeTag(0x2226, 6, 0x82); //dataused (2 bytes. 0x8200, standard, it seems)
    writeTag(0x2226, 7, 0x00); 
    writeTag(0x2226, 8, 0x00); //abspage (3 bytes. The sector, 0x26? check this)
    writeTag(0x2226, 9, 0x22); 
    writeTag(0x2226, 10, 0x00); 
    writeTag(0x2226, 11, 0x00); // checksum (1 byte - will be fixed later)
    writeTag(0x2226, 12, 0x00); // relpage (2 bytes. 0x00 in this case, since we're a 1 sector piece of data)
    writeTag(0x2226, 13, 0x00);
    writeTag(0x2226, 14, 0xFF); // fwdlink (3 bytes. 0xFFFFFF says none)
    writeTag(0x2226, 15, 0xFF);
    writeTag(0x2226, 16, 0xFF);
    writeTag(0x2226, 17, 0xFF); // bkwdlink (3 bytes. 0xFFFFFF says none)
    writeTag(0x2226, 18, 0xFF);
    writeTag(0x2226, 19, 0xFF);
    fixFreeBitmap(bitmapSec, 0x2226);

    //TODO bitmapSec = findBitmapSec();
    //TODO fixFreeBitmap(bitmapSec);

    fixAllTagChecksums();

    freeSectors = countFreeSectors();
    printf("Free sectors in disk: %d\n", freeSectors);

    fwrite(image, 1, FILE_LENGTH, output);

    fclose(output);

    for (int i = 0; i < 200; i++) {
        char calculatedChecksum = calculateChecksum(i);
        printf("sec %d (0x%02X) with chksum 0x%02X:", i, DATA_OFFSET + (i * SECTOR_SIZE), (calculatedChecksum & 0xFF));
        char *tag = readTag(i);
        int fsSector = 0;
        int directorySector = 0;
        for (int j = 0; j < TAG_SIZE; j++) {
            printf("%02X ", tag[j] & 0xFF);
            if (j == 2 && tag[j] == 0x01) {
                fsSector = 1;
            }
            if (j == 5 && tag[j] == 0x04) {
                directorySector = 1;
            }
        }
        printf(" ");
        printSectorType(i);
        printf("\n");
    }

    return 0;
}
