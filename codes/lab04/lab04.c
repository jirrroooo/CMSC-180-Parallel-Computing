/*
*
* Exercise 4 - Distributing Parts of a Matrix over Sockets
*
* Name: John Rommel B. Octavo
* Student Number: 202*-*****
* Section: CMSC 180 - T3L
* Date: April 3, 2024
*
*/

#define _GNU_SOURCE
#include<stdio.h>
#include<string.h>    
#include<stdlib.h>    
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<unistd.h>    
#include<sys/time.h>
#include<pthread.h> 
 

typedef struct node {
  int **ownMatrix, *new_sock;
  int slaveNo, rows, columns;
} slaves;

void printMatrix(int **matrixToPrint, int rows, int columns) {
  int i, j;
  for(i = 0; i < rows; i++) {
    for (j = 0; j < columns; j++)
      printf("%d ", matrixToPrint[i][j]);
    printf("\n");
  }
}

void *connection_handler(void *client);

void master(int matrixSize) {
    struct timeval begin, end;
    double timeTaken;

    int **matrix = (int **)malloc(matrixSize * sizeof(int *));
    for (int i = 0; i < matrixSize; i++) {
        matrix[i] = (int *)malloc(matrixSize * sizeof(int));
        for (int j = 0; j < matrixSize; j++)
            matrix[i][j] = rand() % 10 + 1;
    }

    FILE *configFile = fopen("slave_address.txt", "r");
    if (!configFile) {
        printf("Error opening config file.\n");
        return;
    }

    int t;
    fscanf(configFile, "%d", &t);
    slaves arrayOfSlaves[t];
    pthread_t sniffer_thread[t];

    int i, j, k;
    int submatrices = matrixSize / t;
    int remainder = matrixSize % t;
    int columnSize, endIndex;

    srand(time(NULL));

    columnSize = submatrices;
    endIndex = submatrices;
    for (i = 0; i < t; i++) {
        if (i == t - 1) {
            columnSize += remainder;
            endIndex += remainder;
        }
        arrayOfSlaves[i].ownMatrix = (int **) malloc(matrixSize * sizeof(int *));
        for (j = 0; j < matrixSize; j++) {
            arrayOfSlaves[i].ownMatrix[j] = (int *) malloc(columnSize * sizeof(int));
            for (k = 0; k < columnSize; k++)
                arrayOfSlaves[i].ownMatrix[j][k] = matrix[j][k + i * submatrices];
        }
        endIndex += submatrices;
        arrayOfSlaves[i].slaveNo = i;
        arrayOfSlaves[i].rows = matrixSize;
        arrayOfSlaves[i].columns = columnSize;
    }

    printf("\nMatrix Prepared.\n\n");
    // printMatrix(matrix, matrixSize, matrixSize);


    int socket_desc, sock;
    struct sockaddr_in server;
    char ip[100];
    int port;

    gettimeofday(&begin, NULL);

    for (i = 0; i < t; i++) {
        if (fscanf(configFile, "%s %d", ip, &port) != 2) {
            printf("Error reading config file for slave %d.\n", i);
            break;
        }

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == -1) {
            printf("Could not create socket for slave %d\n", i);
            continue;
        }

        server.sin_addr.s_addr = inet_addr(ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
            printf("Connection to slave %d failed\n", i);
            continue;
        }

        printf("Connected to slave %d\n", i);

        arrayOfSlaves[i].new_sock = malloc(1);
        *arrayOfSlaves[i].new_sock = socket_desc;

        pthread_create(&sniffer_thread[i], NULL, connection_handler, (void *)&arrayOfSlaves[i]);
    }

    fclose(configFile);

    for (i = 0; i < t; i++) {
        pthread_join(sniffer_thread[i], NULL);
    }

    gettimeofday(&end, NULL);
    printf("---End---\n");
    timeTaken =  (end.tv_sec - begin.tv_sec) + ((end.tv_usec - begin.tv_usec)/1000000.0);
    unsigned long long mill = 1000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / 1000;
    printf("Time taken: %f seconds\n", timeTaken);
    printf("Time taken: %llu milliseconds\n", mill);

    for (i = 0; i < t; i++) {
        for (j = 0; j < matrixSize; j++)
            free(arrayOfSlaves[i].ownMatrix[j]);
        free(arrayOfSlaves[i].ownMatrix);
    }
}

void *connection_handler(void *client) {
    slaves *slave = (slaves *)client;
    int sock = *(int *)slave->new_sock;
    int read_size;
    int i, j;
    char slaveMessage[256];

    int **matrixToSlave = slave->ownMatrix;
    int rows = slave->rows;
    int columns = slave->columns;
    int slaveNo = slave->slaveNo;
    printf("Slave %d entered handler.\n\n", slave->slaveNo);

    memset(slaveMessage, 0, 256);

    write(sock, &rows, sizeof(rows));
    write(sock, &columns, sizeof(columns));
    write(sock, &slaveNo, sizeof(slaveNo));

    for (i = 0; i < rows; i++)
        for (j = 0; j < columns; j++)
            write(sock, &matrixToSlave[i][j], sizeof(matrixToSlave[i][j]));

    read_size = recv(sock, slaveMessage, 256, 0);

    if (read_size < 0) {
        perror("recv ack failed");
    } else {
        printf("Message from Slave %d: %s\n", slave->slaveNo, slaveMessage);
        printf("Slave %d disconnected.\n\n", slave->slaveNo);
        fflush(stdout);
    }

    close(sock);
    free(slave->new_sock);
    pthread_exit(NULL);
}


void slave(int port) {
    int sock, client_sock, c, i, j;
    struct sockaddr_in server, client;
    char ackMessage[256] = "ack";
    int **receivedMatrix, rows, columns, slaveNo;

    struct timeval begin, end;
    double timeTaken;

    // Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        perror("Could not create socket");
        return;
    }
    printf("Socket creation successful.\n");
    

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    // Bind
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return;
    }
    printf("Bind successful.\n");

    // Listen
    listen(sock, 3);
    printf("Waiting for master to initiate connection...\n");

    // Accept incoming connection
    c = sizeof(struct sockaddr_in);
    client_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&c);
    if (client_sock < 0) {
        perror("Accept failed");
        return;
    }
    printf("Connection accepted. Receiving data from master...\n");

    gettimeofday(&begin, NULL);

    // Receive rows and columns, and slaveNo from master
    if (recv(client_sock, &rows, sizeof(rows), 0) < 0) {
        perror("recv rows failed");
        return;
    }
    if (recv(client_sock, &columns, sizeof(columns), 0) < 0) {
        perror("recv columns failed");
        return;
    }
    if (recv(client_sock, &slaveNo, sizeof(slaveNo), 0) < 0) {
        perror("recv slaveNo failed");
        return;
    }

    // Receive elements one by one (((inefficiently)))
    receivedMatrix = (int **)malloc(rows * sizeof(int *));
    for (i = 0; i < rows; i++) {
        receivedMatrix[i] = (int *)malloc(columns * sizeof(int));
        for (j = 0; j < columns; j++) {
            if (recv(client_sock, &receivedMatrix[i][j], sizeof(receivedMatrix[i][j]), 0) < 0) {
                perror("recv Matrix failed");
                printf("Error @ [%d][%d]\n", i, j);
                return;
            }
        }
    }

    gettimeofday(&end, NULL);

    printf("Matrix received successfully.\n\n");

    timeTaken =  (end.tv_sec - begin.tv_sec) + ((end.tv_usec - begin.tv_usec)/1000000.0);
    unsigned long long mill = 1000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / 1000;
    printf("Time taken: %f seconds\n", timeTaken);
    printf("Time taken: %llu milliseconds\n", mill);

    // printMatrix(receivedMatrix, rows, columns);

    // Send acknowledgment to master
    write(client_sock, ackMessage, strlen(ackMessage) + 1);
    printf("Acknowledgment sent to master.\n");

    // Clean up
    for (i = 0; i < rows; i++) {
        free(receivedMatrix[i]);
    }
    free(receivedMatrix);

    close(client_sock);
    close(sock);
}


int main(int argc , char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s [n] [port] [s]\n", argv[0]);
        return EXIT_FAILURE;
    }
  
  int n = atoi(argv[1]);
  int port = atoi(argv[2]);
  int s = atoi(argv[3]);

  if (s == 0) 
    master(n);
  else if (s == 1)
    slave(port);
  
  return 0;

}