#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "librcksum/rcksum.h"
#include "libzsync/zsync.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: zsyncranges file.zsync file\n");
        exit(2);
    }

    FILE *zsyncfile_stream;
    if (strcmp(argv[1], "-") == 0) {
        zsyncfile_stream = stdin;
    } else {
        zsyncfile_stream = fopen(argv[1], "r");
        if (!zsyncfile_stream) {
            perror(argv[1]);
            exit(EXIT_FAILURE);
        }
    }
    struct zsync_state *zs = zsync_begin(zsyncfile_stream, true);
    fclose(zsyncfile_stream);
    if (!zs) {
        fprintf(stderr, "zsync_begin failed\n");
        exit(EXIT_FAILURE);
    }

    FILE *seedfile_stream = fopen(argv[2], "r");
    if (!seedfile_stream) {
        perror(argv[2]);
        exit(EXIT_FAILURE);
    }
    int num_blocks = zsync_submit_source_file(zs, seedfile_stream, 0);
    fclose(seedfile_stream);
    if (num_blocks < 0) {
        fprintf(stderr, "Error reading seed file\n");
        exit(EXIT_FAILURE);
    }

    printf("{\"length\":%ld", zsync_get_filelength(zs));

    const char *checksum = NULL;
    const char *checksum_method = NULL;
    zsync_get_checksum(zs, &checksum, &checksum_method);
    printf(",\"checksum\":{\"%s\":\"%s\"}", checksum_method, checksum);

    struct reuseable_range *rr;
    size_t len_rr;
    zsync_get_reuseable_ranges(zs, &rr, &len_rr);
    printf(",\"reuse\":[");
    for (size_t i = 0; i < len_rr; i++) {
        printf("[%ld,%ld,%zu]", rr[i].dst, rr[i].src, rr[i].len);
        if (i < len_rr - 1) {
            printf(",");
        }
    }

    printf("],\"download\":[");
    int nrange = 0;
    off_t *zbyterange = zsync_needed_byte_ranges(zs, &nrange);
    for (int i = 0; i < nrange; i++) {
        printf("[%ld,%ld]", zbyterange[i * 2], zbyterange[i * 2 + 1]);
        if (i < nrange - 1) {
            printf(",");
        }
    }
    printf("]}\n");

    free(zbyterange);
    char *temp_file = zsync_end(zs);
    unlink(temp_file);
    free(temp_file);

    exit(EXIT_SUCCESS);
}
