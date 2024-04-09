#include "curl.h"

#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    char url[PATH_MAX] = "url = file://";
    char cwd[PATH_MAX] = {0};
    getcwd(cwd, PATH_MAX);
    strcat(url, cwd);
    strcat(url, "/curl_test.txt");
    char *getcwd(char *buf, size_t size);
    const char *options[] = {url, NULL};

    FILE *f = curl(options);
    if (!f) {
        fprintf(stderr, "curl failed");
        return EXIT_FAILURE;
    }

    char buffer[1024] = {0};
    fgets(buffer, sizeof(buffer), f);
    if (strcmp(buffer, "I'm a test file\n")) {
        fprintf(stderr, "unexpected output: %s", buffer);
        return EXIT_FAILURE;
    }
    fclose(f);
    return EXIT_SUCCESS;
}
