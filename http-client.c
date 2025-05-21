/*
 * http-fetcher.c
 *
 * A simple HTTP/1.0 client written in C that downloads a file from a remote server.
 * It connects to a given hostname and port, sends an HTTP GET request for a specified
 * path, and saves the response body (e.g., HTML or binary file) locally.
 *
 * Example usage:
 *   ./http-fetcher www.example.com 80 /index.html
 *
 * Output:
 *   Saves the contents of /index.html as "index.html" in the current directory.
 *
 * NOTE: This is a minimal client and does not handle redirects, chunked responses,
 * or persistent connections (i.e., HTTP/1.1 keep-alive).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

// Print an error message and exit
static void error_exit(const char *message) {
    perror(message);
    exit(1);
}

// Print usage instructions and exit
static void usage() {
    fprintf(stderr, "Usage: http-fetcher <hostname> <port> <path>\n");
    fprintf(stderr, "Example: http-fetcher www.example.com 80 /index.html\n");
    exit(1);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        usage();
    }

    const char *hostname = argv[1];      // e.g., www.example.com
    const char *portStr = argv[2];       // e.g., 80
    const char *urlPath = argv[3];       // e.g., /index.html

    // Extract the filename from the path (everything after the last '/')
    const char *slash = strrchr(urlPath, '/');
    if (!slash) {
        usage(); // Invalid path format
    }

    const char *outputFilename = slash + 1;
    if (!*outputFilename) {
        outputFilename = "index.html"; // Fallback if path ends in '/'
    }

    // Resolve the hostname to an IP address (IPv4 only)
    struct hostent *hostEntry;
    if ((hostEntry = gethostbyname(hostname)) == NULL) {
        error_exit("gethostbyname failed");
    }

    const char *ipAddrStr = inet_ntoa(*(struct in_addr *)hostEntry->h_addr);

    // Create a TCP socket
    int clientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock < 0) {
        error_exit("socket failed");
    }

    // Prepare the server address struct
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ipAddrStr);
    serverAddr.sin_port = htons(atoi(portStr));

    // Connect to the server
    if (connect(clientSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        error_exit("connect failed");
    }

    // Construct and send the HTTP GET request
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer),
             "GET %s HTTP/1.0\r\n"
             "Host: %s:%s\r\n"
             "\r\n",
             urlPath, hostname, portStr);

    if (send(clientSock, buffer, strlen(buffer), 0) != (ssize_t)strlen(buffer)) {
        error_exit("send failed");
    }

    // Wrap the socket in a FILE* stream for easier reading using fgets()
    FILE *socketStream = fdopen(clientSock, "r");
    if (!socketStream) {
        error_exit("fdopen failed");
    }

    // Read the HTTP response status line (e.g., HTTP/1.0 200 OK)
    if (!fgets(buffer, sizeof(buffer), socketStream)) {
        if (ferror(socketStream))
            error_exit("I/O error while reading response");
        fprintf(stderr, "Server closed connection unexpectedly\n");
        exit(1);
    }

    // Validate HTTP protocol and status code
    if (strncmp(buffer, "HTTP/1.0 ", 9) != 0 && strncmp(buffer, "HTTP/1.1 ", 9) != 0) {
        fprintf(stderr, "Unrecognized protocol response: %s\n", buffer);
        exit(1);
    }

    if (strncmp(buffer + 9, "200", 3) != 0) {
        // If response is not 200 OK, just print the line and exit
        fprintf(stderr, "%s", buffer);
        exit(1);
    }

    // Skip HTTP response headers (until a blank line)
    while (fgets(buffer, sizeof(buffer), socketStream)) {
        if (strcmp(buffer, "\r\n") == 0 || strcmp(buffer, "\n") == 0) {
            break; // End of headers
        }
    }

    // Open local file for writing response body
    FILE *outFile = fopen(outputFilename, "wb");
    if (!outFile) {
        error_exit("fopen failed");
    }

    // Read file content (body) from server and write to disk
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), socketStream)) > 0) {
        if (fwrite(buffer, 1, bytesRead, outFile) != bytesRead) {
            error_exit("fwrite failed");
        }
    }

    // Check for read errors
    if (ferror(socketStream)) {
        error_exit("fread failed");
    }

    // Clean up
    fclose(outFile);
    fclose(socketStream); // Also closes underlying socket

    return 0;
}
