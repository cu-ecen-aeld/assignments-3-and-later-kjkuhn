#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "syslog.h"


int main(int argc, char **argv)
{
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
    if(argc != 3)
    {
        syslog(LOG_ERR, "Usage: %s <file> <string>", argv[0]);
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        closelog();
        return 1;
    }
    FILE *f = fopen(argv[1], "w");
    if(f == NULL)
    {
        syslog(LOG_ERR, "Failed to open file %s", argv[1]);
        fprintf(stderr, "Failed to open file %s\n", argv[1]);
        closelog();
        return 1;
    }
    if(fputs(argv[2], f) == EOF)
    {
        syslog(LOG_ERR, "Failed to write to file %s", argv[1]);
        fprintf(stderr, "Failed to write to file %s\n", argv[1]);
        fclose(f);
        closelog();
        return 1;
    }
    fclose(f);
    closelog();
    return 0;
}