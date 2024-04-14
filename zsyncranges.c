
#include <stdlib.h>

#include "libzsync/zsync.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: zsyncranges file.zsync file\n");
        exit(2);
    }

    FILE *zsyncfile_stream = fopen(argv[1], "r");
    if (!zsyncfile_stream) {
        perror("fopen(zsyncfile)");
        exit(EXIT_FAILURE);
    }
    struct zsync_state *zs = zsync_begin(zsyncfile_stream);
    fclose(zsyncfile_stream);
    if (!zs) {
        exit(EXIT_FAILURE);
    }

    FILE *seedfile_stream = fopen(argv[2], "r");
    if (!seedfile_stream) {
        perror("fopen(seedfile)");
        exit(EXIT_FAILURE);
    }
    int got_blocks = zsync_submit_source_file(zs, seedfile_stream, 0);
    fclose(seedfile_stream);
    if (!got_blocks) {
        fprintf(stderr, "Error reading seed file\n");
        exit(EXIT_FAILURE);
    }

    int nrange = 0;
    off_t *zbyterange = zsync_needed_byte_ranges(zs, &nrange);
    printf("[");
    for (int i = 0; i < nrange; i++) {
        printf("[%ld,%ld]", zbyterange[i * 2], zbyterange[i * 2 + 1]);
        if (i < nrange - 1) {
            printf(",");
        }
    }
    printf("]\n");
    free(zbyterange);
    zsync_end(zs);

    exit(EXIT_SUCCESS);
}
