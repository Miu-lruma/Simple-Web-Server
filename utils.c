/* utils.c: spidey utilities */

#include "spidey.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

/**
 * Determine mime-type from file extension
 *
 * This function first finds the file's extension and then scans the contents
 * of the MimeTypesPath file to determine which mimetype the file has.
 *
 * The MimeTypesPath file (typically /etc/mime.types) consists of rules in the
 * following format:
 *
 *  <MIMETYPE>      <EXT1> <EXT2> ...
 *
 * This function simply checks the file extension version each extension for
 * each mimetype and returns the mimetype on the first match.
 *
 * If no extension exists or no matching mimetype is found, then return
 * DefaultMimeType.
 *
 * This function returns an allocated string that must be free'd.
 **/

    char *
determine_mimetype(const char *path)
{
    char *ext;
    char *mimetype = NULL;
    char *token;
    char *subtoken; 
    char line[BUFSIZ];
    FILE *fs = NULL;   

    /* Find file extension */
    ext = strrchr(path,'.');
    ext++;

    /* Open MimeTypesPath file */
    fs = fopen(MimeTypesPath, "r");

    /* Scan file for matching file extensions */
    while(fgets(line, BUFSIZ, fs)) {

        //Gets beyond the mimetype part of the line
        token = skip_nonwhitespace(line); 
        token = skip_whitespace(token);

        //Splits the extentions into several tokens
        for (subtoken = strtok(token, " \t"); subtoken; subtoken = strtok(NULL, " \t")) { 
            //Chomps new line character
            subtoken[strcspn(subtoken, "\n")] = 0; 
            //If the extention is the same as the passed in file, set mimetype
            if (streq(ext, subtoken)) {
                mimetype = strdup((strtok(line, " \t")));
            }
        }
    }
    //If mimetype wasn't found, default
    if (mimetype == NULL) {
        mimetype = strdup(DefaultMimeType);
    }

    if (fs) {
        fclose(fs);
    }
    return mimetype;
}

/**
 * Determine actual filesystem path based on RootPath and URI
 *
 * This function uses realpath(3) to generate the realpath of the
 * file requested in the URI.
 *
 * As a security check, if the real path does not begin with the RootPath, then
 * return NULL.
 *
 * Otherwise, return a newly allocated string containing the real path.  This
 * string must later be free'd.
 **/

    char *
determine_request_path(const char *uri)
{
    char path[BUFSIZ];
    char real[BUFSIZ];


    //Concatenates strings
    sprintf(real, "%s%s", RootPath, uri);

    //Converts from "./" to an actual path
    realpath(real, path);
    return strdup(path);
}

/**
 * Determine request type from path
 *
 * Based on the file specified by path, determine what type of request
 * this is:
 *
 *  1. REQUEST_BROWSE: Path is a directory.
 *  2. REQUEST_CGI:    Path is an executable file.
 *  3. REQUEST_FILE:   Path is a readable file.
 *  4. REQUEST_BAD:    Everything else.
 **/
    request_type
determine_request_type(const char *path)
{
    struct stat s;
    //Initially sets type to request bad
    request_type type = REQUEST_BAD;
    if (stat(path, &s) < 0) {
        fprintf(stderr, "Stat on %s failed: %s\n", path, strerror(errno));
        return type;
    }

    //If directory, type is browse
    if(S_ISDIR(s.st_mode)) {
        type = REQUEST_BROWSE;
    }   
    //If not directory. check execute and read with priority to execute
    else if(S_ISREG(s.st_mode)) {
        if(access(path, X_OK) == 0) {
            type = REQUEST_CGI;
        }
        else if(access(path, R_OK) == 0) {
            type = REQUEST_FILE;
        }
    }
    return (type);
}

/**
 * Return static string corresponding to HTTP Status code
 *
 * http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
 **/
    const char *
http_status_string(http_status status)
{
    const char *status_string;

    switch (status) {
        case HTTP_STATUS_OK:
            status_string = "200 OK";
            break;
        case HTTP_STATUS_BAD_REQUEST:
            status_string = "400 Bad Request";
            break;
        case HTTP_STATUS_NOT_FOUND:
            status_string = "404 Not Found";
            break;
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            status_string = "500 Internal Server Error";
            break;
        default:
            status_string = "418 Bui is Funny with his teapots";
            break;
    }
    return status_string;
}

/**
 * Advance string pointer pass all nonwhitespace characters
 **/
    char *
skip_nonwhitespace(char *s)
{
    while(!isspace(*s)) {
        s++;
    }
    return s;
}

/**
 * Advance string pointer pass all whitespace characters
 **/
    char *
skip_whitespace(char *s)
{
    while(isspace(*s)) {
        s++;
    }
    return s;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
