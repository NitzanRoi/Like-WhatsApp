#include <sys/socket.h>
#include <netinet/in.h>
#include "iostream"
#include "whatsappio.h"
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_CONNECTIONS 10
#define BUFFER_SIZE 256
#define EXIT "EXIT"

///todo: bash func to free port : fuser -k portNum/tcp

using std::cout;
using std::cerr;
using std::stoi;
using std::cin;
using std::pair;
using std::vector;

// globals
uint16_t port;
struct sockaddr_in address{};
int masterSocket;
vector<pair<int, string>> connectedClients; // vector of connected clients: each one is a pair: <fd, name>

// todo delete unnecessary uses, and change its name
void printDebug(string p){
    cout << p << "\n";
}

/**
 * reading from the client
 * @param buf the buffer
 * @param fd file descriptor
 * @return length of the input (bcount) if success, -1 otherwise
 */
ssize_t readData(char* buf, int fd){
    unsigned int bcount = 0;
    ssize_t br = 0;
    char* bufferPt = buf;
    char end = '\0';
    while(end != '\n'){
        if ((br = (int)read(fd, bufferPt, BUFFER_SIZE - bcount)) > 0) {
            bcount += br;
            bufferPt += br;
        }
        if (br == 0) { // client disconnected
            printDebug("client disconnected");
            return ERROR_CODE;
        }
        if (br < 0)
            print_error("reading (server) error", ERROR_CODE);
        end = buf[bcount - 1];
    }
    return bcount;
}

/**
 * ensure sockets closing
 * @param clients the sockets array
 */
void closeAll(int clients[]){
    for (int i = 0; i < MAX_CONNECTIONS; i++){
        if (clients[i] != 0)
            close(clients[i]);
        clients[i] = 0;
    }
}

/**
 * define client connection variables
 * @param portNum
 */
void defineClient(char* portNum){
    port = static_cast<uint16_t>(stoi(portNum));
    bzero(&address, sizeof(struct sockaddr_in));
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
}

/**
 * create client connection
 * @return true if success, false otherwise
 */
bool createClientConnection(){
    int opt = 1;
    //create a master socket
    if((masterSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        print_error("server create socket failed", ERROR_CODE);
        return false;
    }
    //set master socket to allow multiple connections
    if(setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char *)&opt, sizeof(opt)) < 0) {
        print_error("setsockopt failed", ERROR_CODE);
        return false;
    }
    //bind the socket to localhost port
    if (bind(masterSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        print_error("bind failed", ERROR_CODE);
        return false;
    }
    printDebug("binding");
    //specify maximum of pending connections for the master socket
    if (listen(masterSocket, MAX_CONNECTIONS) < 0) {
        print_error("listen failed", ERROR_CODE);
        return false;
    }
    printDebug("listening");
    return true;
}

/**
 * check is client exists
 * @param name given name
 * @return true if exists, false otherwise
 */
bool isClientExists(const string& name){
    for (auto &p : connectedClients){
        if (p.second == name)
            return true;
    }
    return false;
}

/**
 * communicate with the clients
 * @return false if something wrong happened, true otherwise
 */
bool communicate(){
    int addrLen, newSocket, maxSd, clientSocket[MAX_CONNECTIONS] = {0};
    auto* message = "Connected Successfully.\n"; // handshaking message
    fd_set clientsfds{}; // the set of all fds
    FD_ZERO(&clientsfds);
    FD_SET(masterSocket, &clientsfds);
    FD_SET(STDIN_FILENO, &clientsfds);
    bool stillRunning = true;
    maxSd = masterSocket;

    while (stillRunning){
        fd_set readfds = clientsfds;
        printDebug("running");
        if (select(maxSd + 1, &readfds, nullptr, nullptr, nullptr) < 0){
            print_error("server select failed", ERROR_CODE);
            close(masterSocket);
            return false;
        }
        printDebug("select done");
        if (FD_ISSET(masterSocket, &readfds)){
            printDebug("socket running");
            // connect new client
            if ((newSocket = accept(masterSocket, (struct sockaddr *)&address, (socklen_t*)&addrLen)) < 0) {
                print_error("accept failed", ERROR_CODE);
                return false;
            }
            // inform user of socket number
            cout << "New connection , socket fd is " << newSocket << "\n";
            if (newSocket > maxSd)
                maxSd = newSocket;
            //send new connection handshake message
            if((size_t)send(newSocket, message, strlen(message), 0) != strlen(message)) {
                print_error("send failed", ERROR_CODE);
                return false;
            }
            printDebug("Welcome message sent successfully");
            // add new socket to array of sockets
            for (int &sock : clientSocket) {
                if(sock == 0) { // if position is empty
                    sock = newSocket;
                    FD_SET(newSocket, &clientsfds);
                    break;
                }
            }
        }
        if (FD_ISSET(STDIN_FILENO, &readfds)){
            string command;
            getline(cin, command); // get input from std
            if (command == EXIT){ // todo if the server process a command - finish it then exit
                for (int &sock : clientSocket) {
                    string msg = "close all";
                    if (sock != 0) {
                        if (((size_t)send(sock, msg.c_str(), msg.length(), 0)) != msg.length()) {
                            print_error("send failed", ERROR_CODE);
                            close(sock);
                            return false;
                        }
                        sock = 0;
                    }
                }
                printDebug("EXIT command is typed: server is shutting down");
                stillRunning = false;
                closeAll(clientSocket);
                // todo: if valgrind is angry maybe fd_zero will make it happy
                return true;
            }
        }
        else {
            for (int i= 0; i < MAX_CONNECTIONS; i++) {
                // check each client if it's in readfds and the get input from it
                if (FD_ISSET(clientSocket[i], &readfds)) {
                    char buffer[256];
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t y = readData(buffer, clientSocket[i]);
                    if (y == ERROR_CODE)
                        continue;
                    buffer[y] = '\0';
                    cout << "hello " << buffer; // prints client name
                }
            }
        }
    }
    return true;
}

/**
 * the main function
 * @param argc num of args
 * @param argv the args
 * @return 0
 */
int main(int argc, char* argv[]) {
    if (argc != 2) { // check number of args
        print_server_usage();
        exit(EXIT_FAILURE);
    }
    if (!isNum(argv[1])) { // check valid port number
        print_server_usage();
        exit(EXIT_FAILURE);
    }
    defineClient(argv[1]);
    if (!createClientConnection())
        exit(EXIT_FAILURE);
    if (!communicate())
        exit(EXIT_FAILURE);
    else
        exit(EXIT_SUCCESS);
    return 0;
}
