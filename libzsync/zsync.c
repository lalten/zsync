
/*
 *   zsync - client side rsync over http
 *   Copyright (C) 2004,2005,2007,2009 Colin Phipps <cph@moria.org.uk>
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

/* This is the heart of zsync.
 *
 * .zsync file parsing and glue between all the main components of zsync.
 *
 * This file is where the .zsync metadata format is understood and read; it
 * extracts it and creates the corresponding rcksum object to apply the rsync
 * algorithm in constructing the target. It joins the HTTP code
 * to the rsync algorithm by converting lists of blocks from rcksum into lists
 * of byte ranges at particular URLs to be retrieved by the HTTP code.
 *
 * It also handles:
 * - blocking edge cases (last block of the file only containing partial data)
 * - checksum verification of the entire output.
 */
#include "zsglobal.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <arpa/inet.h>

#include "librcksum/rcksum.h"
#include "zsync.h"
#include "sha1.h"

/* Probably we really want a table of checksum methods here. But I've only
 * implemented SHA1 so this is it for now. */
static const char ckmeth_sha1[] = { "SHA-1" };

/****************************************************************************
 *
 * zsync_state object and methods
 * This holds a single target file's details, and holds the state of the
 * in-progress local copy of that target that we are constructing (via a
 * contained rcksum_state object)
 *
 * Also holds all the other misc data from the .zsync file.
 */
struct zsync_state {
    struct rcksum_state *rs;    /* rsync algorithm state, with block checksums and
                                 * holding the in-progress local version of the target */
    off_t filelen;              /* Length of the target file */
    int blocks;                 /* Number of blocks in the target */
    size_t blocksize;           /* Blocksize */

    /* Checksum of the entire file, and checksum alg */
    char *checksum;
    const char *checksum_method;

    /* URLs to versions of the target */
    char **url;
    int nurl;

    char *cur_filename;         /* If we have taken the filename from rcksum, it is here */

    /* Hints for the output file, from the .zsync */
    char *filename;             /* The Filename: header */

    time_t mtime;               /* MTime: from the .zsync, or -1 */
};

static int zsync_read_blocksums(struct zsync_state *zs, FILE * f,
                                int rsum_bytes, unsigned int checksum_bytes,
                                int seq_matches);
static int zsync_sha1(struct zsync_state *zs, int fh);
static time_t parse_822(const char* ts);

/* char*[] = append_ptrlist(&num, &char[], "to add")
 * Crude data structure to store an ordered list of strings. This appends one
 * entry to the list. */
static char **append_ptrlist(int *n, char **p, char *a) {
    if (!a)
        return p;
    p = realloc(p, (*n + 1) * sizeof *p);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    p[*n] = a;
    (*n)++;
    return p;
}

/* Constructor */
struct zsync_state *zsync_begin(FILE * f) {
    /* Defaults for the checksum bytes and sequential matches properties of the
     * rcksum_state. These are the defaults from versions of zsync before these
     * were variable. */
    int checksum_bytes = 16, rsum_bytes = 4, seq_matches = 1;

    /* Field names that we can ignore if present and not
     * understood. This allows new headers to be added without breaking
     * backwards compatibility, and conversely to add headers that do break
     * backwards compat and have old clients give meaningful errors. */
    char *safelines = NULL;

    /* Allocate memory for the object */
    struct zsync_state *zs = calloc(sizeof *zs, 1);

    if (!zs)
        return NULL;

    /* Any non-zero defaults here. */
    zs->mtime = -1;

    for (;;) {
        char buf[1024];
        char *p = NULL;
        int l;

        if (fgets(buf, sizeof(buf), f) != NULL) {
            if (buf[0] == '\n')
                break;
            l = strlen(buf) - 1;
            while (l >= 0
                   && (buf[l] == '\n' || buf[l] == '\r' || buf[l] == ' '))
                buf[l--] = 0;

            p = strchr(buf, ':');
        }
        if (p && *(p + 1) == ' ') {
            *p++ = 0;
            p++;
            if (!strcmp(buf, "zsync")) {
                if (!strcmp(p, "0.0.4")) {
                    fprintf(stderr, "This version of zsync is not compatible with zsync 0.0.4 streams.\n");
                    free(zs);
                    return NULL;
                }
            }
            else if (!strcmp(buf, "Min-Version")) {
                // The zsync file format we use is the one from original zsync 0.6.2
                if (strcmp(p, "0.6.2") > 0) {
                    fprintf(stderr,
                            "zsync3 supports only up to zsync 0.6.2 format, but this one requires %s or better\n",
                            p);
                    free(zs);
                    return NULL;
                }
            }
            else if (!strcmp(buf, "Length")) {
                zs->filelen = atoll(p);
            }
            else if (!strcmp(buf, "Filename")) {
                zs->filename = strdup(p);
            }
            else if (!strcmp(buf, "URL")) {
                zs->url = (char **)append_ptrlist(&(zs->nurl), zs->url, strdup(p));
            }
            else if (!strcmp(buf, "Blocksize")) {
                long blocksize = atol(p);
                if (zs->blocksize & (zs->blocksize - 1)) {
                    fprintf(stderr, "nonsensical blocksize %zu\n", zs->blocksize);
                    free(zs);
                    return NULL;
                }
                zs->blocksize = (size_t)blocksize;
            }
            else if (!strcmp(buf, "Hash-Lengths")) {
                if (sscanf
                    (p, "%d,%d,%d", &seq_matches, &rsum_bytes,
                     &checksum_bytes) != 3 || rsum_bytes < 1 || rsum_bytes > 4
                    || checksum_bytes < 3 || checksum_bytes > 16
                    || seq_matches > 2 || seq_matches < 1) {
                    fprintf(stderr, "nonsensical hash lengths line %s\n", p);
                    free(zs);
                    return NULL;
                }
            }
            else if (!strcmp(buf, ckmeth_sha1)) {
                if (strlen(p) != SHA1_DIGEST_LENGTH * 2) {
                    fprintf(stderr, "SHA-1 digest from control file is wrong length.\n");
                }
                else {
                    zs->checksum = strdup(p);
                    zs->checksum_method = ckmeth_sha1;
                }
            }
            else if (!strcmp(buf, "Safe")) {
                safelines = strdup(p);
            }
            else if (! (strcmp(buf, "Z-Filename") || strcmp(buf, "Z-URL") || strcmp(buf, "Z-Map2") || strcmp(buf, "Recompress"))){
                fprintf(stderr, "%s is not supported in zsync3.\n", buf);
            }
            else if (!strcmp(buf, "MTime")) {
                zs->mtime = parse_822(p);
            }
            else if (!safelines || !strstr(safelines, buf)) {
                fprintf(stderr,
                        "unrecognised tag %s - you need a newer version of zsync.\n",
                        buf);
                free(zs);
                return NULL;
            }
            if (zs->filelen && zs->blocksize)
                zs->blocks = (zs->filelen + zs->blocksize - 1) / zs->blocksize;
        }
        else {
            fprintf(stderr, "Bad line - not a zsync file? \"%s\"\n", buf);
            free(zs);
            return NULL;
        }
    }
    if(!zs->url) {
        fprintf(stderr, "No URL in zsync file\n");
        free(zs);
        return NULL;
    }
    if (!zs->filelen || !zs->blocksize) {
        fprintf(stderr, "Not a zsync file (looked for Blocksize and Length lines)\n");
        free(zs);
        return NULL;
    }
    if (zsync_read_blocksums(zs, f, rsum_bytes, checksum_bytes, seq_matches) != 0) {
        free(zs);
        return NULL;
    }
    return zs;
}

/* zsync_read_blocksums(self, FILE*, rsum_bytes, checksum_bytes, seq_matches)
 * Called during construction only, this creates the rcksum_state that stores
 * the per-block checksums of the target file and holds the local working copy
 * of the in-progress target. And it populates the per-block checksums from the
 * given file handle, which must be reading from the .zsync at the start of the
 * checksums.
 * rsum_bytes, checksum_bytes, seq_matches are settings for the checksums,
 * passed through to the rcksum_state. */
static int zsync_read_blocksums(struct zsync_state *zs, FILE * f,
                                int rsum_bytes, unsigned int checksum_bytes,
                                int seq_matches) {
    /* Make the rcksum_state first */
    if (!(zs->rs = rcksum_init(zs->blocks, zs->blocksize, rsum_bytes,
                               checksum_bytes, seq_matches))) {
        return -1;
    }

    /* Now read in and store the checksums */
    zs_blockid id = 0;
    for (; id < zs->blocks; id++) {
        struct rsum r = { 0, 0 };
        unsigned char checksum[CHECKSUM_SIZE];

        /* Read in */
        if (fread(((char *)&r) + 4 - rsum_bytes, rsum_bytes, 1, f) < 1
            || fread((void *)&checksum, checksum_bytes, 1, f) < 1) {

            /* Error - free the rcksum_state and tell the caller to bail */
            fprintf(stderr, "short read on control file; %s\n",
                    strerror(ferror(f)));
            rcksum_end(zs->rs);
            return -1;
        }

        /* Convert to host endian and store */
        r.a = ntohs(r.a);
        r.b = ntohs(r.b);
        rcksum_add_target_block(zs->rs, id, r, checksum);
    }
    return 0;
}

/* parse_822(buf[])
 * Parse an RFC822 date string. Returns a time_t, or -1 on failure.
 * E.g. Tue, 25 Jul 2006 20:02:17 +0000
 */
static time_t parse_822(const char* ts) {
    struct tm t = { 0 };

    if (strptime(ts, "%a, %d %b %Y %H:%M:%S %z", &t) == NULL
        && strptime(ts, "%d %b %Y %H:%M:%S %z", &t) == NULL) {
        return -1;
    }
    return mktime(&t);
}

/* zsync_blocksize(self)
 * Returns the blocksize used by zsync on this target. */
/*
static size_t zsync_blocksize(const struct zsync_state *zs) {
    return zs->blocksize;
}
*/

/* char* = zsync_filename(self)
 * Returns the suggested filename to be used for the final result of this
 * zsync.  Malloced string to be freed by the caller. */
char *zsync_filename(const struct zsync_state *zs) {
    return strdup(zs->filename);
}

/* time_t = zsync_mtime(self)
 * Returns the mtime on the original copy of the target; for the client program
 * to set the mtime of the local file to match, if it so chooses.
 * Or -1 if no mtime specified in the .zsync */
time_t zsync_mtime(const struct zsync_state *zs) {
    return zs->mtime;
}

/* zsync_status(self)
 * Returns  0 if we have no data in the target file yet.
 *          1 if we have some but not all
 *          2 or more if we have all.
 * The caller should not rely on exact values 2+; just test >= 2. Values >2 may
 * be used in later versions of libzsync. */
int zsync_status(const struct zsync_state *zs) {
    int todo = rcksum_blocks_todo(zs->rs);

    if (todo == zs->blocks)
        return 0;
    if (todo > 0)
        return 1;
    return 2;                   /* TODO: more? */
}

/* zsync_progress(self, &got, &total)
 * Writes the number of bytes got, and the total to get, into the long longs.
 */
void zsync_progress(const struct zsync_state *zs, long long *got,
                    long long *total) {

    if (got) {
        int todo = zs->blocks - rcksum_blocks_todo(zs->rs);
        *got = todo * (long long)zs->blocksize;
    }
    if (total)
        *total = zs->blocks * (long long)zs->blocksize;
}

/* zsync_get_urls(self, &num)
 * Returns a (pointer to an) array of URLs (returning the number of them in
 * num) that are remote available copies of the target file (according to the
 * .zsync).
 */
const char *const *zsync_get_urls(struct zsync_state *zs, int *n) {
    *n = zs->nurl;
    return (const char * const *) zs->url;
}

/* zsync_needed_byte_ranges(self, &num)
 * Returns an array of offsets (2*num of them) for the start and end of num
 * byte ranges in the given target, such that retrieving all these byte ranges would be
 * sufficient to obtain a complete copy of the target file.
 */
off_t *zsync_needed_byte_ranges(struct zsync_state * zs, int *num) {
    int nrange;
    off_t *byterange;
    int i;

    /* Request all needed block ranges */
    zs_blockid *blrange = rcksum_needed_block_ranges(zs->rs, &nrange, 0, 0x7fffffff);
    if (!blrange)
        return NULL;

    /* Allocate space for byte ranges */
    byterange = malloc(2 * nrange * sizeof *byterange);
    if (!byterange) {
        free(blrange);
        return NULL;
    }

    /* Now convert blocks to bytes.
     * Note: Must cast one operand to off_t as both blocksize and blrange[x]
     * are int's whereas the product must be a file offfset. Needed so we don't
     * truncate file offsets to 32bits on 32bit platforms. */
    for (i = 0; i < nrange; i++) {
        byterange[2 * i] = blrange[2 * i] * (off_t)zs->blocksize;
        byterange[2 * i + 1] = blrange[2 * i + 1] * (off_t)zs->blocksize - 1;
    }
    free(blrange);      /* And release the blocks, we're done with them */

    *num = nrange;
    return byterange;
}

/* zsync_submit_source_file(self, FILE*, progress)
 * Read the given stream, applying the rsync rolling checksum algorithm to
 * identify any blocks of data in common with the target file. Blocks found are
 * written to our local copy of the target in progress. Progress reports if
 * progress != 0  */
int zsync_submit_source_file(struct zsync_state *zs, FILE * f, int progress) {
    return rcksum_submit_source_file(zs->rs, f, progress);
}

static char *zsync_cur_filename(struct zsync_state *zs) {
    if (!zs->cur_filename)
        if (zs->rs)
            zs->cur_filename = rcksum_filename(zs->rs);

    return zs->cur_filename;
}

/* zsync_rename_file(self, filename)
 * Tell libzsync to move the local copy of the target (or under construction
 * target) to the given filename. */
int zsync_rename_file(struct zsync_state *zs, const char *f) {
    char *rf = zsync_cur_filename(zs);

    int x = rename(rf, f);

    if (!x) {
        free(rf);
        zs->cur_filename = strdup(f);
    }
    else
        perror("rename");

    return x;
}

/* zsync_complete(self)
 * Finish a zsync download. Should be called once all blocks have been
 * retrieved successfully. This returns 0 if the file passes the final
 * whole-file checksum.
 * Returns -1 on error (and prints the error to stderr)
 *          0 if successful but no checksum verified
 *          1 if successful including checksum verified
 */
int zsync_complete(struct zsync_state *zs) {
    int rc = 0;

    /* We've finished with the rsync algorithm. Take over the local copy from
     * librcksum and free our rcksum state. */
    int fh = rcksum_filehandle(zs->rs);
    zsync_cur_filename(zs);
    rcksum_end(zs->rs);
    zs->rs = NULL;

    /* Truncate the file to the exact length (to remove any trailing NULs from
     * the last block); return to the start of the file ready to verify. */
    if (ftruncate(fh, zs->filelen) != 0) {
        perror("ftruncate");
        rc = -1;
    }
    if (lseek(fh, 0, SEEK_SET) != 0) {
        perror("lseek");
        rc = -1;
    }

    /* Do checksum check */
    if (rc == 0 && zs->checksum && !strcmp(zs->checksum_method, ckmeth_sha1)) {
        rc = zsync_sha1(zs, fh);
    }
    close(fh);

    return rc;
}

/* zsync_sha1(self, filedesc)
 * Given the currently-open-and-at-start-of-file complete local copy of the
 * target, read it and compare the SHA1 checksum with the one from the .zsync.
 * Returns -1 or 1 as per zsync_complete.
 */
static int zsync_sha1(struct zsync_state *zs, int fh) {
    SHA1_CTX shactx;

    {                           /* Do SHA1 of file contents */
        unsigned char buf[4096];
        int rc;

        SHA1Init(&shactx);
        while (0 < (rc = read(fh, buf, sizeof buf))) {
            SHA1Update(&shactx, buf, rc);
        }
        if (rc < 0) {
            perror("read");
            return -1;
        }
    }

    {                           /* And compare result of the SHA1 with the one from the .zsync */
        unsigned char digest[SHA1_DIGEST_LENGTH];
        int i;

        SHA1Final(digest, &shactx);

        for (i = 0; i < SHA1_DIGEST_LENGTH; i++) {
            unsigned int j;
            sscanf(&(zs->checksum[2 * i]), "%2x", &j);
            if (j != digest[i]) {
                return -1;
            }
        }
        return 1; /* Checksum verified okay */
    }
}

/* Destructor */
char *zsync_end(struct zsync_state *zs) {
    int i;
    char *f = zsync_cur_filename(zs);

    /* Free rcksum object */
    if (zs->rs)
        rcksum_end(zs->rs);

    /* Clear download URLs */
    for (i = 0; i < zs->nurl; i++)
        free(zs->url[i]);

    /* And the rest. */
    free(zs->url);
    free(zs->checksum);
    free(zs->filename);
    free(zs);
    return f;
}

/* Next come the methods for accepting data received from the remote copies of
 * the target and incomporating them into the local copy under construction. */

/* zsync_submit_data(self, buf[], offset, blocks)
 * Passes data retrieved from the remote copy of
 * the target file to libzsync, to be written into our local copy. The data is
 * the given number of blocks at the given offset (must be block-aligned), data
 * in buf[].  */
static int zsync_submit_data(struct zsync_state *zs,
                             const unsigned char *buf, off_t offset,
                             int blocks) {
    zs_blockid blstart = offset / zs->blocksize;
    zs_blockid blend = blstart + blocks - 1;

    return rcksum_submit_blocks(zs->rs, buf, blstart, blend);
}

/****************************************************************************
 *
 * zsync_receiver object definition and methods.
 * Stores the state for a currently-running download of blocks from a
 * particular URL or version of a file to complete a file using zsync.
 *
 * This is mostly a wrapper for the zsync_state.
 */
struct zsync_receiver {
    struct zsync_state *zs;     /* The zsync_state that we are downloading for */
    unsigned char *outbuf;      /* Working buffer to keep incomplete blocks of data */
    off_t outoffset;            /* and the position in that buffer */
};

/* Constructor */
struct zsync_receiver *zsync_begin_receive(struct zsync_state *zs) {
    struct zsync_receiver *zr = malloc(sizeof(struct zsync_receiver));

    if (!zr)
        return NULL;
    zr->zs = zs;

    zr->outbuf = malloc(zs->blocksize);
    if (!zr->outbuf) {
        free(zr);
        return NULL;
    }

    zr->outoffset = 0;

    return zr;
}

/* zsync_receive_data(self, buf[], offset, buflen)
 * Adds the data in buf (buflen bytes) to this file at the given offset.
 * Returns 0 unless there's an error (e.g. the submitted data doesn't match the
 * expected checksum for the corresponding blocks)
 */
int zsync_receive_data(struct zsync_receiver *zr, const unsigned char *buf,
                       off_t offset, size_t len) {
    int ret = 0;
    size_t blocksize = zr->zs->blocksize;

    if (0 != (offset % blocksize)) {
        size_t x = len;

        if (x > blocksize - (offset % blocksize))
            x = blocksize - (offset % blocksize);

        if (zr->outoffset == offset) {
            /* Half-way through a block, so let's try and complete it */
            if (len)
                memcpy(zr->outbuf + offset % blocksize, buf, x);
            else {
                // Pad with 0s to length.
                memset(zr->outbuf + offset % blocksize, 0, len = x =
                       blocksize - (offset % blocksize));
            }

            if ((x + offset) % blocksize == 0)
                if (zsync_submit_data
                    (zr->zs, zr->outbuf, zr->outoffset + x - blocksize, 1))
                    ret = 1;
        }
        buf += x;
        len -= x;
        offset += x;
    }

    /* Now we are block-aligned */
    if (len >= blocksize) {
        int w = len / blocksize;

        if (zsync_submit_data(zr->zs, buf, offset, w))
            ret = 1;

        w *= blocksize;
        buf += w;
        len -= w;
        offset += w;

    }
    /* Store incomplete block */
    if (len) {
        memcpy(zr->outbuf, buf, len);
        offset += len;          /* not needed: buf += len; len -= len; */
    }

    zr->outoffset = offset;
    return ret;
}


/* Destructor */
void zsync_end_receive(struct zsync_receiver *zr) {
    free(zr->outbuf);
    free(zr);
}
