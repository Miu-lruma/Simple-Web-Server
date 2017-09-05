/* forking.c: Forking HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

/**
 * Fork incoming HTTP requests to handle the concurrently.
 *
 * The parent should accept a request and then fork off and let the child
 * handle the request.
 **/
    void
forking_server(int sfd)
{
    struct request *request;
    pid_t pid;

    /* Accept and handle HTTP request */
    while (true) {
        /* Accept request */
        request = accept_request(sfd);

        /* Fork off child process to handle request */
        signal(SIGINT, SIG_IGN);
        pid = fork();
        if(pid < 0) {
            //debug("fork failed %s", strerror(errno));
            free_request(request);
            continue;
        }

        /* Ignore children */
        if(pid == 0){
            http_status hstat;
            if( (hstat = handle_request(request)) != HTTP_STATUS_OK ) {
                //debug("error occurred");
            }
            free_request(request);
            exit(EXIT_SUCCESS);
        }
        else {
            free_request(request);
        }

    }

    /* Close server socket and exit*/
    //free_request(request);
    close(sfd);
    return;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
