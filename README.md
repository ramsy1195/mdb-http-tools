# MDB Lookup Server
This project implements a TCP server that handles requests for searching a database. It listens for incoming connections from clients, processes their queries, and returns results based on records stored in a database. The server uses a custom binary file format for storing records, which contain a name and a message. The server matches the search query against both the name and the message fields.

## Directory Structure
```text
mdb-http-tools/
├── mdb-lookup-server.c
├── http-client.c
├── README.md
├── Makefile
└── .gitignore
```

## Features
- TCP server that listens for incoming connections.
- Supports database lookups by name or message fields.
- Can handle multiple client connections sequentially.
- Database records are read from a binary file and stored in a linked list.
- The server responds with results to client queries over a TCP connection.
- Search functionality matches the query to both the name and message fields of the records.
- Once the search is complete, a blank line is sent to indicate the end of the search results.

## Prerequisites
- A C compiler (e.g., GCC) to compile the code.
- A binary database file that contains records to search. The format of the database file should match the `MdbRec` structure.
- Standard C libraries and POSIX functions (socket programming).

## Compilation
To compile the project, use the following command:
```bash
gcc mdb-lookup-server.c -o mdb-lookup-server -lm
```
This will produce an executable named `mdb-lookup-server`.

## Usage
To run the server, you need to provide two arguments:
1. The path to the database file (`.mdb` format, binary file).
2. The server port to listen on for incoming connections.

### Example Usage:
```bash
./mdb-lookup-server database.mdb 8080
```
This command will start the server on port `8080` and use the database file `database.mdb` for lookups.

### Client Interaction:
Once the server is running, clients can connect to the server using any TCP client. The server expects clients to send a search query, which will be processed to find matching records.
For example, the client might send a query like:
```bash
Ramya
```
The server will then send back all records whose `name` or `message` field contains "Ramya".
After sending the results, the server will send a blank line to signify the end of the search results.
#### Database Record Structure (`MdbRec`):
Each record in the database is represented by the following structure:
```bash
struct MdbRec {
    char name[50];  // Name of the person
    char msg[200];  // Message associated with the person
};
```

## Notes
- The server is designed to handle multiple client connections sequentially, processing one client at a time.
- Currently, only simple string-based searches are supported (matching the name or msg fields).
- The database must be in binary format for the server to process it correctly.
- The server does not support authentication or encryption.
- Possible extensions or improvements:
  - Implement support for handling multiple clients concurrently using threads or async I/O.
  - Implement indexing for faster search operations, especially with large databases.
  - Add basic authentication to restrict access to certain users or databases.
  - Implement encryption for data transmission to enhance security.
