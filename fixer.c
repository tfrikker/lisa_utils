#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
const int DATA_PER_BLOCK = 0x200; // data bytes per block
const int TAG_PER_BLOCK = 0x14; //tag bytes per block
int main (int argc, char *argv[]) {
    FILE *BLU;//Declare input and output files
    FILE *DC;
    if (access("BLU.blu", F_OK ) != -1) {
        BLU = fopen("BLU.blu", "r"); //and open them
    }
    DC = fopen("ProFile.dc42", "w");
    fseek(BLU, 0, SEEK_END);
    printf("Disk length: 0x%lX\n", ftell(BLU));

    // read disk name
    putc(0xD, DC); // put length of name
    fseek(BLU, 0x00, SEEK_SET);
    for (int i = 0; i < 0xD; i++) {
        putc(getc(BLU), DC);
    }
    // pad rest of DC42 name space
    for (int i = 0; i < 0x3F - 0xD; i++) {
        putc(0x00, DC);
    }

    // read device type
    fseek(BLU, 0x0D, SEEK_SET);
    printf("Type of device: '");
    for (int i = 0; i < 0x03; i++) {
        printf("%02X", getc(BLU));
    }
    printf("'\n");

    // read device block count
    fseek(BLU, 0x12, SEEK_SET);
    uint32_t blocks_in_device;
    fread(&blocks_in_device, 3, 1, BLU);
    printf("Blocks in device: 0x%06X\n", blocks_in_device);

    // read bytes per block
    fseek(BLU, 0x15, SEEK_SET);
    uint16_t bytes_per_block;
    fread(&bytes_per_block, 2, 1, BLU);
    bytes_per_block = htons(bytes_per_block);
    printf("Bytes per block: 0x%04X\n", bytes_per_block);

    // data block size in bytes
    uint32_t val = htonl(DATA_PER_BLOCK * blocks_in_device);
    fwrite((const void*) & val, 4, 1, DC);

    // tag size in bytes
    val = htonl(TAG_PER_BLOCK * blocks_in_device);
    fwrite((const void*) & val, 4, 1, DC);

    // data checksum
    val = 0x00000000;
    uint16_t curWord;
    fseek(BLU, 1 * (DATA_PER_BLOCK + TAG_PER_BLOCK), SEEK_SET); // seek to first real block
    for (int i = 0; i < blocks_in_device; i++) {
        for (int j = 0; j < DATA_PER_BLOCK; j += 2) {
            fread(&curWord, 2, 1, BLU);
            curWord = htons(curWord);
            val += curWord;
            val = (val >> 1)|(val << (32 - 1)); // rotate right 1 bit
        }
        fseek(BLU, TAG_PER_BLOCK, SEEK_CUR); // seek past tags
    }
    fwrite((const void*) & val, 4, 1, DC);

    // tag checksum
    val = 0x00000000;
    fseek(BLU, 2 * (DATA_PER_BLOCK + TAG_PER_BLOCK), SEEK_SET); // seek to first real block
    for (int i = 0; i < (blocks_in_device - 1); i++) { // skip block 0 for calculating tag checksum
        fseek(BLU, DATA_PER_BLOCK, SEEK_CUR); // seek past data
        for (int j = 0; j < 0x14; j++) {
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
    fseek(BLU, 1 * (DATA_PER_BLOCK + TAG_PER_BLOCK), SEEK_SET); // seek to first real block
    for (int i = 0; i < blocks_in_device; i++) {
        for (int j = 0; j < DATA_PER_BLOCK; j++) {
            putc(getc(BLU), DC); // put data
        }
        fseek(BLU, TAG_PER_BLOCK, SEEK_CUR); // seek past tags
    }

    // copy tag data
    fseek(BLU, 1 * (DATA_PER_BLOCK + TAG_PER_BLOCK), SEEK_SET); // seek to first real block
    for (int i = 0; i < blocks_in_device; i++) {
        fseek(BLU, DATA_PER_BLOCK, SEEK_CUR); // seek past data
        for (int j = 0; j < TAG_PER_BLOCK; j++) {
            putc(getc(BLU), DC); // put tags
        }
    }

    fclose(BLU);
    fclose(DC);
    return 0;
}
