/* request.c: HTTP Request Functions */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

#define WHITESPACE      " \t\n"

int parse_request_method(struct request *r);
int parse_request_headers(struct request *r);

/**
 * Accept request from server socket.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
struct request * accept_request(int sfd)
{
    struct request *r;
    struct sockaddr raddr;
    socklen_t rlen;

    /* Allocate request struct (zeroed) */
    r = (struct request *)calloc(1, sizeof(struct request));
    if (r==NULL) exit (1);

    /* Accept a client */
    rlen = sizeof(struct sockaddr);
    int client_fd = accept(sfd, &raddr, &rlen);
    if (client_fd < 0) {
        fprintf(stderr, "Unable to accept: %s\n", strerror(errno));
        free_request(r);
        return NULL;
    }

    /* Lookup client information */

    if(getnameinfo(&raddr, rlen, r->host, sizeof(r->host), r->port, sizeof(r->port), NI_NUMERICHOST | NI_NUMERICSERV ) != 0){
        fprintf(stderr, "Unable to getnameinfo: %s\n", strerror(errno));
        free_request(r);
        return NULL;
    }



    /* Open socket stream */
    FILE *client_file = fdopen(client_fd, "r+");
    if(client_file == NULL) {
        fprintf(stderr, "Unable to fdopen: %s\n", strerror(errno));
        close(client_fd);
    }

    r->fd = client_fd;
    r->file = client_file;

    log("Accepted request from %s:%s", r->host, r->port);
    return r;
}

/**
 * Deallocate request struct.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/
    void
free_request(struct request *r)
{
    struct header *header;

    if (r == NULL) {
        return;
    }

    /* Close socket or fd */
    if (r->file)
        fclose(r->file);
    else if (r->fd >= 0)
        close(r->fd);

    /* Free allocated strings */
    free(r->method);
    free(r->uri);
    free(r->path);
    free(r->query);

    /* Free headers */
    header = r->headers;
    struct header *newHeader;
    while (header) {
        free(header->name);
        free(header->value);
        newHeader = header;
        header = header->next;
        free(newHeader);
    }


    /* Free request */
    free(r);
}
/**
 * Parse HTTP Request.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
    int
parse_request(struct request *r)
{
    /* Parse HTTP Request Method */
    int rMethod = parse_request_method(r);
    if(rMethod < 0){
        fprintf(stderr, "failed to parse request method\n");
        return -1;
    }

    /* Parse HTTP Requet Headers*/
    int rHeaders = parse_request_headers(r);
    if(rHeaders < 0){
        fprintf(stderr, "failed to parse request headers\n");
        return -1;
    }
    return 0;
}

/**
 * Parse HTTP Request Method and URI
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
    int
parse_request_method(struct request *r)
{
    char buffer[BUFSIZ];
    /* Read line from socket */
    if( fgets(buffer, BUFSIZ, r->file) == NULL) {
        //debug("fgets failed%s\n", strerror(errno));
        return -1;
    }

    /* Parse method and uri */
    char * method    = strtok(skip_whitespace(buffer), WHITESPACE);
    char * uri       = strtok(NULL, WHITESPACE);

    /* Parse query from uri */
    char *myquery = strchr(uri, '?');
    char *query;
    if(myquery != NULL){
        *myquery++ = '\0';
        query    = strdup(myquery);
    }

    r->method   = strdup(method);
    r->uri      = strdup(uri);
    if(myquery != NULL) {
        r->query    = strdup(query);
    }
    else{
        r->query    = strdup("");
    }
    /* Record method, uri, and query in request struct */

    //debug("HTTP METHOD: %s", r->method);
    //debug("HTTP URI:    %s", r->uri);
    //debug("HTTP QUERY:  %s", r->query);

    return 0;

    //fail:
    //    return -1;
}

/**
 * Parse HTTP Request Headers
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <VALUE>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, value = buffer.split(':')
 *      header      = new Header(name, value)
 *      headers.append(header)
 **/
    int
       parse_request_headers(struct request *r)
{
    struct header *curr = NULL;
    char buffer[BUFSIZ];
    char *name;
    char *value;
    curr = r->headers;
    /* Parse headers from socket */
    while(fgets(buffer, BUFSIZ, r->file) && strlen(buffer) > 2){
        name = strdup(buffer);
        while(name[strlen(name)-1] != ':') chomp(name);
        chomp(name);
        value = strdup(strchr(buffer, ':'));
        value++;
        value = skip_whitespace(value);
        struct header *newHead = (struct header *) calloc(1, sizeof(struct header));
        if(!curr){
            curr = newHead;
        } else {
            curr->next = newHead;
        }
        newHead->name = name;
        newHead->value = value;
        curr = newHead;

    }

#ifndef NDEBUG
    for (struct header *header = r->headers; header != NULL; header = header->next) {
        //debug("HTTP HEADER %s = %s", header->name, header->value);
    }
#endif
    return 0;

}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
