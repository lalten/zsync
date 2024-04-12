#include "curl.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void test_good() {
    char url[PATH_MAX] = "file://";
    char cwd[PATH_MAX] = {0};
    getcwd(cwd, sizeof(cwd));
    strcat(url, cwd);
    strcat(url, "/curl_test.txt");
    const char *options[] = {"--verbose", url, NULL};

    char *file_content = NULL;
    size_t file_size = 0;
    if (curl_get(options, &file_content, &file_size)) {
        fprintf(stderr, "curl_get failed");
        exit(EXIT_FAILURE);
    }

    if (strcmp(file_content, "I'm a test file\n")) {
        fprintf(stderr, "unexpected output: %s", file_content);
        exit(EXIT_FAILURE);
    }
}

void test_help() {
    const char *options[] = {"--help", NULL};

    char *curl_stdout = NULL;
    size_t curl_stdout_len = 0;
    int ret = curl_get(options, &curl_stdout, &curl_stdout_len);
    if (ret != 0) {
        exit(EXIT_FAILURE);
    }

    if (strncmp(curl_stdout, "Usage: curl [options...] <url>", 30)) {
        fprintf(stderr, "unexpected output: %s", curl_stdout);
        exit(EXIT_FAILURE);
    }
}

void test_notfound() {
    const char *options[] = {"file:///This-file-does-not-exist", NULL};

    char *file_content = NULL;
    size_t file_size = 0;
    int ret = curl_get(options, &file_content, &file_size);
    if (ret != 37) { // "Couldn't open file"
        exit(EXIT_FAILURE);
    }

    options[0] = "http://localhost:0/invalid";
    ret = curl_get(options, &file_content, &file_size);
    if (ret != 7) { // "Failed to connect to localhost port 0"
        exit(EXIT_FAILURE);
    }
}

int main() {
    test_good();
    test_help();
    test_notfound();
    return EXIT_SUCCESS;
}
