#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>

#define PORT "9000"
#define FILENAME "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 512

void handle_sigint(int sig){
    remove(FILENAME);
    exit(EXIT_SUCCESS);
}

void handle_sigterm(int sig){
    remove(FILENAME);
    exit(EXIT_SUCCESS);
}

int main(int argc,  char** argv){

    // Syslog
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_INFO, "Starting program\n");

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigterm);

    // Check for -d flag
    if((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'd')){
        // create new process
        int pid = fork();

        // Check for failure
        if (pid < 0){
            exit(EXIT_FAILURE);
        }
        // Check for success
        if (pid > 0){
            exit(EXIT_SUCCESS);
        }

        // Create new session to remove tty
        if(setsid() < 0) exit(EXIT_FAILURE);


        // change wd to prevent unmount error
        chdir("/");

        // Redirect the stdin, stdout, and stderr to /dev/null to prevent console communication
        close(0);close(1);close(2);
        open("/dev/null",O_RDWR);dup(0);dup(0);
    }

    // Cleanup old file if present
    //if (remove(FILENAME) != 0){
    //    syslog(LOG_INFO, "Delete unsuccessful");
    ///    perror("Failure: ");
    //}

    // Bytes and char array to be sent back to the client
    int32_t len = 0;
    char sendBuffer[BUFFER_SIZE] ={0};

    // Set up socket
    int socket_fd = socket(PF_INET, SOCK_STREAM, 0);

    int reuse = 1;
    setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    struct addrinfo *address;

    // Set up getaddrinfo() struct
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(NULL, PORT, &hints, &address);

    // Check for success
    if (status != 0 || address == NULL)
    {
        return -1;
    }

    // Bind to sockett
    bind(socket_fd, address->ai_addr, sizeof(struct sockaddr));
    syslog(LOG_INFO, "Bind successful w/ file descriptor %d\n", socket_fd);

    // Set up listener
    struct sockaddr_storage client_addr;
    listen(socket_fd, 10);
    syslog(LOG_INFO, "Listening to connections on %d\n", socket_fd);

    socklen_t addr_size = sizeof(client_addr);

    // Accept connection
    int accept_fd;
    while ((accept_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size))){
        struct sockaddr_in *sin = (struct sockaddr_in *)&client_addr;
        unsigned char *client_ip = (unsigned char *)&sin->sin_addr.s_addr;
        unsigned short int client_port = sin->sin_port;

        // If successful log client IP and port
        if (accept_fd != -1){
            syslog(LOG_INFO, "Accepted connection from %d.%d.%d.%d:%d\n", client_ip[0], client_ip[1], client_ip[2], client_ip[3], client_port);
        }

        // Setup data file w/ append
        FILE *file = NULL;
        file = fopen(FILENAME, "a");

        if (file ==  NULL) {
            syslog(LOG_ERR, "Was unable to open the file %s, errno: %d", FILENAME, errno);
            return -1;
        }

        int bytes_rcv;

        // Receive data from client port
        char buffer[20000] = {0};
        while((bytes_rcv = recv(accept_fd, buffer, 20000, 0)) > 0){
            if (bytes_rcv == 0){
                // Nothing received, connection closed by client, break loop
                break;
            }
            if (bytes_rcv == -1){
                // Something went wrong with connection, NOT connected anymore
                syslog(LOG_ERR, "Receive failure errno: %d\n", errno);
                break;
            }

            // store the received packet in the data file
            len += bytes_rcv;
            fprintf(file, "%s", buffer);

            // Add to the send buffer
            strcat(sendBuffer, buffer);
            // Send the 'sendBuffer' as acknowledgment
            //fread(sendBuffer, sizeof(sendBuffer), 100000, file);
            int bytes_sent = send(accept_fd, sendBuffer, len, 0);

            if(bytes_sent == -1){
                // Unable to send, capture errno
                syslog(LOG_ERR, "Send Error,  errno: %d\n", errno);
                break;
            }
        }
        fclose(file);
        syslog(LOG_INFO, "Closed connnection from %d.%d.%d.%d:%d\n",client_ip[0], client_ip[1], client_ip[2], client_ip[3], client_port);
    }
    // Cleanup
    free(address);
    close(accept_fd);
    close(socket_fd);
    //remove(FILENAME);
    closelog();

    return 0;
}