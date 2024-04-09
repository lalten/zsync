#include "curl.h"

#include <stdio.h>
#include <string.h>

FILE *curl(const char **restrict curl_options) {
    char config_file_name[L_tmpnam] = {0};
    tmpnam(config_file_name);
    FILE *config_file = fopen(config_file_name, "w");
    for (const char **option = curl_options; *option; ++option) {
        if(!fprintf(config_file, "%s\n", *option)){
            perror("fprintf");
            return NULL;
        }
    }
    fclose(config_file);

    char cmd[L_tmpnam + 100] = "curl --config ";
    strcat(cmd, config_file_name);
    strcat(cmd, " ; rm -f ");
    strcat(cmd, config_file_name);
    
    FILE *f = popen(cmd, "r");
    if(!f){
        perror("curl popen");
        return NULL;
    }
    return f;
}
