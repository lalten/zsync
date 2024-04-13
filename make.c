/*
 *   zsync - client side rsync over http
 *   Copyright (C) 2004,2005,2009 Colin Phipps <cph@moria.org.uk>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the Artistic License v2 (see the accompanying
 *   file COPYING for the full license terms), or, at your option, any later
 *   version of the same license.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   COPYING file for details.
 */

/* Command-line utility to create .zsync files */

#include "zsglobal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <math.h>
#include <time.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "librcksum/rcksum.h"
#include "libzsync/sha1.h"
#include "format_string.h"

/* We're only doing one file per run, so these are global state for the current
 * file being processed */
SHA1_CTX shactx;
size_t blocksize = 0;
off_t len = 0;

/* And settings from the command line */
int verbose = 0;

/* stream_error(function, stream) - Exit with IO-related error message */
void __attribute__ ((noreturn)) stream_error(const char *func, FILE * stream) {
    fprintf(stderr, "%s: %s\n", func, strerror(ferror(stream)));
    exit(2);
}

/* write_block_sums(buffer[], num_bytes, output_stream)
 * Given one block of data, calculate the checksums for this block and write
 * them (as raw bytes) to the given output stream */
static void write_block_sums(unsigned char *buf, size_t got, FILE * f) {
    struct rsum r;
    unsigned char checksum[CHECKSUM_SIZE];

    /* Pad for our checksum, if this is a short last block  */
    if (got < blocksize)
        memset(buf + got, 0, blocksize - got);

    /* Do rsum and checksum, and convert to network endian */
    r = rcksum_calc_rsum_block(buf, blocksize);
    rcksum_calc_checksum(&checksum[0], buf, blocksize);
    r.a = htons(r.a);
    r.b = htons(r.b);

    /* Write them raw to the stream */
    if (fwrite(&r, sizeof r, 1, f) != 1)
        stream_error("fwrite", f);
    if (fwrite(checksum, sizeof checksum, 1, f) != 1)
        stream_error("fwrite", f);
}

/* read_stream_write_blocksums(data_stream, zsync_stream)
 * Reads the data stream and writes to the zsync stream the blocksums for the
 * given data.
 */
void read_stream_write_blocksums(FILE * fin, FILE * fout) {
    unsigned char *buf = malloc(blocksize);

    if (!buf) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    while (!feof(fin)) {
        int got = fread(buf, 1, blocksize, fin);

        if (got > 0) {
             /* The SHA-1 sum, unlike our internal block-based sums, is on the whole file and nothing else - no padding */
            SHA1Update(&shactx, buf, got);

            write_block_sums(buf, got, fout);
            len += got;
        }
        else {
            if (ferror(fin))
                stream_error("fread", fin);
        }
    }
    free(buf);
}

/* fcopy(instream, outstream)
 * Copies data from one stream to the other until EOF on the input.
 */
void fcopy(FILE * fin, FILE * fout) {
    unsigned char buf[4096];
    size_t len;

    while ((len = fread(buf, 1, sizeof(buf), fin)) > 0) {
        if (fwrite(buf, 1, len, fout) < len)
            break;
    }
    if (ferror(fin)) {
        stream_error("fread", fin);
    }
    if (ferror(fout)) {
        stream_error("fwrite", fout);
    }
}

/* fcopy_hashes(hash_stream, zsync_stream, rsum_bytes, hash_bytes)
 * Copy the full block checksums from their temporary store file to the .zsync,
 * stripping the hashes down to the desired lengths specified by the last 2
 * parameters.
 */
void fcopy_hashes(FILE * fin, FILE * fout, size_t rsum_bytes, size_t hash_bytes) {
    unsigned char buf[20];
    size_t len;

    while ((len = fread(buf, 1, sizeof(buf), fin)) > 0) {
        /* write trailing rsum_bytes of the rsum (trailing because the second part of the rsum is more useful in practice for hashing), and leading checksum_bytes of the checksum */
        if (fwrite(buf + 4 - rsum_bytes, 1, rsum_bytes, fout) < rsum_bytes)
            break;
        if (fwrite(buf + 4, 1, hash_bytes, fout) < hash_bytes)
            break;
    }
    if (ferror(fin)) {
        stream_error("fread", fin);
    }
    if (ferror(fout)) {
        stream_error("fwrite", fout);
    }
}

/* len = get_len(stream)
 * Returns the length of the file underlying this stream */
off_t get_len(FILE * f) {
    struct stat s;

    if (fstat(fileno(f), &s) == -1)
        return 0;
    return s.st_size;
}

/****************************************************************************
 *
 * Main program
 */
int main(int argc, char **argv) {
    FILE *instream;
    char *fname = NULL;
    char **url = NULL;
    int nurls = 0;
    char *outfname = NULL;
    FILE *fout;
    char *infname = NULL;
    int rsum_len, checksum_len, seq_matches;
    time_t mtime = -1;

    /* Open temporary file */
    FILE *tf = tmpfile();

    {   /* Options parsing */
        int opt;
        while ((opt = getopt(argc, argv, "b:o:f:u:v")) != -1) {
            switch (opt) {
            case 'o':
                if (outfname) {
                    fprintf(stderr, "specify -o only once\n");
                    exit(2);
                }
                outfname = strdup(optarg);
                break;
            case 'f':
                if (fname) {
                    fprintf(stderr, "specify -f only once\n");
                    exit(2);
                }
                fname = strdup(optarg);
                break;
            case 'b':
                blocksize = atoi(optarg);
                if ((blocksize & (blocksize - 1)) != 0) {
                    fprintf(stderr,
                            "blocksize must be a power of 2 (512, 1024, 2048, ...)\n");
                    exit(2);
                }
                break;
            case 'u':
                url = realloc(url, (nurls + 1) * sizeof *url);
                url[nurls++] = optarg;
                break;
            case 'v':
                verbose++;
                break;
            }
        }

        /* Open data to create .zsync for - either it's a supplied filename, or stdin */
        if (optind == argc - 1) {
            infname = strdup(argv[optind]);
            instream = fopen(infname, "rb");
            if (!instream) {
                perror("open");
                exit(2);
            }

            {   /* Get mtime if available */
                struct stat st;
                if (fstat(fileno(instream), &st) == 0) {
                    mtime = st.st_mtime;
                }
            }

            /* Use supplied filename as the target filename */
            if (!fname)
                fname = basename(argv[optind]);
        }
        else {
            instream = stdin;
        }
    }

    /* If not user-specified, choose a blocksize based on size of the input file */
    if (!blocksize) {
        blocksize = (get_len(instream) < 100000000) ? 2048 : 4096;
    }

    /* Read the input file and construct the checksum of the whole file, and
     * the per-block checksums */
    SHA1Init(&shactx);
    read_stream_write_blocksums(instream, tf);

    {   /* Decide how long a rsum hash and checksum hash per block we need for this file */
        seq_matches = 1;
        rsum_len = ceil(((log(len) + log(blocksize)) / log(2) - 8.6) / 8);
        /* For large files, the optimum weak checksum size can be more than
         * what we have available. Switch to seq_matches for this case. */
        if (rsum_len > 4) {
            /* seq_matches > 1 in theory would reduce the amount of rsum_len
             * needed, since we get effectively rsum_len*seq_matches required
             * to match before a strong checksum is calculated. In practice,
             * consecutive blocks in the file can be highly correlated, so we
             * want to keep the maximum available rsum_len as well. */
            seq_matches = 2;
            rsum_len = 4;
        }

        /* min lengths of rsums to store */
        rsum_len = max(2, rsum_len);

        /* Now the checksum length; min of two calculations */
        checksum_len = max(ceil(
                (20 + (log(len) + log(1 + len / blocksize)) / log(2))
                / seq_matches / 8),
                ceil((20 + log(1 + len / blocksize) / log(2)) / 8));

        /* Keep checksum_len within 4-16 bytes */
        checksum_len = min(16, max(4, checksum_len));
    }

    if (!outfname && fname) {
        outfname = malloc(strlen(fname) + 10);
        sprintf(outfname, "%s.zsync", fname);
    }

    /* Open output file */
    if (outfname) {
        fout = fopen(outfname, "wb");
        if (!fout) {
            perror("open");
            exit(2);
        }
        free(outfname);
    }
    else {
        fout = stdout;
    }

    /* Okay, start writing the zsync file */
    // We use original zsync 0.6.2 format
    fprintf(fout, "zsync: 0.6.2\n");

    if (fname) {
        fprintf(fout, "Filename: %s\n", fname);
        if (mtime != -1) {
            char buf[32];
            struct tm mtime_tm;

            if (gmtime_r(&mtime, &mtime_tm) != NULL) {
                if (strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %z", &mtime_tm) > 0)
                    fprintf(fout, "MTime: %s\n", buf);
            }
            else {
                fprintf(stderr, "error converting %ld to struct tm\n", mtime);
            }
        }
    }
    fprintf(fout, "Blocksize: " SIZE_T_PF "\n", blocksize);
    fprintf(fout, "Length: " OFF_T_PF "\n", (intmax_t) len);
    fprintf(fout, "Hash-Lengths: %d,%d,%d\n", seq_matches, rsum_len,
            checksum_len);
    {                           /* Write URLs */
        int i;
        for (i = 0; i < nurls; i++)
            fprintf(fout, "URL: %s\n", url[i]);
    }
    if (nurls == 0 && infname) {
        /* Assume that we are in the public dir, and use relative paths.
         * Look for an uncompressed version and add a URL for that to if appropriate. */
        fprintf(fout, "URL: %s\n", infname);
        fprintf(stderr,
                "No URL given, so I am including a relative URL in the .zsync file - you must keep the file being served and the .zsync in the same public directory. Use -u %s to get this same result without this warning.\n",
                infname);
    }

    {   /* Write out SHA1 checksum of the entire file */
        unsigned char digest[SHA1_DIGEST_LENGTH];
        unsigned int i;

        fputs("SHA-1: ", fout);

        SHA1Final(digest, &shactx);

        for (i = 0; i < sizeof digest; i++)
            fprintf(fout, "%02x", digest[i]);
        fputc('\n', fout);
    }

    /* End of headers */
    fputc('\n', fout);

    /* Now copy the actual block hashes to the .zsync */
    rewind(tf);
    fcopy_hashes(tf, fout, rsum_len, checksum_len);

    /* And cleanup */
    fclose(tf);
    fclose(fout);

    return 0;
}
