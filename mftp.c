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
// Default port number to connect control connection
#define MY_PORT_NUMBER              49999
#define BUF_SIZE                      100

// Function used to recieve response message from server
int servResponse(int socketfd, int needPort) {
    char response[256] = {0};
    int i = 0;

    // Continue reading response from server until a newline is sent
    while(1) {
        if(read(socketfd, &response[i], 1) < 1) {
            perror("Unable to read response from server");
            exit(1);
        }
        // Once a new line is read, replace with null terminator
        if(response[i] == '\n') {
            response[i] = '\0';
            break;
        }
        i++;
    }

    if(response[0] == 'A') {
        //if(response[1] != '\n' || response[1] != '\0')
            //printf("%s\n", &response[1]);

        if(needPort) {
            return (atoi(&response[1]));
        }
        return 1;
    }
    // Print if error messsage is sent
    else if(response[0] == 'E') {
        printf("%s\n", &response[1]);
        return 0;
    }

    return 0;
}

// Function used to concat message type with message (not very usefull)
char* prependChar(char cmd, char* path) {
    char temp[strlen(path) + 1];
    temp[0] = cmd;

    for (int i = 0; i <= strlen(path); i++) {
        temp[i+1] =  path[i];
    }

    return strdup(temp);
}

// Function used to create a data connection with server when file content transfer is needed
int dataConnect(char *ip, int port) {
    int datafd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dataServAddr;
    struct hostent* dataHostEntry;
    struct in_addr **dataPptr;

    memset(&dataServAddr, 0, sizeof(dataServAddr));
    dataServAddr.sin_family = AF_INET;
    dataServAddr.sin_port = htons(port);

    // Get hostname of ip address given on command line
    if((dataHostEntry = gethostbyname(ip)) == NULL) {
        herror("Could not get hostname");
        exit(1);
    }

    dataPptr = (struct in_addr **) dataHostEntry -> h_addr_list;
    memcpy(&dataServAddr.sin_addr, *dataPptr, sizeof(struct in_addr));

    // Trying connecting to the ip address given
    if(connect(datafd, (struct sockaddr *) &dataServAddr, sizeof(dataServAddr)) < 0) {
        perror("Could not connect");
        exit(1);
    }
    // Return file descriptor that will be used from data communication
    return datafd;
}

int main(int argc, char const *argv[]) {
    // Init data types and structures needed for server/client connection
    char ip[20];
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servAddr;
    struct hostent* hostEntry;
    struct in_addr **pptr;
    int numberRead;
    char buf[BUF_SIZE] = {0};

    // Check number of command line arguments match needed input
    if(argc == 2) {
        sscanf(argv[1], "%s", ip);
    }
    else {
        printf("Invalid arguments. Please try:\n> ./mftp hostname\n");
        exit(1);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);

    // Get hostname of ip address given on command line
    if((hostEntry = gethostbyname(ip)) == NULL) {
        herror("Could not get hostname");
        exit(1);
    }

    pptr = (struct in_addr **) hostEntry -> h_addr_list;
    memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

    // Trying connecting to the ip address given
    if(connect(socketfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("Could not connect");
        exit(1);
    }

    printf("Connected to Server: %s\n", ip);

    // Continue to ask user for input until they request to exit or error occurs
    while(1) {
        char inputBuf[BUF_SIZE] = {0};
        char cmd[BUF_SIZE] = {0};
        char path[BUF_SIZE] = {0};

        // Prompt and input collection
        printf("MFTP> ");
        fgets(inputBuf, BUF_SIZE, stdin);
        sscanf(inputBuf, "%s %s", cmd, path);
        // User requests to exit
        if(strcmp(cmd, "exit") == 0) {
            char *msg = prependChar('Q', "");
            write(socketfd, msg, strlen(msg));
            write(socketfd, "\n", 1);
            free(msg);

            servResponse(socketfd, 0);

            exit(0);
        }
        // Command to change local directory
        if(strcmp(cmd, "cd") == 0) {
            char fullPath[256] = {0};
            char cwd[256] = {0};
            getcwd(cwd, sizeof(cwd));
            printf("Old current working dir: %s\n", cwd);
            // If relative path is given, concat it to cwd and change to dir
            if(path[0] != '/') {
                strcpy(fullPath, cwd);
                strcat(fullPath, "/");
                strcat(fullPath, path);
                if(chdir(fullPath) == -1)
                    fprintf(stderr, "Could not change cwd to '%s':\n%s\n", path, strerror(errno));
            }
            // Change to dir if absolute path is given
            else
                if(chdir(path) == -1)
                    fprintf(stderr, "Could not change cwd to '%s':\n%s\n", path, strerror(errno));
            getcwd(cwd, sizeof(cwd));
            printf("New current working dir: %s\n", cwd);
        }
        // Command to list contents in local cwd
        if(strcmp(cmd, "ls") == 0) {
            int pid = fork();
            if(pid < 0) {
                perror("Failed to fork process");
                exit(1);
            }
            if(pid) {
                waitpid(pid, NULL, 0);
            }
            else {
                int fd[2];
                int status;
                int forkFlag;
                // Create file descriptors for piping in to out
                if(pipe(fd) == -1){
                    fprintf(stderr, "Could not pipe.\nError: %s\n", strerror(errno));
                    exit(1);
                }
                // Fork and have child exec ls and parent exec more from childs output
                if((forkFlag = fork())) {
                    // Check that fork was successful
                    if(forkFlag == -1){
                        fprintf(stderr, "Fork was not executed.\nError: %s\n", strerror(errno));
                        exit(1);
                    }
                    // Parent will wait to read what child writes
                    if(close(fd[1]) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    // Close stdin and replace with stdin fd shared with child
                    if(close(0) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    if(dup(fd[0]) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    if(close(fd[0]) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    // Wait for child exec to finish
                    if(wait(&status) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }

                    // Execute more with output from child ls exec
                    execlp("more", "more", "-20", (char*)0);
                    // Exec shouldn't return so error occured
                    fprintf(stderr, "Program didn't run correctly.\nError: %s\n", strerror(errno));
                    exit(1);
                }
                else {
                    // Child will execute a program and write to pipe for parent to read
                    if(close(fd[0]) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }

                    // Close stdout and replace with stdout fd shared with parent
                    if(close(1) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    if(dup(fd[1]) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    if(close(fd[1]) == -1) {
                        fprintf(stderr, "%s\n", strerror(errno));
                        exit(1);
                    }
                    // Execute ls and send output to parent
                    execlp("ls", "ls", "-l", (char*)0);
                    // Exec shouldn't return so error occured
                    fprintf(stderr, "Program didn't run correctly.\nError: %s\n", strerror(errno));
                    exit(1);
                }
            }
        }
        // Command to change directory on server
        if(strcmp(cmd, "rcd") == 0) {
            char *msg = prependChar('C', path);
            write(socketfd, msg, strlen(msg));
            write(socketfd, "\n", 1);

            servResponse(socketfd, 0);

            free(msg);
        }
        // Command that displays server side file/directory contents
        if(strcmp(cmd, "show") == 0 || strcmp(cmd, "rls") == 0) {
            char *msg = prependChar('D', "");
            write(socketfd, msg, strlen(msg));
            write(socketfd, "\n", 1);
            free(msg);
            // Get port sent from server for data communication
            int port = servResponse(socketfd, 1);

            if(port) {
                int datafd = dataConnect(ip, port);
                char *msg;

                // Send client request for ls or 'cat like' action
                if(strcmp(cmd, "show") == 0)
                    msg = prependChar('G', path);
                else
                    msg = prependChar('L', "");
                write(socketfd, msg, strlen(msg));
                write(socketfd, "\n", 1);
                free(msg);

                // Gather response from server
                servResponse(socketfd, 0);

                // Fork to send output from server to client side more exec
                int pid = fork();
                if(pid) {
                    waitpid(pid, NULL, 0);
                }
                else {
                    close(0);
                    dup(datafd);
                    execlp("more", "more", "-20", (char*)0);
                    fprintf(stderr, "Program didn't run correctly.\nError: %s\n", strerror(errno));
                    exit(1);
                }

                close(datafd);
            }
        }
        // Command to transfer file from server to client
        if(strcmp(cmd, "get") == 0) {
            char *msg = prependChar('D', "");
            write(socketfd, msg, strlen(msg));
            write(socketfd, "\n", 1);
            free(msg);

            int port = servResponse(socketfd, 1);

            if(port) {
                // Make connection with port sent from server for data transfer
                int datafd = dataConnect(ip, port);

                char *msg = prependChar('G', path);
                write(socketfd, msg, strlen(msg));
                write(socketfd, "\n", 1);
                free(msg);

                if(servResponse(socketfd, 0)) {
                    char *filename;
                    char *token = strtok(path, "/");
                    // Cycle through path tokens to find name of file
                    while(token != NULL) {
                        char *temp = token;
                        token = strtok(NULL, "/");

                        if (token == NULL) {
                            filename = temp;
                        }
                    }
                    // Make sure file doesn't already exists, create file with given permissions
                    int newFd = open(filename, O_CREAT | O_EXCL | O_RDWR, 0755);

                    if(newFd == -1) {
                        perror("Cannot create file");
                    }
                    else {
                        // Begin writing contents sent from server to newly created file
                        char writeBuf[4096];
                        while((numberRead = read(datafd, writeBuf, 4096)) > 0) {
                            write(newFd, writeBuf, numberRead);
                        }

                        close(newFd);
                    }
                }
                close(datafd);
            }
        }
        // Command to transfer file contents from client to server
        if(strcmp(cmd, "put") == 0) {
            struct stat fs;
            // Make sure file exists and is a regular file
            if(stat(path, &fs) == -1){
                 perror("Unable to determine file type");
            }
            else {
                if((fs.st_mode & S_IFMT) != S_IFREG) {
                    printf("Path given was not for a file\n");
                }
                else{
                    // Request data tranfer and port for new connection
                    char *msg = prependChar('D', "");
                    write(socketfd, msg, strlen(msg));
                    write(socketfd, "\n", 1);
                    free(msg);
                    // Get port number from server for new data connection
                    int port = servResponse(socketfd, 1);

                    if(port) {
                        // Create data connection to tranfer between server/client
                        int datafd = dataConnect(ip, port);

                        char *msg = prependChar('P', path);
                        write(socketfd, msg, strlen(msg));
                        write(socketfd, "\n", 1);
                        free(msg);

                        if(servResponse(socketfd, 0)) {
                            // Check if you can read file and open to read
                            int newFd = open(path, O_RDONLY);

                            if(newFd == -1) {
                                perror("Cannot create file");
                            }
                            else {
                                // Begin writing connents of file to server
                                char writeBuf[4096];
                                while((numberRead = read(newFd, writeBuf, 4096)) > 0) {
                                    write(datafd, writeBuf, numberRead);
                                }
                                close(newFd);
                            }
                        }
                        // Close data connection when finished transfering data
                        close(datafd);
                    }
                }
            }
        }
    }
    // Exit program but line should not be reached
    return 0;
}
