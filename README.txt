Programmer: Jesse Griffin
Assignment 10: Server/Client FTP
Due Date: 4/15/19
Professor: Ben McCamish

Program Description:
    This assignment in a culmination of two programs. It
    uses both a server and client program.

    The server program will be a daemon that constantly
    waits for a connection. Once it recieves a connection,
    it will fork a child process that will handle the rest
    of the program so that it can continue listening for
    another client. The child act as an ftp interface,
    sending and recieving data.

    The client program will try and connect to the ip address
    given at start of program. Once connected, the client
    will have a variety of commands it can run.

Run Program:
    To compile the server program, use command:

        > gcc -o mftpserve mftpserve.c

    This will create an executable named "mftpserve" that
    will need to be ran before the client code can connect

    After the server program has been compiled, run with:

        > ./mftpserve



    To compile the server program, use command:

        > gcc -o mftp mftp.c

    This will create an executable named "mftp" that
    will be ran to connect to the server

    After the client program has been compiled, run with:

        > ./mftp <hostname>
