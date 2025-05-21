/*
 * mdb-lookup-server.c
 *
 * This program merges mdb-lookup.c and TCPEchoServer.c into a server
 * that listens for TCP client connections and performs database lookups.
 * Clients can query the server with a search key, and the server will
 * send back matching records from the database.
 * 
 * Comments marked with "MODIFIED" indicate places where changes were made
 * for clarity, reorganization, and readability. The logic remains the same.
 */

#include "mdb.h"
#include "mylist.h"

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and accept() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>     /* for signal() */

#define MAX_CONNECTIONS 5   /* Maximum outstanding connection requests */
#define MAX_KEY_LENGTH 5    /* Max key length for search queries */

/* Function to handle errors and terminate the program */
static void terminate(const char *message)
{
    perror(message);
    exit(1); 
}

/* Function to handle client requests */
void processClientRequest(int clientSocket, char *databaseFile);

/* Main function - server entry point */
int main(int argc, char *argv[])
{
    int serverSocket;           /* Socket descriptor for server */
    int clientSocket;           /* Socket descriptor for client */
    struct sockaddr_in serverAddr; /* Server address */
    struct sockaddr_in clientAddr; /* Client address */
    unsigned short serverPort;     /* Server port */
    unsigned int clientAddrLength; /* Length of client address structure */

    /* Ignore SIGPIPE to prevent termination when writing to a disconnected socket */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) 
        terminate("signal() failed");

    /* Ensure proper usage: database file and server port must be specified */
    if (argc != 3)  
    {
        fprintf(stderr, "Usage:  %s <database_file> <Server Port>\n", argv[0]);
        exit(1);
    }

    char *databaseFile = argv[1]; /* Database filename from arguments */
    serverPort = atoi(argv[2]);   /* Server port from arguments */

    /* Create socket for incoming connections */
    if ((serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        terminate("socket() failed");

    /* Prepare server address structure */
    memset(&serverAddr, 0, sizeof(serverAddr));   // Zero out structure
    serverAddr.sin_family = AF_INET;                // Internet address family
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
    serverAddr.sin_port = htons(serverPort);       // Local port

    /* Bind to the local address */
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        terminate("bind() failed");

    /* Mark the socket to listen for incoming connections */
    if (listen(serverSocket, MAX_CONNECTIONS) < 0)
        terminate("listen() failed");

    for (;;) /* Infinite loop to handle incoming connections */
    {
        clientAddrLength = sizeof(clientAddr); /* Initialize client address size */

        /* Wait for a client to connect */
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLength)) < 0)
            terminate("accept() failed");

        /* Client is now connected */
        fprintf(stderr, "\nConnection established with: %s\n", inet_ntoa(clientAddr.sin_addr));

        /* Process client request */
        processClientRequest(clientSocket, databaseFile);

        /* Log when the client connection terminates */
        fprintf(stderr, "Connection terminated from: %s\n", inet_ntoa(clientAddr.sin_addr));
    }

    /* NOT REACHED */
}

/* Function to handle client communication and database lookups */
void processClientRequest(int clientSocket, char *databaseFile)
{
    /* Open the specified database file for reading */
    FILE *filePointer = fopen(databaseFile, "rb"); // Open in binary mode
    if (filePointer == NULL) 
        terminate("Failed to open database file");

    /* Initialize a linked list to store database records */
    struct List recordList;
    initList(&recordList);

    struct MdbRec record; 
    struct Node *node = NULL;

    /* Read all records from the database file into memory */
    while (fread(&record, sizeof(record), 1, filePointer) == 1) 
    {
        /* Dynamically allocate memory for a new record and copy the data */
        struct MdbRec *recordCopy = (struct MdbRec *)malloc(sizeof(record));
        if (!recordCopy)
            terminate("Memory allocation failed");

        memcpy(recordCopy, &record, sizeof(record));

        /* Add the record to the linked list */
        node = addAfter(&recordList, node, recordCopy);
        if (node == NULL) 
            terminate("Failed to add record to list");
    }

    /* Check for any fread() error */
    if (ferror(filePointer)) 
        terminate("Error reading database file");

    /* Wrap the client socket with a FILE* for easier reading */
    FILE *clientInput = fdopen(clientSocket, "r");
    if (clientInput == NULL) 
        terminate("Failed to associate file stream with socket");

    char queryLine[1000];
    char searchKey[MAX_KEY_LENGTH + 1];

    char resultBuffer[1000];
    int resultLength;
    int sendResult;

    /* Process the client's queries */
    while (fgets(queryLine, sizeof(queryLine), clientInput) != NULL) 
    {
        /* Extract the search key and remove any newline character */
        strncpy(searchKey, queryLine, sizeof(searchKey) - 1);
        searchKey[sizeof(searchKey) - 1] = '\0';
        size_t lastChar = strlen(searchKey) - 1;
        if (searchKey[lastChar] == '\n')
            searchKey[lastChar] = '\0';

        /* Traverse the list and search for matching records */
        struct Node *currentNode = recordList.head;
        int recordCount = 1;
        while (currentNode) 
        {
            struct MdbRec *currentRecord = (struct MdbRec *)currentNode->data;
            if (strstr(currentRecord->name, searchKey) || strstr(currentRecord->msg, searchKey)) 
            {
                /* Format the result and send it to the client */
                resultLength = sprintf(resultBuffer, "%4d: {%s} said {%s}\n", recordCount, currentRecord->name, currentRecord->msg);
                if ((sendResult = send(clientSocket, resultBuffer, resultLength, 0)) != resultLength) 
                {
                    perror("send() failed");
                    break;
                }
            }
            currentNode = currentNode->next;
            recordCount++;
        }

        /* Send a blank line to indicate the end of search results */
        resultLength = sprintf(resultBuffer, "\n");
        if ((sendResult = send(clientSocket, resultBuffer, resultLength, 0)) != resultLength) 
            perror("send() failed");
    }

    /* Check if fgets() failed while reading from the client */
    if (ferror(clientInput)) 
        perror("fgets() failed");

    /* Cleanup: free all dynamically allocated records */
    traverseList(&recordList, &free);
    removeAllNodes(&recordList);

    /* Close the database file and socket */
    fclose(filePointer);
    fclose(clientInput);
}
