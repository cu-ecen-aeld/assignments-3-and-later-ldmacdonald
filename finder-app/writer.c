#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
    FILE *fptr;
openlog("writer.c", LOG_USER, LOG_LOCAL1);
// 2 inputs + the program name
if (argc != 3){
    printf("ERROR: Invalid Number of Arguments.");
    printf("Total number of arguments should be 2.");
    printf("The order of the arguments should be:");
    printf("  1)The Full Path to the File to Write.");
    printf("  2)String to be written into the designated file.");
}

fptr = fopen(argv[1],"w");

if(fptr == NULL){
    printf("Error: could not open file.");
    // SysLog call
    // 3 levels of syslog debug, info, errors
    // debug -> debug mode
    // error -> error happened `err no` if known, save every time
    //openlog("writer.c", LOG_USER, LOG_LOCAL1);  // default syslog /var/log/syslog  <file>
    syslog(LOG_ERR, "Error: could not open file.");
    closelog(); // remember read/write lock
    exit(1);
}

/*
Don't need to create directory
Write to file using arguments as described above
Error if string couldn't be written to file
*/
fprintf(fptr, "%s", argv[2]);

if(ferror(fptr)) {
    char message[] = "Writing ";
    strcat(message, argv[2]);
    strcat(message, " to ");
    strcat(message, argv[1]);

    syslog(LOG_DEBUG, message);
}

closelog();
fclose(fptr);
}