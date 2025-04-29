#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#define bytes uint8_t*

// ---------- Constants ----------
// bytes
const int FILE_LENGTH = 0x4EF854; // length of a standard 5MB ProFile image
const int SECTOR_SIZE = 0x200; // bytes per sector
const int DATA_OFFSET = 0x54; // length of BLU file header
const int TAG_SIZE = 0x14; // for ProFile disks
// sectors
const int SECTORS_IN_DISK = 0x2600; // for 5MB ProFile

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

bytes readTag(const int sector) {
    bytes tag = malloc(TAG_SIZE);
    const int startIdx = DATA_OFFSET + (SECTORS_IN_DISK * SECTOR_SIZE) + (sector * TAG_SIZE);
    for (int i = 0; i < TAG_SIZE; i++) {
        tag[i] = getImage()[startIdx + i];
    }

    return tag;
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

void findSFileSec() {
    sFileSec = (int) readMDDFLong(0x94) + MDDFSec;
    sfileBlockCount = (int) readMDDFInt(0x9A);
    printf("emptyfile: 0x%02X\n", readMDDFInt(0x9E));
}

void dumpFiles() {
    const uint16_t slist_packing = readMDDFInt(0x98); //number of s_entries per block in slist
    uint16_t idx = 0;
    for (int i = sFileSec; i < (sFileSec + sfileBlockCount); i++) {
        bytes data = readSector(i);
        for (int sfileIdx = 0; sfileIdx < slist_packing; sfileIdx++) {
            if (idx < 10) {
                idx++;
                continue;
            }
            int srec = sfileIdx * 14; //length of srecord
            uint32_t fileAddr = readLong(data, srec + 4);
            if (fileAddr != 0x00000000) { //real file
                int hintSec = readLong(data, srec) + MDDFSec;
                printf("hintSec = 0x%02X\n", hintSec);
                bytes hSec = readSector(hintSec);
                int nameLength = hSec[0];
                char *name = (char *) malloc((nameLength * sizeof(char)) + 1);
                printf("Name = ");
                for (int n = 0; n < nameLength; n++) { //bytes we have to read
                    name[n] = hSec[n + 1];
                    if (name[n] == '/') {
                        name[n] = '-';
                    }
                    printf("%c", name[n]);
                }
                printf("\n");
                name[nameLength] = '\0';

                char fullpath[256];
                fullpath[0] = '\0';
                strcat(fullpath, "extracted/");
                strcat(fullpath, name);
                printf("Fullpath = %s\n", fullpath);

                FILE *output = fopen(fullpath, "w");

                int nextSec = readLong(data, srec + 4) + MDDFSec;
                while (true) {
                    printf("- nextSec = 0x%02X\n", nextSec);
                    bytes dataSec = readSector(nextSec);
                    for (int b = 0; b < SECTOR_SIZE; b++) {
                        fprintf(output, "%c", dataSec[b]);
                    }
                    free(dataSec);
                    bytes dataTags = readTag(nextSec);
                    for (int x = 0; x < TAG_SIZE; x++) {
                        printf("%02X", dataTags[x]);
                    }
                    printf("\n");
                    nextSec = (int) ((dataTags[0xE] << 16) | (dataTags[0xF] << 8) | dataTags[0x10]);
                    if ((nextSec & 0xFFFFFF) == 0xFFFFFF) {
                        free(dataTags);
                        break;
                    }
                    nextSec += MDDFSec;
                    free(dataTags);
                }

                fclose(output);
                free(hSec);
            }
            idx++;
        }
        free(data);
    }
}

int main(int argc, char *argv[]) {
    //initialize all global vars
    readFile();
    findMDDFSec();
    findSFileSec();

    dumpFiles();
    return 0;
}
