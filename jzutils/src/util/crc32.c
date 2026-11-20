/* ======================================================================== */
/*  CRC32 -- Wrapper program around file_crc32() that calculates and shows  */
/*           CRC values for files.                                          */
/* ======================================================================== */

#include "config.h"
#include "lzoe/lzoe.h"
#include "file/file.h"
#include "misc/crc32.h"
#include "misc/file_crc32.h"

int main(int argc, char *argv[])
{
    int status = 0;

    if (argc < 2)
    {
        printf("Usage: %s file [file [file [...]]]\n", argv[0]);
        exit(1);
    }

    while (0<--argc)
    {
        const char *fname = *++argv;
        LZFILE *f = lzoe_fopen(fname, "rb");

        if (!f)
        {
            fprintf(stderr, "%s: cannot open '%s'\n", argv[0], fname);
            status = 1;
            continue;
        }
        lzoe_fclose(f);

        printf("%.8X  %s\n", file_crc32(fname), fname);
    }

    return status;
}
