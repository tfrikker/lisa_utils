#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const uint32_t PROFILE_TYPE = 0x000000;
const uint32_t WIDGET_TYPE = 0x000100;
const uint32_t PRIAM_TYPE = 0x00FF00;

const int dataPerBlock = 0x200; // data bytes per block

int main (int argc, char *argv[]) {
    int tagPerBlock; //tag bytes per block

    FILE *BLU;//Declare input and output files
    FILE *DC;
    if (access("BLU.blu", F_OK ) != -1) {
        BLU = fopen("BLU.blu", "r"); //and open them
    }
    DC = fopen("ProFile.dc42", "w");

    // check disk length
    fseek(BLU, 0, SEEK_END);
    printf("Disk length: 0x%lX\n", ftell(BLU));

    // read disk name
    fseek(BLU, 0x0, SEEK_SET);
    char diskName[0x3F] = "";
    fread(diskName, 1, 0xD, BLU);
    putc(0xD, DC); // put length of name
    fwrite(diskName, 1, 0x3F, DC);
    printf("Disk name: %s\n", diskName);

    // read device type
    fseek(BLU, 0xD, SEEK_SET);
    uint32_t deviceType = 0;
    fread(&deviceType, 3, 1, BLU);
    printf("Device type: 0x%06X", deviceType);
    if (deviceType == PROFILE_TYPE) {
        printf(" (ProFile)\n");
        tagPerBlock = 0x14;
    } else if (deviceType == WIDGET_TYPE) {
        printf(" (Widget)\n");
        tagPerBlock = 0x14;
    } else if (deviceType == PRIAM_TYPE) {
        printf(" (Priam)\n");
        tagPerBlock = 0x18;
    } else {
        printf(" (unsupported disk type)\n");
        return 1;
    }

    // read device block count
    fseek(BLU, 0x12, SEEK_SET);
    uint32_t blocks_in_device = 0;
    fread(&blocks_in_device, 3, 1, BLU);
    printf("Blocks in device: 0x%06X\n", blocks_in_device);

    // read bytes per block
    fseek(BLU, 0x15, SEEK_SET);
    uint16_t bytes_per_block;
    fread(&bytes_per_block, 2, 1, BLU);
    bytes_per_block = htons(bytes_per_block);
    printf("Bytes per block: 0x%04X\n", bytes_per_block);

    // data block size in bytes
    uint32_t val = htonl(dataPerBlock * blocks_in_device);
    fwrite((const void*) & val, 4, 1, DC);

    // tag size in bytes
    val = htonl(tagPerBlock * blocks_in_device);
    fwrite((const void*) & val, 4, 1, DC);

    // data checksum
    val = 0x00000000;
    uint16_t curWord;
    fseek(BLU, 1 * (dataPerBlock + tagPerBlock), SEEK_SET); // seek to first real block
    for (int i = 0; i < blocks_in_device; i++) {
        for (int j = 0; j < dataPerBlock; j += 2) {
            fread(&curWord, 2, 1, BLU);
            curWord = htons(curWord);
            val += curWord;
            val = (val >> 1)|(val << (32 - 1)); // rotate right 1 bit
        }
        fseek(BLU, tagPerBlock, SEEK_CUR); // seek past tags
    }
    fwrite((const void*) & val, 4, 1, DC);

    // tag checksum
    val = 0x00000000;
    fseek(BLU, 2 * (dataPerBlock + tagPerBlock), SEEK_SET); // seek to first real block
    for (int i = 0; i < (blocks_in_device - 1); i++) { // skip block 0 for calculating tag checksum
        fseek(BLU, dataPerBlock, SEEK_CUR); // seek past data
        for (int j = 0; j < tagPerBlock; j++) {
            fread(&curWord, 2, 1, BLU);
            curWord = htons(curWord);
            val += curWord;
            val = (val >> 1)|(val << (32 - 1)); // rotate right 1 bit
        }
    }
    fwrite((const void*) & val, 4, 1, DC);

    // disk encoding byte (doesn't matter for this non-standard case)
    putc(0x00, DC);

    // format byte (doesn't matter for this non-standard case)
    putc(0x00, DC);

    // DC42 magic bytes
    putc(0x01, DC);
    putc(0x00, DC);

    // copy disk data
    fseek(BLU, 1 * (dataPerBlock + tagPerBlock), SEEK_SET); // seek to first real block
    for (int i = 0; i < blocks_in_device; i++) {
        for (int j = 0; j < dataPerBlock; j++) {
            putc(getc(BLU), DC); // put data
        }
        fseek(BLU, tagPerBlock, SEEK_CUR); // seek past tags
    }

    // copy tag data
    fseek(BLU, 1 * (dataPerBlock + tagPerBlock), SEEK_SET); // seek to first real block
    for (int i = 0; i < blocks_in_device; i++) {
        fseek(BLU, dataPerBlock, SEEK_CUR); // seek past data
        for (int j = 0; j < tagPerBlock; j++) {
            putc(getc(BLU), DC); // put tags
        }
    }

    fclose(BLU);
    fclose(DC);
    return 0;
}
