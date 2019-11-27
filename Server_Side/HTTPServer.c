#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <signal.h>


#define CLIENTS_IN_QUEUE  10
#define RCVBUFSIZE  10240
#define CONNECTION_TIME_OUT_Sec  2
#define CONNECTION_TIME_OUT_USec  3000000
#define SHMOBJ_PATH  "/ipc_obj"
#define INT_SIZE  4

void interruptHandler(int signal);
void responseForClient(int sckt);
void sendMessageToClient(int sckt, int code, int file_size);
void postResponse(int sckt, char *filename, int file_size);
void print(char * str);
void getResponse(int sckt, char* filename, char* fileType);
void sendBytes(int sckt, int fileSize, int filed);
void DieWithError(char *errorMessage) {
    perror(errorMessage);
}

/** TCP client handling function */
int main() {

    int servSock;
    int clntSock;
    int *active_connections;

    struct sockaddr_in servAddr;
    struct sockaddr_in clntAddr;
    struct timeval time_out = {CONNECTION_TIME_OUT_Sec,0};
    fd_set sckt_set;

    unsigned short servPort;
    unsigned int clntLen;


    signal(SIGINT,interruptHandler);

    /*Main Thread Setup*/
    servPort = 8080;  /* First arg: local port */
    /* Create socket for incoming connections */
    print("Create The Socket");
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket () failed");
    print("End Create The Socket");

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servPort);

    /* Bind to the local address */
    print("Bind The Socket");
    if (bind(servSock, (struct sockaddr *)&servAddr,sizeof(servAddr)) < 0)
        DieWithError("bind () failed");
    print("End Bind The Socket");

    /* Mark the socket so it will listen for incoming connections */
    print("Listen The Socket");
    if (listen(servSock, CLIENTS_IN_QUEUE) < 0)
        DieWithError("listen() failed") ;
    print("End Listen The Socket");

    /* Shared Memory Between Socket processes established */
    int shmfd;

    /* creating the shared memory object    --  shm_open()  */
    shmfd = shm_open(SHMOBJ_PATH, O_CREAT|O_RDWR, S_IRWXU | S_IRWXG);
    if (shmfd < 0) {
        perror("In shm_open()");
        exit(1);
    }
    fprintf(stderr, "Created shared memory object %s\n", SHMOBJ_PATH);

    /* adjusting mapped file size (make room for the whole integer)      --  ftruncate() */
    ftruncate(shmfd, INT_SIZE);
    /* clearing out the location */
    active_connections = (int*)mmap(NULL, INT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    *active_connections = 0 ;


    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        clntLen = sizeof(clntAddr);
        /* Wait for a client to connect */
        print("Accept The Socket");
        if ((clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0) {
            DieWithError("accept() failed");
        }
        print("End Accept The Socket");



        int thrd_num = fork();
        if (thrd_num == 0) {
            print("Child created");
            //Now in the Child Process
            print("Start Responding");


            /* requesting the shared segment    --  mmap() */
            active_connections = (int*)mmap(NULL, INT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
            if (active_connections == NULL) {
                perror("In mmap()");
                exit(1);
            }
            print("Shared memory segment allocated correctly \n"); // for multiple clients #10

            /* incrementing the shared variable */
            *active_connections = *active_connections + 1;

            int ready_for_reading = 0;

            /* Empty the FD Set */
            FD_ZERO(&sckt_set );
            /* Listen to the input descriptor */
            FD_SET(clntSock, &sckt_set);

            print("Start timeout provision ");
            /* Listening for input stream for any activity */
            while(1){
                    time_out.tv_usec = (CONNECTION_TIME_OUT_USec/ *active_connections);
                    ready_for_reading = select(clntSock+1 , &sckt_set, NULL, NULL, &time_out); // give you thread ready for
                    //reading or writing
                    //printf("Time out for this connection is %d \n", (CONNECTION_TIME_OUT_USec/ *active_connections));


                    if (ready_for_reading == -1) {
                        /* Some error has occured in input */
                        print("Unable to read your input\n");
                        break;
                    } else if (ready_for_reading) {
                        responseForClient(clntSock);
                    } else {
                        print(" Timeout - client not responding - closing connection \n");
                        break;
                    }
            }
            *active_connections = *active_connections - 1;
            print("End Connection");
            close(clntSock);
            exit(0);
        } else if (thrd_num > 0) {
            //Now in the Parent Process
        } else {
            //Error in Creating The Child
            perror("Error Creating the Child");
        }
    }

}

/** Top level response function
 * @param  sckt  The server socket number connected to this client.
 * */
void responseForClient(int sckt) {

    char rcvBuffer[RCVBUFSIZE]={0};
    /*char* rcvBuffer = malloc(RCVBUFSIZE*sizeof(char));
    memset(rcvBuffer, 0, RCVBUFSIZE);*/

    int recvMsgSize;
    /* Receive message from client */
//    print("********************** Receving Command From Client");
    if ((recvMsgSize = recv(sckt, rcvBuffer, RCVBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");
//    print("********************** End Receving Command From Client");

    /* Send received string and receive again until end of transmission */
//    print("Before Start While");
    while (recvMsgSize > 0) /* zero indicates end of transmission */
    {
        //An Empty Message
        if (strlen(rcvBuffer) == 0) {
            sendMessageToClient(sckt, 404, 0);
        } else {
            char * first_msg = strtok(rcvBuffer, "\n");
            char * sec_msg = strtok(NULL, "\n");
            printf("%s\n", first_msg);
            char * msgType = strtok(first_msg, " ");
            char *filename = strtok(NULL, " ");
            strtok(NULL, " ");
            char fileNameTemp[BUFSIZ]={0};
            strcpy(fileNameTemp, filename);
            strtok(filename, ".");
            char *fileType = strtok(NULL, ".");

            print("Check Extentiosn");
            if (!strcmp(fileType, "html") && !strcmp(fileType, "jpg") && !strcmp(fileType, "txt")) {
            	sendMessageToClient(sckt, 404, 0);
                //return;
            }
            print("After Check Extentiosn");

            print(msgType);
            if(strncmp(msgType, "GET", 3) == 0) {
                printf("hello form the");
                getResponse(sckt,fileNameTemp,fileType);
            } else if(strncmp(msgType, "POST", 4) == 0) {
                print("Start Sending For Post : THe File");
                postResponse(sckt, fileNameTemp, atoi(sec_msg));
                print("End Sending For Post : THe File");
            }
        }

        //Keep Reading from the user
        print("Start Keep Receving");
        recvMsgSize = recv(sckt, rcvBuffer, sizeof(rcvBuffer), 0);
        print("After Start Keep Receving");
    }
//    print("After Start While");
}

/** Sending HTTP reply codes
 * @param sckt  The server socket connected to this client.
 * @param code  The HTTP code to be sent to the client.
 * @param file_size  The size of the file to be sent/received by the client.
 * */
void sendMessageToClient(int sckt, int code, int file_size) {

    char msg[BUFSIZ] = {0};

    char size_str[BUFSIZ] = {0};
    switch (code) {
        case 200:
            //size_str[0] = (char) file_size;
            //printf("The size %s\n", size_str);
            sprintf(size_str, "%d", file_size);
            strncpy(msg, "HTTP/1.0 200 OK\\r\\n\r\n", sizeof("HTTP/1.0 200 OK\\r\\n\r\n"));
            strncat(msg, size_str, strlen(size_str));
            send(sckt,msg, strlen(msg), 0);
            break;
        case 404:
            strncpy(msg, "HTTP/1.0 404 Not Found\\r\\n\r\n", sizeof("HTTP/1.0 404 Not Found\\r\\n\r\n"));
            send(sckt,msg, strlen(msg), 0);
            break;
    }
}

/** Response for a POST request
 * @param sckt  The server socket connected to this client.
 * @param filename  The name of the file sent by the client.
 * @param file_size  The size of the file to be sent by the client .*/
void postResponse(int sckt, char *filename, int file_size) {
    /*char* buffer = malloc(BUFSIZ*sizeof(char));
    memset(buffer, 0, BUFSIZ);*/
    char fileBuffer[RCVBUFSIZE] = {0};
    /*char* fileBuffer = malloc(RCVBUFSIZE*sizeof(char));
    memset(fileBuffer, 0, RCVBUFSIZE);*/
    //int file_size;
    int bytesReceived = 0;
    FILE *received_file;

    print("Start Sending the OK message for Post");
    sendMessageToClient(sckt, 200, 0);
    print("End Sending the OK message for Post");

    /* Receiving file size */
    print("Start Receving The File Size");
    //recv(sckt, buffer, BUFSIZ, 0);
    print("End Receving The File Size");
    //file_size = atoi(buffer);
    //free(buffer);

    char *org = "/home/said/Desktop/Computer_Networks-master (2)/Basic_Socket_Programming/Server_Side/";
 	char str[250]={0};
 	strcpy(str, org);
 	strcat(str, filename);


    print("Start Create The Receving File");
    received_file = fopen(str, "w");

    if (received_file == NULL)
    {
        DieWithError("Faild to Open File\n");
    }
    print("The Receving File Created");

    /* Receive data in chunks*/
    print("Start Receving the File from Post");
    while (file_size > 0 && (bytesReceived = recv(sckt, fileBuffer, RCVBUFSIZE, 0)) > 0)
    {
        //printf("Bytes received %s\n",fileBuffer);
        //printf("The file Size : %d and bytes : %d\n",file_size, bytesReceived);
//        print("********1*********** Start REc");
//        print("******************* End REc");
        file_size -= bytesReceived;
        //fprintf(received_file, "%s", fileBuffer);
        fwrite(fileBuffer, 1, bytesReceived, received_file);
    }
    print("End Receving the File from Post");
    //free(fileBuffer);

    fclose(received_file);

    if(bytesReceived < 0)
    {
        DieWithError("\n Read Error \n");
    }

}

/** Logging function
 * @param str String that needs to be outputed on the terminal for logging reasons.
 * */
void print(char * str) {
    printf("%s\n", str);
}



/** Response for a GET request
 * @param sckt  The server socket connected to this client.
 * @param filename The name of the file that should be sent to the client .
 * @param fileType  The type of the file to be sent to client .
 * */
void getResponse(int sckt, char* filename, char* fileType){


//
//    FILE * file;
//    file = fopen(filename, "r");

    char *org = "/home/said/Desktop/Computer_Networks-master (2)/Basic_Socket_Programming/Server_Side/";
	char str[250]={0};
	strcpy(str, org);
	strcat(str, filename);

	printf("STR: %s",str);
    if(access(str, R_OK) != -1){
    	printf("here we are");

        long file_size;
        FILE * file_to_send;

        /* Opening File to read its contents */
        file_to_send = fopen(str, "r");
        if (file_to_send == NULL)
        {
            DieWithError("Faild to Open File\n");
        }

        /* Sizing The file first */
        fseek(file_to_send, 0L, SEEK_END);
        file_size = ftell(file_to_send);
        sendMessageToClient(sckt, 200, file_size);
        rewind(file_to_send);
        if (file_size < 1) {
            printf("File is Empty");
            send(sckt , "NULL", sizeof("NULL") , 0);
            //return;
        }

        /* Choosing the method of transmit */
        //printf("The File Name 2  : %s\n", filename);
        int filefd = open(str, O_RDONLY);
        sendBytes(sckt, file_size, filefd);
        close(filefd);
    } else {
        sendMessageToClient(sckt, 404, 0);
        DieWithError("File Doesn't Exist \n ");
    }
}

/** Function that sends files as a ByteStream
 * @param sckt  The server socket connected to this client.
 * @param fileSize  The size of the file to be sent to client.
 * @param filed  The file descriptor from which data is taken to be sent to client.
 * */
void sendBytes(int sckt, int fileSize, int filed) {

    //struct stat file_stat;
    int sent_bytes;

    /* Sending file data */
    print("Start Send the File in Post");
    sent_bytes = sendfile(sckt, filed, NULL, fileSize);
    print("Stop Send the File in Post");
    if (sent_bytes <= 0) {
        DieWithError("Error While Sending file");
    }
}

///* Checks client activity
// * @param clntSock  The server socket connected to this client.
// * @param time_out The timeout interval before a client is deemed inactive.
// * @return more than one if the client is active , -1 otherwise.
// */
//short is_active(int clntSock, struct timeval time_out) {
//
//   if( setsockopt (clntSock, SOL_SOCKET, SO_RCVTIMEO , (char *)&time_out,
//                sizeof(time_out)) < 0 || setsockopt (clntSock, SOL_SOCKET, SO_RCVTIMEO , (char *)&time_out,
//                                                     sizeof(time_out)) < 0) {
//       return 1 ;
//   }
//    return -1
//}
/** Handles interrupt signal
 *
 * @param signal signal type
 */
void interruptHandler(int signal){
    /* Removing shared memory */
    if (shm_unlink(SHMOBJ_PATH) != 0) {
        perror("In shm_unlink()");
        exit(1);
    }
}
