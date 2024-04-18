#include "curl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *make_curl_cmd(const char **restrict curl_options) {
    char *cmd_buf = NULL;
    size_t cmd_size;
    FILE *cmd_stream = open_memstream(&cmd_buf, &cmd_size);
    if (!cmd_stream) {
        perror("open_memstream(cmd_buf)");
        return NULL;
    }
    const char *curl_cmd = getenv("ZSYNC_CURL");
    fprintf(cmd_stream, "%s", curl_cmd ?: "curl");
    for (const char **option = curl_options; *option; ++option) {
        fprintf(cmd_stream, " %s", *option);
    }
    if (fclose(cmd_stream)) {
        perror("fclose(cmd_stream)");
        return NULL;
    }
    return cmd_buf;
}

void print_output_on_error(int ret, const char *restrict cmd_buf, const char *restrict buf, size_t buf_size) {
    fprintf(stderr, "\"%s\" exited with code %i", cmd_buf, ret);
    if (buf_size) {
        fprintf(stderr, ": %s", buf);
    }
    fprintf(stderr, "\n");
}

int get_curl_stdout(const char *cmd_buf, char **buf, size_t *curl_stdout_len) {
    FILE *curl_stdout = popen(cmd_buf, "r");
    if (!curl_stdout) {
        perror("curl popen");
        return -1;
    }

    FILE *memstream = open_memstream(buf, curl_stdout_len);
    if (!memstream) {
        perror("open_memstream");
        pclose(curl_stdout);
        return -1;
    }

    int c = EOF;
    while ((c = fgetc(curl_stdout)) != EOF) {
        if (fputc(c, memstream) == EOF) {
            perror("fputc(memstream)");
        }
    }

    if (fclose(memstream)) {
        perror("fclose(memstream)");
        pclose(curl_stdout);
        return -1;
    }

    int ret = pclose(curl_stdout);
    if (ret == -1) {
        perror("pclose(curl_stdout)");
    }
    ret = WEXITSTATUS(ret);
    if (ret) {
        print_output_on_error(ret, cmd_buf, *buf, *curl_stdout_len);
        free(*buf);
        *buf = NULL;
    }
    return ret;
}

int curl_get(const char **curl_options, char **restrict bufloc, size_t *sizeloc) {
    const char *cmd_buf = make_curl_cmd(curl_options);
    if (!cmd_buf) {
        return -1;
    }
    int ret = get_curl_stdout(cmd_buf, bufloc, sizeloc);
    free((void *)cmd_buf);
    return ret;
}
