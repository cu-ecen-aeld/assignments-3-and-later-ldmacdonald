#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <signal.h>


static volatile bool SigTerm = false;
static volatile bool SigInt =  false;

#define PORT 9000
#define BUFFER_SIZE 4096
#define MAX_TEMPBUFF_SIZE 16384 // Temp buffer 4x base buffer size
#define TEMPFILE_COPY_SIZE 4096

static char const *const filename = "/var/tmp/aesdsocketdata";

typedef struct {
    char *filename;
    int fd;
    off_t pos;
    char *data;
} datafile_context_t;

// Set up file and return true if file start position greater than or equal to 0 and data exists
static int datafile_init(datafile_context_t *const context, char const *const filename) {
    context->filename = (char*)filename;
    // Open file and assign return code to fd
    context->fd = open(context->filename, O_RDWR | O_CREAT | O_APPEND, 0666);
    //printf("File open with fd: %d \n", context->fd);
    // check if file opened
    if(context->fd <= 0) return -1;
    // Make sure file loaded
    lseek(context->fd, 0, SEEK_END);
    context->pos = lseek(context->fd, 0, SEEK_CUR);
    
    // initialize data to defined buffer size
    context->data = malloc(BUFFER_SIZE);
    return (context->pos >= 0) && (context->data != NULL);
}

// Close out fd and release the memory allocated for data
static void datafile_close(datafile_context_t *const context){
    if(context->fd > 0) close(context->fd);
    if(context->data) free(context->data);
}

// Close and remove filename from context
static void datafile_kill(datafile_context_t  *const context){
    datafile_close(context);
    remove(context->filename);
}

// Accept and log client IP
static inline int  accept_client(int socket){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    (void)memset(&addr, 0, sizeof(addr));
    const int res = accept(socket, (struct sockaddr*)&addr, &len);
    if(res >= 0) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);
    }

    return res;
}

static int client_send(int clientfd, datafile_context_t *const context){
    int sent = 0;
    // Set to start of  file
    lseek(context->fd, 0, SEEK_SET);
    do {
        const int data_size = read(context->fd, context->data, BUFFER_SIZE);
        if(data_size < 0){
            const int en = errno;
            if(en == EINTR) return -1;
            return 0;
        }
        if(data_size  == 0) return sent;
        int count = 0;
        //printf("Sending data: %s\n", context->data);
        while(count < data_size) {
            const int sent_size = send(clientfd, context->data + count, data_size, 0);
            if(sent_size < 0) {
                const int en = errno;
                if(en == EINTR)  return -1;
                if((en == EAGAIN) || (errno == EWOULDBLOCK)) continue;
                return sent;
            }

            count += sent_size;
            sent +=  sent_size;
        }
    } while(true);
}

static int write_packet(int fd, char *data, int size){
    int  written = 0;
    while(written < size) {
        // Not sure what 'written' does here
        //printf("write_packet received: %s", data);
        int res = write(fd, data, size);
        //printf("res =  %i", res);
        if(res < 0){
            const int en = errno;
            if(en == EINTR) return -1;
            if((en == EAGAIN) || (errno == EWOULDBLOCK)) continue;
            return 0;
        }
        written += res;
    }

    return written;
}

static int recv_client(int clientfd, datafile_context_t *const context) {
    int received = 0;
    // Either not resizing or not saving the received data
    // Looks like I'm losing data here
    do {
        const int data_size = recv(clientfd, context->data, BUFFER_SIZE, 0);
        //printf("socket received: %s", context->data);
        if(data_size < 0){
            const int en = errno;
            if(en == EINTR) return -1;
            if((en == EAGAIN) || (errno == EWOULDBLOCK)) continue;
            return received;
        }


        if(data_size == 0) return received;
        char *pointer = context->data;
        received += data_size;
        char *begin = pointer;
        
       // Use string function to check for \n, split write first part
       // If not just write packet and update pointer
       //printf("at write\n");
       //printf("filename: %s\n", context->filename);
       //printf("writing: %s\n", context->data);
       //char *token;
       //token = strtok(begin, "\n");
       //while (token != NULL){
       received = write_packet(context->fd, begin, data_size);
          //token = strtok(NULL, "\n");
          
       //}

       return received;
       
    } while(true);
}

static void signal_handler(int sig) {
    if(sig == SIGINT) SigInt = true;
    if(sig == SIGTERM) SigTerm = true;
}

int main (int argc, char *argv[]){

    bool daemonize = false;

    // Iterate over the arguments and set daemonize if -d is encountered
    for (int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "-d") == 0){
            daemonize = true;
        }
    }

    // Clean up old file
    remove(filename);

    // Set up syslog
    openlog(NULL, LOG_PERROR, LOG_USER);

    ///  Setup daemonize fork
    if(daemonize){
        const int res = fork();
        if(res < 0){
            const int en = errno;
            syslog(LOG_ERR, "Cannot fork (errno %d), exiting", en);
            exit(EXIT_FAILURE);
        }
        if(res > 0){
            syslog(LOG_INFO,  "Daemonize successful");
            // Close out parent
            exit(EXIT_SUCCESS);
        }
    }

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1; // reuse
 

    // Creating socket file descriptor
    if (server_fd < 0){
        const int en = errno;
        syslog(LOG_ERR, "Cannot set socket options (errno %d), exiting", en);
        return -1;
    }

    // Forcefully attaching socket to the PORT value
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,  &opt, sizeof(opt))) {
        const int en = errno;
        syslog(LOG_ERR, "Cannot set socket options (errno %d), exiting", en);
        close(server_fd);
        return -1;
    }


    // Setting some values to be used wih bind
    struct sockaddr_in address;
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    // Binding socket to the PORT value
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0){
        const  int en = errno;
        syslog(LOG_ERR, "Cannot bind socket (errno %d), exiting", en);
        close(server_fd);
        return -1;
    }
    
    // Set up signals SIGINT/ SIGTERM
    struct sigaction signals;

    bzero(&signals, sizeof(signals));
    signals.sa_handler =  &signal_handler;



    


    if (listen(server_fd, 30) != 0){
        const int en = errno;
        syslog(LOG_ERR, "Cannot listen socket (errno %d), exiting", en);
        close(server_fd);
        return -1;
    }

    datafile_context_t log_context;
    if(!datafile_init(&log_context, filename)){
        const int en = errno;
        printf("datatfile failed");
        syslog(LOG_ERR, "Cannot instantiate data file (errno %d), exiting", en);
        close(server_fd);
        return -1;
    }
    

    // Set up a loop to listen and accept packets
    while(true) {

        if ((sigaction(SIGTERM, &signals, NULL) == -1) || (sigaction(SIGINT, &signals, NULL) == -1)){
            datafile_kill(&log_context);
        }

        const int new_socket = accept_client(server_fd);
        if (new_socket < 0){
            const int en = errno;
            if(en == EINTR) break;
            syslog(LOG_ERR, "Cannot accept connection (errno %d), exiting", en);
            close(server_fd);
            datafile_close(&log_context);
            return -1;
        }

        // wrapped recv
        if(recv_client(new_socket,  &log_context) > 0){
            //printf("client send\n");
            client_send(new_socket, &log_context);
            close(new_socket);
            
        }
        //printf("SigInt: %d, SigTerm: %d\n", SigInt, SigTerm);
        if(SigInt || SigTerm){
            //printf("Hit SigInt/ SigTerm");
            close(server_fd);
            close(new_socket);;
            //datafile_kill(&log_context);
            break;
        }

    }

    close(server_fd);
    closelog();
    return 0;
}