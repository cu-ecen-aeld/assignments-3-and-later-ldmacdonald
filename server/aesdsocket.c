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
#include "pthread.h"
#include <arpa/inet.h>

// Extra libraries from example, putting here just in case needed
// #include "fcntl.h"
// #include <sys/stat.h>
// #include <netinet/in.h>

// Existing structure from last assignment
#define PORT 9000
#define FILENAME "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 512

// Adding in some global socket fd values for init purposes
int running = 1;
int socket_fd = -1;
int client_fd = -1;
int file_fd = -1;

// Threading init
int num_threads = 0;
pthread_mutex_t file_mutex;
pthread_mutex_t threads_mutex;
pthread_cond_t threads_cond;

typedef struct thread_node {
    pthread_t thread_id;
    int client_fd;
    struct thread_node *next;
} thread_node_t;

thread_node_t *threads = NULL;

//Setting up thread handling functions
void add_thread(pthread_t thread_id, int client_fd){
    thread_node_t *node = (thread_node_t *)malloc(sizeof(thread_node_t));
    node->thread_id = thread_id;
    node->client_fd = client_fd;
    node->next = threads;
    threads = node;

    pthread_mutex_lock(&threads_mutex);
    num_threads++;
    pthread_mutex_unlock(&threads_mutex);
}

void remove_thread(thread_node_t *node){
    // If threads is the node to be removed, change threads to the next node in chain
    if (threads == node){
        threads = node->next;
    } else {
        thread_node_t *current = threads;
        // Traverse node graph via the 'next' reference
        while (current->next != node){
            current = current->next;
        }
        current->next = node->next;
    }
    free(node);
}

// Consolidating Signal Handling to include threading/ mutex support
void signal_handler(int signum __attribute__((unused))) {
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "Signal thrown, exiting.");
        running = 0;

        //Close out any running sockets/ files
        if(socket_fd >= 0) close(socket_fd);
        if(client_fd >= 0) close(client_fd);
        if(file_fd >= 0) close(file_fd);
        unlink(FILENAME);

        while(threads != NULL) {
            pthread_join(threads->thread_id, NULL);
            remove_thread(threads);
        }

        // Closing out the mutexes
        pthread_mutex_destroy(&file_mutex);
        pthread_mutex_destroy(&threads_mutex);
        pthread_cond_destroy(&threads_cond);

        // Closing Syslog
        closelog();

        exit(0);
    }
}

void *append_timestamp(){
    while (running) {
        time_t now = time(NULL);
        struct tm *tm = gmtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "timestamp:%Y-Tm-%d %H:%M:%S\n", tm);

        pthread_mutex_lock(&file_mutex);
        if(write(file_fd, timestamp, strlen(timestamp)) < 0){
            perror("write timestamp");
        } else {
            syslog (LOG_INFO, "Appended timestamp: %s", timestamp);
        }
        pthread_mutex_unlock(&file_mutex);

        usleep(10000000);
    }

    return NULL;
}

void *handle_connection(void *arg){
    char buffer[BUFFER_SIZE];
    thread_node_t *node = (thread_node_t *)arg;
    int client_fd = node-> client_fd;

    while (1){
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if(bytes_received < 0) {
            perror("recv");
            pthread_exit((void *)1);
        } else if (bytes_received == 0) {
            break;
        }

        pthread_mutex_lock(&file_mutex);
        if(write(file_fd, buffer, bytes_received) <0 ) {
            perror("write");
            pthread_mutex_unlock(&file_mutex);
            pthread_exit((void *)1);
        }
        pthread_mutex_unlock(&file_mutex);

        if(strchr(buffer, '\n') != NULL) {
            lseek(file_fd, 0, SEEK_SET);
            while((bytes_received = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                send(client_fd, buffer, bytes_received, 0);
            }
        }
    }

    close(client_fd);
    client_fd = -1;
    free(arg);
    syslog(LOG_INFO, "Closed socket connection %d", client_fd);

    return NULL;
}

int main(int argc,  char** argv){

    // Syslog
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_INFO, "Starting program\n");

    if (signal(SIGINT, signal_handler) == SIG_ERR){
        syslog(LOG_ERR, "Failed to set SIGINT handling.");
        return 1;
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR){
        syslog(LOG_ERR, "Failed to set SIGTERM handling.");
        return 1;
    }

    // Check for -d flag, changing to set daemon_mode flag for later
    int daemon_mode = 0;
    if((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'd')){
        daemon_mode = 1;
    }


    // Set up socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Rewriting socket based on example, original throws big blocks of red in IDE
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(socket_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0){
        perror("bind");
        close(socket_fd);
        return 1;
    }
    syslog(LOG_INFO, "Bind successful w/ file descriptor %d\n", socket_fd);

    // Daemonize 
    if(daemon_mode){
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed");
            return 1;
        }
        if (pid > 0){
            return 0;
        }
        if (setsid() < 0) {
            perror("setsid");
            return 1;
        }

        // More understandable version of IO closes that I did in last version. 
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_RDWR);
    }

    // Set up listener
    if (listen(socket_fd, 1) != 0){
        perror("listen");
        close(socket_fd);
        return 1;
    }
    syslog(LOG_INFO, "Listening to connections on %d\n", socket_fd);

    // Opening file
    file_fd = open(FILENAME, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (file_fd < 0) {
        perror("open");
        close(socket_fd);
        return 1;
    }

    // Timestamp thread
    pthread_t thread_id;
    thread_node_t *node = (thread_node_t *)malloc(sizeof(thread_node_t));
    node->client_fd = client_fd;
    if(pthread_create(&thread_id, NULL, append_timestamp, (void*)node) < 0) {
        perror("timing pthread create");
        close(client_fd);
    } else {
        add_thread(thread_id, client_fd);
    }

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &addr_len);
        if(client_fd < 0) {
            perror("accept error");
            continue;
        }

        char *client_ip = inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO, "Accepted connection from IP %s.", client_ip);

        // Handling connection in new thread
        pthread_t thread_id;
        thread_node_t *node = (thread_node_t *)malloc(sizeof(thread_node_t));
        node->client_fd = client_fd;
        if(pthread_create(&thread_id, NULL, handle_connection, (void *)node) < 0){
            perror("connection error");
            close(client_fd);
        } else {
            add_thread(thread_id, client_fd);
        }
    }

    // Thread joining
    while (threads != NULL) {
        pthread_join(threads->thread_id, NULL);
        remove_thread(threads);
    }

    // Cleanup
    close(socket_fd);
    close(file_fd);
    unlink(FILENAME);
    closelog();

    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&threads_mutex);
    pthread_cond_destroy(&threads_cond);

    return 0;
}