#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>

typedef struct
{
    float **submatrix;
    float *y;
    int rows;
    int cols;
    int slaveNo;
    int *new_sock;
    float *result;
} slaves;

// Structure to pass arguments to thread function
typedef struct
{
    float **submatrix;
    float *y;
    int rows;
    int cols;
    int slaveNo;
    float *result;
} ThreadData;

float** transposeMatrix(float **matrix, int rows, int cols) {
    // Allocate memory for the transposed matrix
    float **transposedMatrix = (float **)malloc(cols * sizeof(float *));
    for (int i = 0; i < cols; i++) {
        transposedMatrix[i] = (float *)malloc(rows * sizeof(float));
    }

    // Transpose the matrix
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            transposedMatrix[j][i] = matrix[i][j];
        }
    }

    // Return the transposed matrix
    return transposedMatrix;
}


void *pearson_cor(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    for (int j = 0; j < data->cols; j++) {
        float sum_X = 0.0, sum_X_sq = 0.0, sum_y = 0.0, sum_y_sq = 0.0, sum_Xy = 0.0;
        
        for (int i = 0; i < data->rows; i++) {
            float submatrix_element = data->submatrix[i][j];
            sum_X += submatrix_element;
            sum_X_sq += pow(submatrix_element, 2);
            sum_y += data->y[i];
            sum_y_sq += pow(data->y[i], 2);
            sum_Xy += submatrix_element * data->y[i];
        }
        
        float numerator = data->rows * sum_Xy - sum_X * sum_y;
        float denominator = sqrt((data->rows * sum_X_sq - pow(sum_X, 2)) * (data->rows * sum_y_sq - pow(sum_y, 2)));
        
        if (denominator != 0) {
            data->result[j] = numerator / denominator;
        } else {
            data->result[j] = 0; 
        }
    }

    pthread_exit(NULL);
}


void printMatrix(float **matrix, int rows, int cols)
{
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
            printf("%.2f ", matrix[i][j]);
        printf("\n");
    }
}

void set_affinity(int cpu_core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);

    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
    {
        perror("pthread_setaffinity_np");
    }
}

void *connection_handler(void *client);

void master(int matrixSize)
{
    struct timeval begin, end;
    double timeTaken;

    float **matrix = (float **)malloc(matrixSize * sizeof(float *));
    for (int i = 0; i < matrixSize; i++)
    {
        matrix[i] = (float *)malloc(matrixSize * sizeof(float));
        for (int j = 0; j < matrixSize; j++)
            matrix[i][j] = rand() % 10 + 1;
    }

    // printf("\nMatrix: \n");
    // printMatrix(matrix, matrixSize, matrixSize);

    // printf("\nVector y: \n");
    float *y = (float *)malloc(matrixSize * sizeof(float));
    for (int i = 0; i < matrixSize; i++)
    {
        y[i] = rand() % 10 + 1;
        // printf("%f ", y[i]);
    }
    // printf("\n");

    FILE *configFile = fopen("slave_address.txt", "r");
    if (!configFile)
    {
        printf("Error opening config file.\n");
        return;
    }

    int t;
    fscanf(configFile, "%d", &t);
    slaves arrayOfSlaves[t];
    pthread_t sniffer_thread[t];

    int submatrices = matrixSize / t;
    int remainder = matrixSize % t;
    int columnSize, endIndex;

    columnSize = submatrices;
    endIndex = submatrices;

    for (int i = 0; i < t; i++)
    {
        if (i == t - 1)
        {
            columnSize += remainder;
            endIndex += remainder;
        }
        arrayOfSlaves[i].submatrix = (float **)malloc(columnSize * sizeof(float *));
        for (int j = 0; j < columnSize; j++)
        {
            arrayOfSlaves[i].submatrix[j] = (float *)malloc(matrixSize * sizeof(float)); // Transposed allocation
            for (int k = 0; k < matrixSize; k++)
                arrayOfSlaves[i].submatrix[j][k] = matrix[j + i * submatrices][k]; // Transposed indexing
        }
        arrayOfSlaves[i].y = y;
        arrayOfSlaves[i].slaveNo = i;
        arrayOfSlaves[i].rows = columnSize;                                    // rows and columns swapped
        arrayOfSlaves[i].cols = matrixSize;                                    // rows and columns swapped
        arrayOfSlaves[i].result = (float *)malloc(columnSize * sizeof(float)); // Result size matches row size
    }

    printf("\nMatrix and vector y prepared.\n\n");

    int socket_desc, sock;
    struct sockaddr_in server;
    char ip[100];
    int port;

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int core_counter = 0;

    gettimeofday(&begin, NULL);
    
    for (int i = 0; i < t; i++)
    {
        if(core_counter == 8){
            core_counter = 0;
        }

        if (fscanf(configFile, "%s %d", ip, &port) != 2)
        {
            printf("Error reading config file for slave %d.\n", i);
            break;
        }

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == -1)
        {
            printf("Could not create socket for slave %d\n", i);
            continue;
        }

        server.sin_addr.s_addr = inet_addr(ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            printf("Connection to slave %d failed\n", i);
            continue;
        }

        printf("Connected to slave %d\n", i);

        arrayOfSlaves[i].new_sock = malloc(sizeof(int));
        *arrayOfSlaves[i].new_sock = socket_desc;

        //Assign thread to a specific core
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_counter, &cpuset);


        pthread_create(&sniffer_thread[i], NULL, connection_handler, (void *)&arrayOfSlaves[i]);
        pthread_setaffinity_np(sniffer_thread[i], sizeof(cpu_set_t), &cpuset);

        core_counter++;
    }

    fclose(configFile);

    for (int i = 0; i < t; i++)
    {
        pthread_join(sniffer_thread[i], NULL);
    }

    // Aggregate results
    float *final_result = (float *)malloc(matrixSize * sizeof(float));
    for (int i = 0; i < t; i++)
    {
        int startIdx = i * submatrices;
        int rows = arrayOfSlaves[i].rows;
        for (int j = 0; j < rows; j++)
        {
            final_result[startIdx + j] = arrayOfSlaves[i].result[j];
        }
    }

    // Print final result
    // printf("Final Pearson Correlation Coefficients:\n");
    // for (int i = 0; i < matrixSize; i++)
    // {
    //     printf("%.4f ", final_result[i]);
    // }
    // printf("\n");

    gettimeofday(&end, NULL);
    printf("\n---End---\n");
    timeTaken = (end.tv_sec - begin.tv_sec) + ((end.tv_usec - begin.tv_usec) / 1000000.0);
    unsigned long long mill = 1000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / 1000;
    printf("Time taken: %f seconds\n", timeTaken);
    printf("Time taken: %llu milliseconds\n", mill);

    // Free Alocated Memory

    for (int i = 0; i < t; i++)
    {
        for (int j = 0; j < arrayOfSlaves[i].rows; j++)
            free(arrayOfSlaves[i].submatrix[j]);
        free(arrayOfSlaves[i].submatrix);
        free(arrayOfSlaves[i].result);
    }

    free(y);
    free(final_result);
}


void *connection_handler(void *client) {
    slaves *slave = (slaves *)client;
    int sock = *(int *)slave->new_sock;
    int read_size;
    int i, j;
    char slaveMessage[256];

    float **matrixToSlave = slave->submatrix;
    float *y = slave->y;
    int rows = slave->rows;
    int cols = slave->cols;
    int slaveNo = slave->slaveNo;
    printf("Slave %d entered handler.\n\n", slave->slaveNo);

    memset(slaveMessage, 0, 256);

    write(sock, &rows, sizeof(rows));
    write(sock, &cols, sizeof(cols));
    write(sock, &slaveNo, sizeof(slaveNo));

    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++)
            write(sock, &matrixToSlave[i][j], sizeof(matrixToSlave[i][j]));

    for (i = 0; i < cols; i++)
        write(sock, &y[i], sizeof(y[i]));

    // Receive result from slave
    for (i = 0; i < rows; i++) {
        if (recv(sock, &slave->result[i], sizeof(slave->result[i]), 0) < 0) {
            perror("recv result failed");
            close(sock);
            free(slave->result);
            free(slave->new_sock);
            pthread_exit(NULL);
        }
    }

    // printf("\nReceived from slave %d\n", slave->slaveNo );
    // for(i=0; i<rows; i++){
    //     printf("%f ", slave->result[i]);
    // }

    // printf("\n");

    read_size = recv(sock, slaveMessage, 256, 0);

    if (read_size < 0) {
        perror("recv ack failed");
    } else {
        printf("Message from Slave %d: %s\n", slave->slaveNo, slaveMessage);
        printf("Slave %d disconnected.\n\n", slave->slaveNo);
        fflush(stdout);
    }

    free(slave->new_sock);

    // Close socket
    close(sock);

    pthread_exit(NULL);
}


void slave(int port)
{
    int sock, client_sock, c, i, j;
    struct sockaddr_in server, client;
    char ackMessage[256] = "ack";
    float **receivedMatrix, *y;
    int rows, cols, slaveNo;

    struct timeval begin, end;
    double timeTaken;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Could not create socket");
        return;
    }
    printf("Socket creation successful.\n");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    // Bind
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
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
    if (client_sock < 0)
    {
        perror("Accept failed");
        return;
    }
    printf("Connection accepted. Receiving data from master...\n");

    // Receive rows and columns, and slaveNo from master
    if (recv(client_sock, &rows, sizeof(rows), 0) < 0)
    {
        perror("recv rows failed");
        return;
    }
    if (recv(client_sock, &cols, sizeof(cols), 0) < 0)
    {
        perror("recv cols failed");
        return;
    }
    if (recv(client_sock, &slaveNo, sizeof(slaveNo), 0) < 0)
    {
        perror("recv slaveNo failed");
        return;
    }

    // Receive matrix
    receivedMatrix = (float **)malloc(rows * sizeof(float *));
    for (i = 0; i < rows; i++)
    {
        receivedMatrix[i] = (float *)malloc(cols * sizeof(float));
        for (j = 0; j < cols; j++)
        {
            if (recv(client_sock, &receivedMatrix[i][j], sizeof(receivedMatrix[i][j]), 0) < 0)
            {
                perror("recv Matrix failed");
                printf("Error @ [%d][%d]\n", i, j);
                return;
            }
        }
    }

    // printf("\nMatrix: \n");
    // printMatrix(receivedMatrix, rows, cols);

    // printf("\nVector y: \n");

    // Receive vector y
    y = (float *)malloc(cols * sizeof(float));
    for (i = 0; i < cols; i++)
    {
        if (recv(client_sock, &y[i], sizeof(y[i]), 0) < 0)
        {
            perror("recv y failed");
            return;
        }
        // else{
        //     printf("%f ", y[i]);
        // }
    }


    printf("Matrix and vector y received successfully.\n\n");


    float **transposedMatrix = transposeMatrix(receivedMatrix, rows, cols);

    // printf("\nTransposed Matrix: \n");
    // printMatrix(transposedMatrix, cols, rows);


    float *result = (float *)malloc(rows * sizeof(float));
    ThreadData threadData = {transposedMatrix, y, cols, rows, slaveNo, result};

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    gettimeofday(&begin, NULL);

    pthread_t pearson_thread;
    pthread_create(&pearson_thread, NULL, pearson_cor, (void *)&threadData);
    pthread_setaffinity_np(pearson_thread, sizeof(cpu_set_t), &cpuset);
    pthread_join(pearson_thread, NULL);

    gettimeofday(&end, NULL);

    timeTaken = (end.tv_sec - begin.tv_sec) + ((end.tv_usec - begin.tv_usec) / 1000000.0);
    unsigned long long mill = 1000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / 1000;
    mill = 1000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / 1000;
    printf("\nComputation time taken: %f seconds\n", timeTaken);
    printf("Computation time taken: %llu milliseconds\n", mill);

    // Send result back to master
    // printf("\nResult: \n");
    for (i = 0; i < rows; i++)
    {
        write(client_sock, &result[i], sizeof(result[i]));
        // printf("%f ", result[i]);
    }

    write(client_sock, ackMessage, strlen(ackMessage) + 1);
    printf("\nAcknowledgment sent to master.\n");

    // Clean up
    for (i = 0; i < rows; i++)
    {
        free(receivedMatrix[i]);
    }
    free(receivedMatrix);

    for (int i = 0; i < rows; i++) {
        free(transposedMatrix[i]);
    }
    free(transposedMatrix);

    free(y);
    free(result);

    close(client_sock);
    close(sock);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
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
