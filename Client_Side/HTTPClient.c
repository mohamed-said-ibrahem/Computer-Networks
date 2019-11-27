
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <asm/errno.h>
#include <errno.h>
#include <time.h>

#define RCVBUFSIZE 10240       /* Size of receive buffer */
#define PIPELINE 50

void handleGetResponse(char* filename, int socket);
void handlePostResponse_Request(char* filename, int socket, int fifd);
int isOK(char* msg); //status
void sendFileToServer(char *filename, int socket, int filefd); //post request
void startConnection(char * command, char* command_type, char* file_name, char* host_name, int port_num, int sock);
void print(char * str);
int get_file_size(char * filename);

void DieWithError(char *errorMessage) {
    perror(errorMessage);
}

int files_sizes = 0;

int main(int argc, char *argv[])
{
	print("hello");
    int sock;
    struct sockaddr_in servAddr;
    unsigned short servPort;
    char *servlP;
    clock_t start, end;
		double cpu_time_used;

    if ((argc< 2) || (argc> 3)) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage: %s <Server IP> [<Server Port>]\n",
                argv[0]);
        exit(1);
    }

    servlP = argv[1] ;

/* First arg' server IP address  */
/* Second arg'  port number */
    if (argc == 3)
        servPort = atoi(argv[2]); /* Use given port, if any */
    else
        servPort = 8080; /* 8080 is the well-known port for HTTP*/

    print("Before Socket Creation");
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError(" socket () failed") ;
    print("After Socket Creation");
/* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));
/* Zero out structure */
    servAddr.sin_family
            = AF_INET;
/* Internet address family */
    servAddr.sin_addr.s_addr = inet_addr(servlP);
/* Server IP address */
    servAddr.sin_port
            = htons(servPort); /* Server port */


    FILE *fp;
    char * line;
    size_t len = 0;
    ssize_t read_size;
    int port_number = 8080 ;
    char * command_type;
    char * file_name = NULL;
    char * host_name;
    int num_req = 0;

    print("Before Read File Commands");
    fp = fopen("/home/said/Desktop/Computer_Networks-master (2)/Basic_Socket_Programming/Client_Side/c.txt", "r");
    if (fp == NULL)
        DieWithError("Commands File Not Found");
    print("After Read File Commands");

    /* Establish the connection to the  server */
    print("Before Socket Connect");
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        DieWithError("connect () failed") ;
    print("After Socket Creation");

    print("Start Reading Commands");
    files_sizes = 0;
		start = clock();
		while ((read_size = getline(&line, &len, fp)) != -1) {
        print("Start Handling Command");
        char command_arr [strlen(line)];
        strncpy(command_arr , line, strlen(line));
        //printf("The Command is %s\n", line);
        command_type = strtok (command_arr," ");
        file_name = strtok (NULL," ");
        host_name = strtok (NULL," ");
        char * pn = strtok(NULL, " ");
        if (pn != NULL)
            port_number = atoi(pn);
        print("End Handling Command");
        print("Start Connection");
        startConnection(line, command_type,file_name,host_name,port_number,sock);
        print("End Connection");
        print("Close Socket");
        num_req++;
    }
    print("End Reading Commands");
    fclose(fp);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;


    printf("The Time Taken %f\n", cpu_time_used);
    printf("The num of Requests %d\n", num_req);
    printf("The Total Files sizes %d\n", files_sizes);
    close(sock);
    exit(0);
}

void startConnection(char * command, char* command_type, char* file_name, char* host_name, int port_num, int sock) {
//    char *org = "/home/said/Desktop/Computer_Networks-master (2)/Basic_Socket_Programming/Client_Side/";
	char *org="";
    char str[250] = {0};
    strcpy(str, org);
    strcat(str, file_name);
	if(strcmp(command_type, "POST") == 0){

        int file_size = get_file_size(str);
        files_sizes += file_size;
        char size_str[BUFSIZ] = {0};
        sprintf(size_str, "%d", file_size);
        strncat(command, size_str, strlen(size_str));
        print("In Post Start");
        int filefd = open(str, O_RDONLY);
        if (filefd == -1) {
            DieWithError(" open file failed") ;
        }
        print("Start Sending in Post");
        if (send(sock, command, strlen(command), 0) != strlen(command))
            DieWithError("send() sent a different number of bytes than expected");
        print("Finish Sending in Post");
        print("Start Handling Post Response");
        handlePostResponse_Request(str, sock, filefd); //filefd pointer to check if you can open the file with read only
        print("End Handling Post Response");
    } else if (strcmp(command_type, "GET") == 0){
        fd_set active_fd_set, read_fd_set;
        /* Connection request on original socket. */
        print("Start Sending in GET");
        if (send(sock, command, strlen(command), 0) != strlen(command))
            DieWithError("send() sent a different number of bytes than expected");
        print("Stop Sending in GET");
        print("Start Handling GET Response");
        FD_ZERO (&active_fd_set);
        FD_SET (sock, &active_fd_set);
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)  //select to block and check
        {
            perror ("select");
            exit (EXIT_FAILURE);
        }

        /* Service all the sockets with input pending. */
        for (int i = 0; i < FD_SETSIZE; ++i)
            if (FD_ISSET (i, &read_fd_set)) {
                //If something happened on the master socket ,
                //then its an incoming connection
//        if (FD_ISSET(sock, &readfds)) {
                handleGetResponse(str, sock);
                //      }
            }
        print("Stop Handling GET Response");
    } else{
        printf("The Command : %s\n", command);
        DieWithError("Command not supported");
    }
}



void handleGetResponse(char* filename, int socket) {

    //set of socket descriptors

    //add master socket to set
    int bytesReceived = 0;
    /*char* recvBuff = malloc(RCVBUFSIZE*sizeof(char));
    memset(recvBuff, 0, RCVBUFSIZE);*/
    char recvBuff[RCVBUFSIZE]={0};
    /*char* fileSizeBuff = malloc(BUFSIZ*sizeof(char));
    memset(fileSizeBuff, 0, BUFSIZ);*/
    print("Start Recieve The File Size from GET");
    /* Receiving file size */
    //TODO: Add Again if No Effect
    //recv(socket, fileSizeBuff, BUFSIZ, 0);
    print("Stop Recieve The File Size from GET");

    //int file_size = atoi(fileSizeBuff);
    //free(fileSizeBuff);

    //printf("From Socket %d and the size sent is %d\n", socket, file_size);

    /* Create file where data will be stored */
    char repoBuff2[BUFSIZ] = {0};
    /*char* repoBuff = malloc(BUFSIZ*sizeof(char));
    memset(repoBuff, 0, BUFSIZ);*/
        //Wait For OK
    print("Wait From OK for GET");
    if ((recv(socket, repoBuff2, BUFSIZ, 0)) > 0)
    {
        print("Check OK for GET");
        printf("Buffffer %s\n",repoBuff2);

        int file_size = isOK(repoBuff2);
        files_sizes += file_size;
        if (file_size != -1) {

            print("Create a file in GET");
            FILE *fp;
//            printf("fiiiiile name %s",filename);

            char *org = "";
            char str[250] = {0};
            strcpy(str, org);
            strcat(str, filename);

            printf("naaaaame %s",str);

            fp = fopen(str, "w");
            if (fp == NULL) {
                DieWithError("Error opening file");
            }
            print("Stop Create a file in GET");

            /* Receive data in chunks*/
            print("While Reading the File from GET");
            printf("file_ Size %d",file_size);

            while (file_size > 0 && (bytesReceived = recv(socket, recvBuff, RCVBUFSIZE, 0)) > 0)
                //if ((bytesReceived = recv(socket, recvBuff, RCVBUFSIZE, 0)) > 0)
            {
                print("************************************ REC Start");
                //bytesReceived = recv(socket, recvBuff, RCVBUFSIZE, 0);
                print("************************************ REC END");
                //printf("The file Size : %d and bytes : %d\n",file_size, bytesReceived);
                file_size -= bytesReceived;
                //fprintf(fp, "%s", recvBuff);
                fwrite(recvBuff, 1, bytesReceived, fp);
            }
            print("End Reading the File from GET");
            //free(recvBuff);
            fclose(fp);

            if (bytesReceived < 0) {
                DieWithError("\n Read Error \n");
            }
        }
    }
}

void handlePostResponse_Request(char* filename, int socket, int fifd) {

    char repoBuff[BUFSIZ] = {0};
    /*char* repoBuff = malloc(BUFSIZ*sizeof(char));
    memset(repoBuff, 0, BUFSIZ);*/

    //Wait For OK
    print("Wait From OK for POST");
    if ((recv(socket, repoBuff, BUFSIZ, 0)) > 0)
    {
        //printf("%s\n",repoBuff);
        print("Check OK for POST");
        if (isOK(repoBuff) != -1) {
            //Start Sending the File
            print("OK Recieved and Start send File To server from Post");
            sendFileToServer(filename, socket, fifd);
            print("End send File To server from Post");
        } else {
            //DieWithError(repoBuff);
        }
    } else {
        DieWithError("Error While Recieving");
    }
    //free(repoBuff);
}

void sendFileToServer(char *filename, int socket, int filefd) {

    struct stat file_stat;
    int sent_bytes;

    print("Start get File Size to send");
    /* Get file stats */
    if (fstat(filefd, &file_stat) < 0)
    {
        DieWithError("Error in fstat");
    }
    print("Stop get File Size to send");

    int fileSize = file_stat.st_size;
    //char fileSizeStr[BUFSIZ]={0};
    /*char* fileSizeStr = malloc(BUFSIZ*sizeof(char));
    memset(fileSizeStr, 0, BUFSIZ);*/
    //sprintf(fileSizeStr, "%d\r", fileSize);

    /*print("Start Send the File Size");
    if(send(socket, fileSizeStr, sizeof(fileSizeStr), 0) < 0) {
        DieWithError("Error while sending size file");
    }
    print("Stop Send the File Size");*/
    //free(fileSizeStr);

    /* Sending file data */
    print("Start Send the File in Post");
    sent_bytes = sendfile(socket, filefd, NULL, fileSize);
    print("Stop Send the File in Post");
    if (sent_bytes <= 0) {
        DieWithError("Error While Sending file");
    }
}

int isOK(char* msg) {
//    printf("The Message is : %s\n", msg);
    char * first_msg = strtok(msg, "\n");
    char * sec_msg = strtok(NULL, "\n");
    printf("%s\n", first_msg);
    //printf("Second is : %s\n", sec_msg);
    strtok(first_msg, " ");
    char *respType = strtok(NULL, " ");
    if (strcmp(respType, "200") == 0) {
        return atoi(sec_msg);
    } else if (strcmp(respType, "404") == 0) {
        return -1;
    }
    return -1;
}

void print(char * str) {
    printf("%s\n", str);
}

int get_file_size(char * filename) {
    long file_size;
    FILE * file_to_send;

    /* Opening File to read its contents */
    file_to_send = fopen(filename, "r");
    if (file_to_send == NULL)
    {
        DieWithError("Faild to Open File\n");
    }

    /* Sizing The file first */
    fseek(file_to_send, 0L, SEEK_END);
    file_size = ftell(file_to_send);
    return file_size;
}

