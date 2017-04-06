#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 64

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct simple_tcp_client_arg{
    char* host;
    int port;
    int* finish_work;
};
static void * simple_tcp_client(void *vp_arg){
    struct simple_tcp_client_arg *arg = (struct requests_sender_arg *) vp_arg;

    //Connection section
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    inet_aton("127.0.0.1", &address);
    address.sin_port   = htons(48655);
    address.sin_family = AF_INET;

    if (connect(sockfd, (struct sockaddr*) &address, sizeof(address)))
        handle_error("connect");

    //Working section
    srand(time(NULL));
    char out_msg[BUFFER_SIZE];
    char in_msg [BUFFER_SIZE];
    while (!*(arg->finish_work)){
        //send
        sprintf(out_msg, "%d", rand());
        printf("Num is %s\n", out_msg);
        if(send(sockfd, (void*) &out_msg, BUFFER_SIZE + 1, MSG_CONFIRM) == -1)
            handle_error("send");
        //recv
        if(recv(sockfd, (void*) &in_msg, BUFFER_SIZE + 1, MSG_WAITALL) == -1)
            handle_error("recv");
        printf("%s\n", in_msg);
        fflush(stdout);
        sleep(1);
    }
    pthread_exit((void*) "Client thread ended");
}

int main(void)
{
    printf("Client\n\n");
    pthread_t* client_th; struct simple_tcp_client_arg client_config;
    int end_of_client_work = 0;
    client_config.finish_work = &end_of_client_work;
    client_config.host = "127.0.0.1";
    client_config.port = 48655;
    if ( pthread_create( &client_th, NULL, simple_tcp_client, &client_config ) )
        handle_error("pthread_create");

    getchar();
    end_of_client_work = 1;
    //Exit section
    char* exit_client_code;
    if ( pthread_join( client_th, (void**) &exit_client_code ) )
        return 1;
    printf("\n%s\n", exit_client_code);

    return EXIT_SUCCESS;
}
