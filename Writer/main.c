#include "tcpserver.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)


int main(int argc, char** argv){
    printf("Server\n\n");
    pthread_t* server; struct server_arg server_config;
    int end_of_server_work = 0;
    server_config.finish_work = &end_of_server_work;
    server_config.host = "127.0.0.1";
    server_config.port = 1984;
    if ( pthread_create( &server, NULL, server_thread, &server_config ) )
        handle_error("pthread_create");

    getchar();

    //Exit section
    char* exit_server_code;
    if ( pthread_join( server, (void**) &exit_server_code ) )
            return 1;
    printf("\n%s\n", exit_server_code);
    return EXIT_SUCCESS;
}
