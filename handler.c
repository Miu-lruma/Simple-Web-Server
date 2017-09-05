/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>

/* Internal Declarations */
http_status handle_browse_request(struct request *request);
http_status handle_file_request(struct request *request);
http_status handle_cgi_request(struct request *request);
http_status handle_error(struct request *request, http_status status);

/**
 * Handle HTTP Request
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
    http_status
handle_request(struct request *r)
{
    http_status result;

    /* Parse request */
    int pr = parse_request(r);
    if(pr < 0) {
        //debug("parse request \n");
    }

    /* Determine request path */
    r->path = determine_request_path(r->uri);
    request_type type = determine_request_type(r->path);
    //debug("HTTP REQUEST PATH: %s", r->path);
    //debug("HTTP Request TYPE: %d", type);

    /* Dispatch to appropriate request handler type */
    if(type == REQUEST_BROWSE && (result = handle_browse_request(r)) != HTTP_STATUS_OK) { 
        result = handle_error(r, result);
    }
    else if(type == REQUEST_FILE && (result = handle_file_request(r)) != HTTP_STATUS_OK) {
        result = handle_error(r, result);
    }
    else if(type == REQUEST_CGI && (result = handle_cgi_request(r)) != HTTP_STATUS_OK) {
    }
    else if(type == REQUEST_BAD){
        result = HTTP_STATUS_NOT_FOUND;
        result = handle_error(r, result);
        //debug("bad request\n");
    }


    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    return result;
}

/**
 * Handle browse request
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
    http_status
handle_browse_request(struct request *r)
{
    struct dirent **entries;
    int n;

    /* Open a directory for reading or scanning */
    DIR *d = opendir(r->path);
    if(d==NULL) {
        perror("opendir");
        return HTTP_STATUS_NOT_FOUND;
    }

    n = scandir( r->path, &entries, NULL, alphasort);
    if (n<0) {
        fprintf(stderr, "Unable to scandir on %s: %s\n", r->path, strerror(errno));
        closedir(d);
        return HTTP_STATUS_NOT_FOUND;
    }

    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\n");
    fprintf(r->file, "Content-Type: text/html\n");
    fprintf(r->file, "\r\n");

    /* For each entry in directory, emit HTML list item */
    fprintf(r->file, "<ul>");
    for(int it = 1; it < n; it++) {
        char *slash = "/";

        fprintf(r->file, "<li><a href=%s%s%s>%s</a></li>", r->uri, entries[it]->d_name, slash, entries[it]->d_name);
    }
    fprintf(r->file, "</ul>");

    /* Flush socket, return OK */
    fflush(r->file);
    closedir(d);
    free(entries);
    return HTTP_STATUS_OK;
}

/**
 * Handle file request
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
    http_status
handle_file_request(struct request *r)
{
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    printf("enters file handler\n");
    /* Open file for reading */
    if( (fs = fopen(r->path, "r"))  == NULL) {
        fprintf(stderr, "fopen failed: %s\n", strerror(errno));
        close(r->fd);
        return HTTP_STATUS_NOT_FOUND;
    }

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\n");
    fprintf(r->file, "Content-Type: text/html\n");
    fprintf(r->file, "\r\n");
    /* Read from file and write to socket in chunks */
    if(streq(mimetype, "text/plain")) fprintf(r->file, "<pre>");
    while((nread = fread(buffer,1, BUFSIZ-1, fs)) != 0){
        fwrite(buffer, nread, 1, r->file);
    } 
    if(streq(mimetype, "text/plain")) fprintf(r->file, "</pre>");
    /* Close file, flush socket, deallocate mimetype, return OK */
    fflush(r->file);
    close(r->fd);
    free(mimetype);
    return HTTP_STATUS_OK;
}

/**
 * Handle file request
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
    http_status
handle_cgi_request(struct request *r)
{
    FILE *pfs;
    char buffer[BUFSIZ];

    /* Export CGI environment variables from request:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    setenv("DOCUMENT_ROOT", RootPath, 1);
    setenv("QUERY_STRING", r->query, 1);
    setenv("REMOTE_ADDR", r->port, 1);
    setenv("REQUEST_METHOD", r->method, 1);
    setenv("REQUEST_URI", r->path, 1);
    setenv("SCRIPT_FILENAME", (strrchr(r->path, '/')+1), 1);
    setenv("SERVER_PORT", Port, 1);
    struct header *curr = r->headers;
    while(curr != NULL){
        if( streq(curr->name, "Host") != 0) {
            setenv("HTTP_HOST", curr->value, 1);
        }
        else if( streq(curr->name, "Accept") != 0){
            setenv("HTTP_ACCEPT", curr->value, 1);
        }
        else if( streq(curr->name, "Accept-Language") != 0) {
            setenv("HTTP_ACCEPT_LANGUAGE", curr->value, 1);
        }
        else if( streq(curr->name, "Accept-Encoding") != 0) {
            setenv("HTTP_ACCEPT_ENCODING", curr->value, 1);
        }
        else if( streq(curr->name, "Connection") != 0) {
            setenv("HTTP_CONNECTION", curr->value, 1);
        }
        else if( streq(curr->name, "User-Agent") != 0) {
            setenv("HTTP_USER_AGENT", curr->value, 1);
        }
        curr = curr->next;
    }

    /* Export CGI environment variables from request headers */

    /* POpen CGI Script */
    if( (pfs = popen(r->path, "r")) == NULL) {
        fprintf(stderr, "Could Not popen: %s\n", strerror(errno));
        pclose(pfs);
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }


    /* Copy data from popen to socket */
    while( fgets(buffer, BUFSIZ, pfs) != NULL ) {
        //debug( "fgets failed");


        printf("%s\n", buffer);
        printf("%s", buffer);
        if( fputs(buffer, r->file) != EOF ) {
            //debug( "fputs failed");
        }
    }

    /* Close popen, flush socket, return OK */
    fflush(r->file);
    pclose(pfs);
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
    http_status
handle_error(struct request *r, http_status status)
{
    const char *status_string = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->file, "HTTP/1.0 %s\n", status_string);
    fprintf(r->file, "Content-Type: text/html\n");
    fprintf(r->file, "\r\n");

    /* Write HTML Description of Error*/
    fprintf(r->file, "<h1>");
    fprintf(r->file, "%s\n", status_string);
    fprintf(r->file, "</h1>");
    fprintf(r->file, "<h2>");
    fprintf(r->file, "Stuffs whack, G.");
    fprintf(r->file, "</h2>");
    fprintf(r->file, "<center><img src=\"https://s-media-cache-ak0.pinimg.com/originals/52/bb/50/52bb5089b673a7f85fed7c7bac2fc53c.jpg\"></center>");
    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
