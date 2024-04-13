#include "curl.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void test_good() {
    unsetenv("ZSYNC_CURL");
    char url[PATH_MAX] = "file://";
    char cwd[PATH_MAX] = {0};
    getcwd(cwd, sizeof(cwd));
    strcat(url, cwd);
    strcat(url, "/curl_test.txt");
    const char *options[] = {"--verbose", url, NULL};

    char *file_content = NULL;
    size_t file_size = 0;
    int ret = curl_get(options, &file_content, &file_size);
    if (ret) {
        fprintf(stderr, "curl_get failed with code %i\n", ret);
        exit(EXIT_FAILURE);
    }

    if (strcmp(file_content, "I'm a test file\n")) {
        fprintf(stderr, "unexpected output: %s", file_content);
        exit(EXIT_FAILURE);
    }
}

void test_help() {
    unsetenv("ZSYNC_CURL");
    const char *options[] = {"--help", NULL};

    char *curl_stdout = NULL;
    size_t curl_stdout_len = 0;
    int ret = curl_get(options, &curl_stdout, &curl_stdout_len);
    if (ret != 0) {
        fprintf(stderr, "curl_get failed with code %i\n", ret);
        exit(EXIT_FAILURE);
    }

    if (strncmp(curl_stdout, "Usage: curl [options...] <url>", 30)) {
        fprintf(stderr, "unexpected output: %s", curl_stdout);
        exit(EXIT_FAILURE);
    }
}

void test_notfound() {
    unsetenv("ZSYNC_CURL");
    const char *options[] = {"file:///This-file-does-not-exist", NULL};

    char *file_content = NULL;
    size_t file_size = 0;
    int ret = curl_get(options, &file_content, &file_size);
    if (ret != 37) { // "Couldn't open file"
        fprintf(stderr, "curl_get failed with code %i\n", ret);
        exit(EXIT_FAILURE);
    }

    options[0] = "http://localhost:0/invalid";
    ret = curl_get(options, &file_content, &file_size);
    if (ret != 7) { // "Failed to connect to localhost port 0"
        fprintf(stderr, "curl_get failed with code %i\n", ret);
        exit(EXIT_FAILURE);
    }
}

void test_envvar_good() {
    setenv("ZSYNC_CURL", "echo", 1);
    const char *options[] = {"I am echoed instead", NULL};

    char *curl_stdout = NULL;
    size_t curl_stdout_len = 0;
    int ret = curl_get(options, &curl_stdout, &curl_stdout_len);
    if (ret != 0) {
        fprintf(stderr, "curl_get failed with code %i\n", ret);
        exit(EXIT_FAILURE);
    }
    if (strcmp(curl_stdout, "I am echoed instead\n")) {
        fprintf(stderr, "unexpected output: %s", curl_stdout);
        exit(EXIT_FAILURE);
    }
}

void test_envvar_bad() {
    setenv("ZSYNC_CURL", "not-an-executable", 1);
    const char *options[] = {NULL};

    char *curl_stdout = NULL;
    size_t curl_stdout_len = 0;
    int ret = curl_get(options, &curl_stdout, &curl_stdout_len);
    if (ret != 127) {
        fprintf(stderr, "curl_get failed with code %i\n", ret);
        exit(EXIT_FAILURE);
    }
}

int main() {
    test_good();
    test_help();
    test_notfound();
    test_envvar_good();
    test_envvar_bad();
    return EXIT_SUCCESS;
}
