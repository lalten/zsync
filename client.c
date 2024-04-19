
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

/* zsync command-line client program */

#include "zsglobal.h"

#include <assert.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "libzsync/zsync.h"

#include "curl.h"
#include "progress.h"
#include "url.h"

/* read_seed_file(zsync, filename_str)
 * Reads the given file and applies the rsync
 * checksum algorithm to it, so any data that is contained in the target file
 * is written to the in-progress target. So use this function to supply local
 * source files which are believed to have data in common with the target.
 */
void read_seed_file(struct zsync_state *z, const char *fname) {
    {
        /* Simple file - open it */
        FILE *f = fopen(fname, "r");
        if (!f) {
            perror("open");
            fprintf(stderr, "not using seed file %s\n", fname);
        } else {

            /* Give the contents to libzsync to read, to find any content that
             * is part of the target file. */
            if (!no_progress)
                fprintf(stderr, "reading seed file %s: ", fname);
            if (zsync_submit_source_file(z, f, !no_progress) < 0) {
                fprintf(stderr, "error reading seed file %s\n", fname);
            }

            /* And close */
            if (fclose(f) != 0) {
                perror("close");
            }
        }
    }

    { /* And print how far we've progressed towards the target file */
        long long done, total;

        zsync_progress(z, &done, &total);
        if (!no_progress)
            fprintf(stderr, "\rDone reading %s. %02.1f%% of target obtained.      \n", fname, (100.0f * done) / total);
    }
}

long long http_down;
char *referer;

/* A ptrlist is a very simple structure for storing lists of pointers. This is
 * the only function in its API. The structure (not actually a struct) consists
 * of a (pointer to a) void*[] and an int giving the number of entries.
 *
 * ptrlist = append_ptrlist(&entries, ptrlist, new_entry)
 * Like realloc(2), this returns the new location of the ptrlist array; the
 * number of entries is passed by reference and updated in place. The new entry
 * is appended to the list.
 */
static void **append_ptrlist(int *n, void **p, void *a) {
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

/* zs = read_zsync_control_file(location_str)
 * Reads a zsync control file from either a URL or filename specified in
 * location_str. This is treated as a URL if no local file exists of that name
 * and it starts with a URL scheme ; only http URLs are supported.
 */
struct zsync_state *read_zsync_control_file(const char *p) {
    const char *curl_options[] = {
        "--fail-with-body", "--silent", "--show-error", "--location", "--netrc", p, NULL,
    };
    char *buffer = NULL;
    size_t buffer_size = 0;
    int ret = curl_get(curl_options, &buffer, &buffer_size);
    if (ret) {
        fprintf(stderr, "curl exited %i, Failed to download %s\n", ret, p);
        exit(1);
    }
    FILE *stream = fmemopen(buffer, buffer_size, "r");
    if (!stream) {
        perror("fmemopen");
        exit(1);
    }
    /* Read the .zsync */
    struct zsync_state *zs;
    if ((zs = zsync_begin(stream, false)) == NULL) {
        exit(1);
    }

    fclose(stream);
    free(buffer);
    return zs;
}

/* str = get_filename_prefix(path_str)
 * Returns a (malloced) string of the alphanumeric leading segment of the
 * filename in the given file path.
 */
static char *get_filename_prefix(const char *p) {
    char *s = strdup(p);
    char *t = strrchr(s, '/');
    char *u;

    if (t)
        *t++ = 0;
    else
        t = s;
    u = t;
    while (isalnum(*u)) {
        u++;
    }
    *u = 0;
    if (*t > 0)
        t = strdup(t);
    else
        t = NULL;
    free(s);
    return t;
}

/* filename_str = get_filename(zs, source_filename_str)
 * Returns a (malloced string with a) suitable filename for a zsync download,
 * using the given zsync state and source filename strings as hints. */
char *get_filename(const struct zsync_state *zs, const char *source_name) {
    char *p = zsync_filename(zs);
    char *filename = NULL;

    if (p) {
        if (strchr(p, '/')) {
            fprintf(stderr, "Rejected filename specified in %s, contained path component.\n", source_name);
            free(p);
        } else {
            char *t = get_filename_prefix(source_name);

            if (t && !memcmp(p, t, strlen(t)))
                filename = p;
            else
                free(p);

            if (t && !filename) {
                fprintf(stderr, "Rejected filename specified in %s - prefix %s differed from filename %s.\n",
                        source_name, t, p);
            }
            free(t);
        }
    }
    if (!filename) {
        filename = get_filename_prefix(source_name);
        if (!filename)
            filename = strdup("zsync-download");
    }
    return filename;
}

/* fetch_remaining_blocks_http(struct zsync*, const char* url)
 * For the given zsync_state, using the given absolute HTTP URL,
 * retrieve the parts of the target that are currently missing.
 * Returns 0 if this URL was useful, non-zero if we crashed and burned.
 */
int fetch_remaining_blocks_http(struct zsync_state *z, const char *u) {
    int ret = 0;
    struct zsync_receiver *zr = zsync_begin_receive(z);
    if (!zr) {
        return -1;
    }

    if (!no_progress)
        fprintf(stderr, "downloading new blocks from %s:\n", u);

    /* Get a set of byte ranges that we need to complete the target */
    int nrange = 0;
    off_t *zbyterange = zsync_needed_byte_ranges(z, &nrange);
    if (!zbyterange)
        return 1;
    if (nrange == 0)
        return 0;

    const char *curl_options[] = {
        NULL, "--fail-with-body", "--silent", "--show-error", "--location", "--netrc", u, NULL,
    };

    /* Loop while we're receiving data, until we're done or there is an error */
    off_t zoffset = 0;
    for (int i = 0; i < nrange; i++) {
        assert(zbyterange[i * 2 + 1] > zbyterange[i * 2]);
        size_t len = zbyterange[i * 2 + 1] - zbyterange[i * 2] + 1;
        zoffset = zbyterange[i * 2];
        if (!no_progress) {
            fprintf(stderr, "Getting range %d/%d: %ld+%ld\n", i + 1, nrange, zoffset, len);
        }
        char range_option[PATH_MAX] = "";
        sprintf(range_option, "--range %ld-%ld", zoffset, zoffset + len);
        curl_options[0] = range_option;

        char *buf = NULL;
        size_t buf_size = 0;
        int ret = curl_get(curl_options, &buf, &buf_size);
        if (ret) {
            fprintf(stderr, "curl exited %i, failed to download range %ld-%ld of %s\n", ret, zoffset, zoffset + len, u);
            ret = 1;
            break;
        }
        if (buf_size < len) {
            if (i == nrange - 1) {
                // The last block may have an end past the end of the file.
                // zsync_complete will truncate the file to the correct size.
                buf = realloc(buf, len);
                memset(buf + buf_size, 0, len - buf_size);
            } else {
                fprintf(stderr, "Unexpected short curl read (got %ld, expected %ld)\n", buf_size, len);
                ret = 1;
                break;
            }
        }
        /* Pass received data to the zsync receiver, which writes it to the
         * appropriate location in the target file */
        if (zsync_receive_data(zr, (unsigned char *)buf, zoffset, len) != 0)
            ret = 1;

        free(buf);

        // Needed in case next call returns len=0 and we need to signal where the EOF was.
        zoffset += len;
        http_down += buf_size;
    }
    /* Let the zsync receiver know that we're at EOF; there
     * could be data in its buffer that it can use or needs to process */
    if (zsync_receive_data(zr, NULL, zoffset, 0) != 0)
        ret = 1;

    /* Clean up */
    free(zbyterange);
    zsync_end_receive(zr);
    return ret;
}

/* fetch_remaining_blocks_from_url(struct zsync_state*, url)
 * For the given zsync_state, using the given URL,
 * retrieve the parts of the target that are currently missing.
 * Returns true if this URL was useful, false if we crashed and burned.
 */
int fetch_remaining_blocks_from_url(struct zsync_state *zs, const char *url) {
    /* URL might be relative - we need an absolute URL to do a fetch */
    char *abs_url = make_url_absolute(referer, url);
    if (!abs_url) {
        fprintf(stderr,
                "URL '%s' from the .zsync file is relative, but I don't know the referer URL (you probably downloaded "
                "the .zsync separately and gave it to me as a file). I need to know the referring URL (the URL of the "
                ".zsync) in order to locate the download. You can specify this with -u (or edit the URL line(s) in the "
                ".zsync file you have).\n",
                url);
        return -1;
    }
    /* Try fetching data from this URL */
    int rc = fetch_remaining_blocks_http(zs, abs_url);
    if (rc != 0) {
        fprintf(stderr, "failed to retrieve from %s\n", abs_url);
    }
    free(abs_url);
    return rc;
}

/* int fetch_remaining_blocks(struct zsync_state*)
 * Using the URLs in the supplied zsync state, downloads data to complete the
 * target file.
 * Returns 0 if there were no URLs to download from, 1 if there were (in which
 * case consult zsync_status to see how far it got).
 */
int fetch_remaining_blocks(struct zsync_state *zs) {
    int n;
    const char *const *url = zsync_get_urls(zs, &n);
    int *status; /* keep status for each URL - 0 means no error */
    int ok_urls = n;

    if (!url) {
        fprintf(stderr, "No download URLs known");
        return 0;
    }
    status = calloc(n, sizeof *status);

    /* Keep going until we're done or have no useful URLs left */
    while (zsync_status(zs) < 2 && ok_urls) {
        /* Still need data; pick a URL to use. */
        int try = rand() % n;

        if (!status[try]) {
            /* Try fetching data from this URL */
            int rc = fetch_remaining_blocks_from_url(zs, url[try]);
            if (rc != 0) {
                status[try] = 1;
                ok_urls--;
            }
        }
    }
    free(status);
    return 1;
}

static int set_mtime(char *filename, time_t mtime) {
    struct stat s;
    struct utimbuf u;

    /* Get the access time, which I don't want to modify. */
    if (stat(filename, &s) != 0) {
        perror("stat");
        return -1;
    }

    /* Set the modification time. */
    u.actime = s.st_atime;
    u.modtime = mtime;
    if (utime(filename, &u) != 0) {
        perror("utime");
        return -1;
    }
    return 0;
}

/****************************************************************************
 *
 * Main program */
int main(int argc, char **argv) {
    struct zsync_state *zs;
    char *temp_file = NULL;
    char **seedfiles = NULL;
    int nseedfiles = 0;
    char *filename = NULL;
    long long local_used;
    time_t mtime;

    srand(getpid());
    { /* Option parsing */
        int opt;

        while ((opt = getopt(argc, argv, "o:i:qu:")) != -1) {
            switch (opt) {
            case 'o':
                free(filename);
                filename = strdup(optarg);
                break;
            case 'i':
                seedfiles = (char **)append_ptrlist(&nseedfiles, (void **)seedfiles, optarg);
                break;
            case 'q':
                no_progress = 1;
                break;
            case 'u':
                referer = strdup(optarg);
                break;
            }
        }
    }

    /* Last and only non-option parameter must be the path/URL of the .zsync */
    if (optind == argc) {
        fprintf(stderr, "No .zsync file specified.\nUsage: zsync http://example.com/some/filename.zsync\n");
        exit(3);
    } else if (optind < argc - 1) {
        fprintf(stderr, "Usage: zsync http://example.com/some/filename.zsync\n");
        exit(3);
    }

    /* No progress display except on terminal */
    if (!isatty(0))
        no_progress = 1;

    /* STEP 1: Read the zsync control file */
    if ((zs = read_zsync_control_file(argv[optind])) == NULL)
        exit(1);

    /* Get eventual filename for output, and filename to write to while working */
    if (!filename)
        filename = get_filename(zs, argv[optind]);
    temp_file = malloc(strlen(filename) + 6);
    strcpy(temp_file, filename);
    strcat(temp_file, ".part");

    { /* STEP 2: read available local data and fill in what we know in the
       * target file */
        int i;

        /* If the target file already exists, we're probably updating that file
         * - so it's a seed file */
        if (!access(filename, R_OK)) {
            seedfiles = (char **)append_ptrlist(&nseedfiles, (void **)seedfiles, filename);
        }
        /* If the .part file exists, it's probably an interrupted earlier
         * effort; a normal HTTP client would 'resume' from where it got to,
         * but zsync can't (because we don't know this data corresponds to the
         * current version on the remote) and doesn't need to, because we can
         * treat it like any other local source of data. Use it now. */
        if (!access(temp_file, R_OK)) {
            seedfiles = (char **)append_ptrlist(&nseedfiles, (void **)seedfiles, temp_file);
        }

        /* Try any seed files supplied by the command line */
        for (i = 0; i < nseedfiles; i++) {
            int dup = 0, j;

            /* And stop reading seed files once the target is complete. */
            if (zsync_status(zs) >= 2)
                break;

            /* Skip dups automatically, to save the person running the program
             * having to worry about this stuff. */
            for (j = 0; j < i; j++) {
                if (!strcmp(seedfiles[i], seedfiles[j]))
                    dup = 1;
            }

            /* And now, if not a duplicate, read it */
            if (!dup)
                read_seed_file(zs, seedfiles[i]);
        }
        free(seedfiles);

        /* Show how far that got us */
        zsync_progress(zs, &local_used, NULL);

        /* People that don't understand zsync might use it wrongly and end up
         * downloading everything. Although not essential, let's hint to them
         * that they probably messed up. */
        if (!local_used) {
            if (!no_progress)
                fputs("No relevent local data found - I will be downloading the whole file. If that's not what you "
                      "want, CTRL-C out. You should specify the local file is the old version of the file to download "
                      "with -i. Or perhaps you just have no data that helps download the file\n",
                      stderr);
        }
    }

    /* libzsync has been writing to a randomely-named temp file so far -
     * because we didn't want to overwrite the .part from previous runs. Now
     * we've read any previous .part, we can replace it with our new
     * in-progress run (which should be a superset of the old .part - unless
     * the content changed, in which case it still contains anything relevant
     * from the old .part). */
    if (zsync_rename_file(zs, temp_file) != 0) {
        perror("rename");
        exit(1);
    }

    /* STEP 3: fetch remaining blocks via the URLs from the .zsync */
    {
        int fetch_status = fetch_remaining_blocks(zs);
        int target_status = zsync_status(zs);
        if (target_status < 2) {
            fprintf(
                stderr,
                "%s. Incomplete transfer left in %s.\n(If this is the download filename with .part appended, zsync "
                "will automatically pick this up and reuse the data it has already done if you retry in this dir.)\n",
                fetch_status == 0    ? "No download URLs are known, so no data could be downloaded. The .zsync file is "
                                       "probably incomplete."
                : target_status == 0 ? "No data downloaded - none of the download URLs worked"
                                     : "Not all of the required data could be downloaded, and the remaining data could "
                                       "not be retrieved from any of the download URLs.",
                temp_file);
            exit(3);
        }
    }

    { /* STEP 4: verify download */
        int r;

        if (!no_progress)
            printf("verifying download...");
        r = zsync_complete(zs);
        switch (r) {
        case -1:
            fprintf(stderr, "Aborting, download available in %s\n", temp_file);
            exit(2);
        case 0:
            if (!no_progress)
                printf("no recognised checksum found\n");
            break;
        case 1:
            if (!no_progress)
                printf("checksum matches OK\n");
            break;
        }
    }

    free(temp_file);

    /* Get any mtime that we is suggested to set for the file, and then shut
     * down the zsync_state as we are done on the file transfer. Getting the
     * current name of the file at the same time. */
    mtime = zsync_mtime(zs);
    temp_file = zsync_end(zs);

    /* STEP 5: Move completed .part file into place as the final target */
    if (filename) {
        char *oldfile_backup = malloc(strlen(filename) + 8);
        int ok = 1;

        strcpy(oldfile_backup, filename);
        strcat(oldfile_backup, ".zs-old");

        if (!access(filename, F_OK)) {
            /* Backup the old file. */
            /* First, remove any previous backup. We don't care if this fails -
             * the link below will catch any failure */
            unlink(oldfile_backup);

            /* Try linking the filename to the backup file name, so we will
               atomically replace the target file in the next step.
               If that fails due to EPERM, it is probably a filesystem that
               doesn't support hard-links - so try just renaming it to the
               backup filename. */
            if (link(filename, oldfile_backup) != 0 && (errno != EPERM || rename(filename, oldfile_backup) != 0)) {
                perror("linkname");
                fprintf(stderr, "Unable to back up old file %s - completed download left in %s\n", filename, temp_file);
                ok = 0; /* Prevent overwrite of old file below */
            }
        }
        if (ok) {
            /* Rename the file to the desired name */
            if (rename(temp_file, filename) == 0) {
                /* final, final thing - set the mtime on the file if we have one */
                if (mtime != -1)
                    set_mtime(filename, mtime);
            } else {
                perror("rename");
                fprintf(stderr, "Unable to back up old file %s - completed download left in %s\n", filename, temp_file);
            }
        }
        free(oldfile_backup);
        free(filename);
    } else {
        printf("No filename specified for download - completed download left in %s\n", temp_file);
    }

    /* Final stats and cleanup */
    if (!no_progress) {
        float local_percent = 100.0f * local_used / (local_used + http_down);
        float http_percent = 100.0f * http_down / (local_used + http_down);
        printf("used %lld (%.2f%%) local, fetched %lld (%.2f%%)\n", local_used, local_percent, http_down, http_percent);
    }
    free(referer);
    free(temp_file);
    return 0;
}
