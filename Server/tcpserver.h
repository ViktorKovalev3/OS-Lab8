#ifndef TCPSERVER
#define TCPSERVER
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>           // For O_* constants
#include <sys/stat.h>        // For mode constants
#include <sys/types.h>       // For AF_* PF_* constants

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>


#define NUMBER_OF_WORKERS_THREADS 3
#define NUMBER_OF_SIMULTANEOUS_CLIENTS 100
#define BUFFER_SIZE 64
#define MAX_MSG 10 // look /proc/sys/fs/mqueue/msg_max

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct mq_request_node{
    int  socket;
    char msg[BUFFER_SIZE];
};

struct thread_settings{
    int  id;
    int* end;         //1 is end
};

struct requests_receiver_arg{
    struct thread_settings thread;
    mqd_t* mq_recv;
    int   listen_socket_fd;
};
static void * requests_receiver(void *vp_arg){
    struct requests_receiver_arg *arg = (struct requests_receiver_arg*) vp_arg;
    int tmp_socket = arg->listen_socket_fd;
    struct mq_request_node input_request;
    while (!*(arg->thread.end)){
        bzero(input_request.msg, BUFFER_SIZE);
        input_request.socket = accept(tmp_socket, NULL, NULL);
        if (input_request.socket == -1) handle_error("accept");
        if (read(input_request.socket, input_request.msg, BUFFER_SIZE) == -1)
            handle_error("client read");
        if (mq_send(*(arg->mq_recv), (const char*) &input_request, sizeof(struct mq_request_node), NULL))
            handle_error("mq_send recv");
    }
    pthread_exit((void*) &arg->thread.id);
}

struct requests_handler_arg{
    struct thread_settings thread;
    mqd_t* mq_recv;
    mqd_t* mq_send;
};
static void * requests_handler(void *vp_arg){
    struct requests_handler_arg *arg = (struct requests_handler_arg*) vp_arg;
    struct mq_request_node moved_request;
    char tmp_msg[BUFFER_SIZE];
    while (!*(arg->thread.end)){
        if (mq_receive(*(arg->mq_recv), (char*) &moved_request, sizeof(struct mq_request_node), NULL) == -1)
            handle_error("mq_receive recv");

        bzero(tmp_msg, BUFFER_SIZE);
        sprintf(tmp_msg, "%s", moved_request.msg);

        bzero(moved_request.msg, BUFFER_SIZE);
        sprintf(moved_request.msg, "you say: %s", tmp_msg);
        if (mq_send(*(arg->mq_send), (const char*) &moved_request, sizeof(struct mq_request_node), 0))
            handle_error("mq_send send");
        bzero(moved_request.msg, BUFFER_SIZE);
    }
    pthread_exit((void*) &arg->thread.id);
}

struct requests_sender_arg{
    struct thread_settings thread;
    mqd_t* mq_send;
};
static void * requests_sender(void *vp_arg){
    struct requests_sender_arg *arg = (struct requests_sender_arg *) vp_arg;
    struct mq_request_node output_request;
    while (!*(arg->thread.end)){
        if (mq_receive(*(arg->mq_send), (char*) &output_request, sizeof(struct mq_request_node), NULL) == -1)
            handle_error("mq_receive send");
        if (send(output_request.socket, (void*) output_request.msg, BUFFER_SIZE, MSG_CONFIRM) == -1)
            handle_error("send");
        if(close(output_request.socket)) handle_error("close client socket");
    }
    pthread_exit((void*) &arg->thread.id);
}

struct server_arg{
    char* host;        // Host adress like ddd.ddd.ddd.ddd
    int port;          // Listening port
    int*  finish_work; // Finish flag, 0 - server work, 1 - isn't work
};
static void * server_thread(void *vp_arg){
    struct server_arg *arg = (struct server_arg*) vp_arg;

    /*** Message Queue section ***/
    struct mq_attr mq_attributes;
    // Flags: 0 or O_NONBLOCK
    mq_attributes.mq_flags   = O_NONBLOCK;
    // Max. # of messages on queue
    mq_attributes.mq_maxmsg  = MAX_MSG;
    // Max. message size (bytes)
    mq_attributes.mq_msgsize = sizeof(struct mq_request_node);
    // # of messages currently in queue
    mq_attributes.mq_curmsgs = 0;
    mqd_t mq_send = mq_open("/SendMsgQueue",
                            O_RDWR | O_CREAT,
                            S_IRUSR | S_IWUSR,
                            &mq_attributes
                            );
    if (mq_send == -1) handle_error("mq_open send");
    mqd_t mq_recv = mq_open("/RecvMsgQueue",
                            O_RDWR | O_CREAT,
                            S_IRUSR | S_IWUSR,
                            &mq_attributes
                            );
    if (mq_recv == -1) handle_error("mq_open recv");

    /*** Socket section ***/
    const int listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in in_address;
    inet_aton(arg->host, &in_address.sin_addr);
    in_address.sin_port   = htons(arg->port);
    in_address.sin_family = AF_INET;

    if (bind(listen_socket_fd, (struct sockaddr*) &in_address, sizeof(in_address)))
        handle_error("bind");
    if (listen(listen_socket_fd, NUMBER_OF_WORKERS_THREADS * NUMBER_OF_SIMULTANEOUS_CLIENTS))
        handle_error("listen");

    /*** Threads section ***/
    pthread_t threads[NUMBER_OF_WORKERS_THREADS * 3];\
    struct requests_receiver_arg receiver_args[NUMBER_OF_WORKERS_THREADS];
    struct requests_handler_arg  handler_args [NUMBER_OF_WORKERS_THREADS];
    struct requests_sender_arg   sender_args  [NUMBER_OF_WORKERS_THREADS];

    for (int i = 0; i < NUMBER_OF_WORKERS_THREADS * 3; ++i){
        int thread_number = i / 3;
        if (i % 3 == 0) {
            receiver_args[thread_number].thread.id        = i;
            receiver_args[thread_number].thread.end       = arg->finish_work;
            receiver_args[thread_number].listen_socket_fd = listen_socket_fd;
            receiver_args[thread_number].mq_recv          = &mq_recv;
            if ( pthread_create( &threads[i], NULL, requests_receiver, &receiver_args[thread_number] ) )
                    handle_error("pthread_create receiver");
        } else if (i % 3 == 1){
            handler_args[thread_number].thread.id        = i;
            handler_args[thread_number].thread.end       = arg->finish_work;
            handler_args[thread_number].mq_recv          = &mq_recv;
            handler_args[thread_number].mq_send          = &mq_send;
            if ( pthread_create( &threads[i], NULL, requests_handler, &handler_args[thread_number] ) )
                    handle_error("pthread_create handler");
        } else {
            sender_args[thread_number].thread.id        = i;
            sender_args[thread_number].thread.end       = arg->finish_work;
            sender_args[thread_number].mq_send          = &mq_send;
            if ( pthread_create( &threads[i], NULL, requests_sender, &sender_args[thread_number] ) )
                    handle_error("pthread_create sender");
        }
    }

    /*** Close all threads ***/
    int check_summ = 0; int* tmp_id;
    for (int i = 0; i < NUMBER_OF_WORKERS_THREADS * 3; ++i){
        if ( pthread_join( threads[i], (void**) &tmp_id ) )
            handle_error("pthread_join");
        check_summ += *tmp_id;
    }
    if (check_summ != ((NUMBER_OF_WORKERS_THREADS * 3 - 1) * (NUMBER_OF_WORKERS_THREADS * 3) / 2) ) // Arithmetic progression
        printf("All threads sucessful ended");

    /*** Close socket ***/
    if(close(listen_socket_fd)) handle_error("close server socket");

    /*** Close and clear Message Queue section ***/
    if (mq_close(mq_send)) handle_error("mq_close send");
    if (mq_close(mq_recv)) handle_error("mq_close recv");
    if (mq_unlink("/SendMsgQueue")) handle_error("mq_unlink send");
    if (mq_unlink("/RecvMsgQueue")) handle_error("mq_unlink recv");

    pthread_exit((void*) "Server thread ended");
}

#endif // TCPSERVER

