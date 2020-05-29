#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#define MY_PORT_NUMBER              49999
#define BUF_SIZE                      100

// Function use for responding to client with A/E with msg
void respond(int socket, char res, char* msg) {
    char respond[256] = {0};

    respond[0] = res;
    strcpy(&respond[1], msg);

    write(socket, respond, strlen(respond));
    write(socket, "\n", 1);

    return;
}

// Function will read in response from client and return response as string
char* clientRes(int socket) {
    char response[256] = {0};
    int i = 0;

    while(1) {
        if(read(socket, &response[i], 1) < 1) {
            perror("Unable to read response from server");
            return NULL;
        }

        if(response[i] == '\n') {
            response[i] = '\0';
            break;
        }

        i++;
    }
    return strdup(response);
}

// Function used to open a second connection bewteen client/server for sending data
int dataServe(int connectfd) {
    // Declare structures needed to init connection
    int dataListen = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dataServAddr;
    int datafd;
    int dLength = sizeof(struct sockaddr_in);
    struct sockaddr_in dataClientAddr;
    int numberRead;
    char buf[BUF_SIZE] = {0};

    // Assign values for server structures
    memset(&dataServAddr, 0, sizeof(dataServAddr));
    dataServAddr.sin_family = AF_INET;
    dataServAddr.sin_port = htons(0);
    dataServAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket to port number
    if(bind(dataListen, (struct sockaddr *) &dataServAddr, sizeof(dataServAddr)) < 0) {
        perror("Could not bind");
        exit(1);
    }

    memset(&dataServAddr, 0, sizeof(dataServAddr));

    // Obtain next avail socket
    socklen_t len = sizeof(dataServAddr);
    getsockname(dataListen, (struct sockaddr *) &dataServAddr, &len);

    // Create queue for which server can listen to
    if(listen(dataListen, 1) == -1){
        perror("Could not begin listening for data connection");
        exit(1);
    }
    char resBuf[BUF_SIZE] = {0};
    sprintf(resBuf, "%i", ntohs(dataServAddr.sin_port));
    respond(connectfd, 'A', resBuf);

    // Wait for client to connect
    datafd = accept(dataListen, (struct sockaddr *) &dataClientAddr, (socklen_t *) &dLength);

    // Return file descriptor used to transfer data
    return datafd;
}

int main(int argc, char const *argv[]) {
    // Init data types and structures needed for server/client connection
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servAddr;
    int connectfd;
    int length = sizeof(struct sockaddr_in);
    struct sockaddr_in clientAddr;
    int numberRead;
    char buf[BUF_SIZE] = {0};

    // Init server attributes
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket to port number
    if(bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("Could not bind");
        exit(1);
    }

    // Create queue for which server can listen to
    if(listen(listenfd, 4) == -1){
        perror("Could not begin listening for clients");
        exit(1);
    }

    // While loop to continue listening for new clients
    while(1) {
        // Wait until a client has successfully connected
        connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, (socklen_t *) &length);
        // Exit if error with connection
        if(connectfd == -1)
            perror("Error connecting to client");
        else {
            int pid = fork();
            if(pid < 0) {
                perror("Failed to fork process");
                exit(1);
            }
            // Parent will wait for child to exit to prevent zombie process
            if(pid) {
                // Not working, zombies staying alive ---------------------------------------------------------
                waitpid(-1, NULL, WNOHANG);
            }
            // Child will take care of new connection
            else {
                int numberRead;
                char *res;

                printf("Now connected with %s\n", inet_ntoa(clientAddr.sin_addr));

                // Continue communication until client closes or requests to exit
                while((res = clientRes(connectfd)) != NULL) {
                    // Client requests to exit
                    if(res[0] == 'Q'){
                        respond(connectfd, 'A', "");
                        printf("Client %s in exiting normally\n", inet_ntoa(clientAddr.sin_addr));
                        break;
                    }
                    // CLient requests server to change directory
                    if(res[0] == 'C'){
                        int error = 0;
                        res[numberRead] = '\0';
                        char path[BUF_SIZE];
                        strcpy(path, &res[1]);

                        char fullPath[256] = {0};
                        char cwd[256] = {0};
                        getcwd(cwd, sizeof(cwd));
                        printf("Old current working dir: %s\n", cwd);
                        // Concat relative path to cwd and change to that directory
                        if(path[0] != '/') {
                            strcpy(fullPath, cwd);
                            strcat(fullPath, "/");
                            strcat(fullPath, path);
                            if(chdir(fullPath) == -1) {
                                fprintf(stderr, "Could not change cwd to '%s':\n%s\n", path, strerror(errno));
                                respond(connectfd, 'E', strerror(errno));
                                error = 1;
                            }
                        }
                        // If absolute path is given, change to that dir
                        else{
                            if(chdir(path) == -1) {
                                fprintf(stderr, "Could not change cwd to '%s':\n%s\n", path, strerror(errno));
                                respond(connectfd, 'E', strerror(errno));
                                error = 1;
                            }
                        }
                        // Print new cwd to server stdout
                        getcwd(cwd, sizeof(cwd));
                        printf("New current working dir: %s\n", cwd);
                        if(error == 0)
                            respond(connectfd, 'A', cwd);
                    }
                    // Client requests data transfer
                    if(res[0] == 'D') {
                        // Make data connection between client/server
                        int datafd = dataServe(connectfd);

                        if(datafd){
                            // Determine what type of file transfer client requests
                            char *res = clientRes(connectfd);
                            // Client requests a server side ls
                            if(res[0] == 'L') {
                                respond(connectfd, 'A', "");
                                // Fork to exec ls command
                                int pid = fork();
                                if(pid) {
                                    waitpid(pid, NULL, 0);
                                }
                                else {
                                    // Close stdout for forked process and send exec output to client data file desc
                                    close(1);
                                    dup(datafd);
                                    execlp("/bin/ls", "ls", "-l", (char*)0);
                                    fprintf(stderr, "Program didn't run correctly.\nError: %s\n", strerror(errno));
                                    exit(1);
                                }
                            }
                            // Client requests contents of server side file to be sent
                            if(res[0] == 'G') {
                                // Make sure file exist and is a regular file
                                struct stat fs;
                                if(stat(&res[1], &fs) == -1){
                                    fprintf(stderr, "Could not send file '%s':\n%s\n", &res[1], strerror(errno));
                                    respond(connectfd, 'E', strerror(errno));
                                }
                                else {
                                    if((fs.st_mode & S_IFMT) != S_IFREG) {
                                        perror("Path given was not for a file");
                                        respond(connectfd, 'E', "Path given was not for a file");
                                    }
                                    else{
                                        // Make sure process can open file
                                        int newFd = open(&res[1], O_RDONLY);

                                        if(newFd == -1) {
                                            perror("Cannot open file");
                                            respond(connectfd, 'E', strerror(errno));
                                        }
                                        else {
                                            // Begin reading file and write contents to client
                                            respond(connectfd, 'A', "");
                                            printf("Sending contents of '%s' to client %s\n", &res[1], inet_ntoa(clientAddr.sin_addr));
                                            char writeBuf[4096];
                                            while((numberRead = read(newFd, writeBuf, 4096)) > 0) {
                                                write(datafd, writeBuf, numberRead);
                                            }
                                            close(newFd);
                                        }
                                    }
                                }
                            }
                            // Client requests to send file data over to server
                            if(res[0] == 'P') {
                                char *filename;
                                char *token = strtok(&res[1], "/");
                                // Cycle through tokens of path given to find file name
                                while(token != NULL) {
                                    char *temp = token;
                                    token = strtok(NULL, "/");

                                    if (token == NULL) {
                                        filename = temp;
                                    }
                                }
                                // Make sure file doesn't already exist and create it with given permissions
                                int newFd = open(filename, O_CREAT | O_EXCL | O_RDWR, 0755);

                                if(newFd == -1) {
                                    perror("Cannot create file");
                                    respond(connectfd, 'E', strerror(errno));
                                }
                                else {
                                    // Read data from client and write to newly created file
                                    respond(connectfd, 'A', "");
                                    printf("Receiving file '%s' from client %s\n", filename, inet_ntoa(clientAddr.sin_addr));
                                    char writeBuf[4096];
                                    while((numberRead = read(datafd, writeBuf, 4096)) > 0) {
                                        write(newFd, writeBuf, numberRead);
                                    }

                                    close(newFd);
                                }
                            }
                            close(datafd);
                        }
                        else{
                            respond(connectfd, 'E', strerror(errno));
                        }
                    }
                }
                // Close client connection and exit forked child
                close(connectfd);
                exit(0);
            }
        }
    }
    // This should never be reached with while 1 loop
    return 0;
}
