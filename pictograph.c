#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define H_LINECOUNT 160
#define V_LINECOUNT 112
#define H_LINELENGTH 100
#define bpp 5

#define BSWP16(in) ((in & 0xFF00) >> 8  | \
                    (in & 0x00FF) << 8)

void usage(char * p)
{
    printf("error, no file specified\n");
    printf("syntax:\n");
    printf("%s <flashram save> <slot number> <bitmap to insert>\n",p);
    printf("Note: bitmap should be grayscale 5bpp sized to 160x112\n");
}

void mmsum(unsigned char *data, unsigned int size, char * slot)
{
    if(strcmp((const char*)&data[0x24],"ZELDA3"))
        return;
    int i;
    unsigned short sum = 0;
    for(i = 0; i < size; i++)
    {
        sum+=data[i];
    }
    sum -= data[0x0100A] + data[0x0100B];
    if(sum != BSWP16(*(unsigned short*)&data[0x0100A]))
    {
        printf("Bad checksum for %s\n", slot);
        *(unsigned short*)&data[0x0100A] = BSWP16(sum);
    }
}

uint32_t BitRead(uint8_t *buf, uint32_t offset, uint32_t size)
{
	int result = 0;
	int i;
	for (i = 0; i < size; i++)
		if (buf[(offset+i)/8] & (0x80 >> ((offset+i)%8)))
			result += 1 << (size-i-1);
	return result;
}

void BitWrite(uint8_t *buf, uint32_t offset, uint32_t size, uint32_t value)
{
    int i;
    for(i = 0; i < size; i++)
    {
        int div = (offset+i)/8,
            mod = (offset+i)%8;
        buf[div] &= ~(0x80 >> mod);
        buf[div] |= ((value >> (size-i-1))) << (7 - mod);
    }
}

int main(int argc, char *argv[])
{
    int i, j;
    if(argc < 4)
    {
        usage(argv[0]);
        return -1;
    }
    /* Process slot number */
    int slot = atoi(argv[2]);
    if(slot != 1 && slot != 2)
    {
        printf("Error slot number must be 1 or 2");
        abort();
    }

    char *flashram_name = argv[1],
         *bitmap_name   = argv[3];

    /* Load FlashRAM file */
    FILE *flashram_file = fopen(flashram_name, "rb+");
    if(flashram_file == NULL)
    {
        printf("Error opening flashram file\n");
        abort();
    }
    unsigned char *flashram_buf = (unsigned char*)malloc(131072);
    if(flashram_buf == NULL)
    {
        printf("Error allocating memory for flashram file\n");
        abort();
    }
    i = fread(flashram_buf, 1, 131072, flashram_file);
    if(i != 131072)
    {
        printf("Error reading flashram file into memory\n");
        abort();
    }

    /* Load Bitmap file */
    FILE *bitmap_file = fopen(bitmap_name, "rb+");
    if(bitmap_file == NULL)
    {
        printf("Error opening bitmap file\n");
        abort();
    }
    fseek(bitmap_file, 0, SEEK_END);
    int bmp_len = ftell(bitmap_file);
    fseek(bitmap_file, 0, SEEK_SET);
    unsigned char *bitmap_buf = (unsigned char*)malloc(bmp_len);
    if(bitmap_buf == NULL)
    {
        printf("Error allocating memory for bitmap file\n");
        abort();
    }

    i = fread(bitmap_buf, 1, bmp_len, bitmap_file);
    if(i != bmp_len)
    {
        printf("Error reading bitmap file into memory\n");
        abort();
    }
    fclose(bitmap_file);

    /* Sanity checks on the bitmap */
    if(*(uint16_t*)&bitmap_buf[0x12] != 160)
    {
        printf("Error bitmap image width is not 160\n");
        abort();
    }
    if(*(uint16_t*)&bitmap_buf[0x16] != 112)
    {
        printf("Error bitmap image height is not 112\n");
        abort();
    }
    /*if(*(uint16_t*)&bitmap_buf[0x22] != 17920)
    {
        printf("Error bitmap pixel array size must be 17920 bytes (160*112*8bpp)\n");
        abort();
    }*/

    printf("Writing image to 0x%08X\n", slot * 0x8000);
    for(i = 0; i < V_LINECOUNT; i++)
    {
        for(j = 0; j < H_LINECOUNT; j++)
        {
            BitWrite(flashram_buf, slot * 0x8000 * 8 + 0x10E0 * 8 + (i * H_LINECOUNT * bpp) + (j * bpp), 5, bitmap_buf[bmp_len - (i+1) * H_LINECOUNT + j] & 0x1F);
        }
    }
    printf("Fixing checksums...\n");
    mmsum(&flashram_buf[0x00000], 0x2000, "slot 1 chunk 1 (Fixed)");
    mmsum(&flashram_buf[0x02000], 0x2000, "slot 1 chunk 2 (Fixed)");
    mmsum(&flashram_buf[0x04000], 0x2000, "slot 2 chunk 1 (Fixed)");
    mmsum(&flashram_buf[0x06000], 0x2000, "slot 2 chunk 2 (Fixed)");
    mmsum(&flashram_buf[0x08000], 0x4000, "slot 1 owl save chunk 1 (Fixed)");
    mmsum(&flashram_buf[0x0C000], 0x4000, "slot 1 owl save chunk 2 (Fixed)");
    mmsum(&flashram_buf[0x10000], 0x4000, "slot 2 owl save chunk 1 (Fixed)");
    mmsum(&flashram_buf[0x14000], 0x4000, "slot 2 owl save chunk 2 (Fixed)");
    fseek(flashram_file, 0, SEEK_SET);
    if(fwrite(flashram_buf, 1, 131072, flashram_file) != 131072)
    {
        printf("Error writing flashram back to disk\n");
        abort();
    }
    free(bitmap_buf);
    free(flashram_buf);
    fclose(flashram_file);

    return 0;
}
