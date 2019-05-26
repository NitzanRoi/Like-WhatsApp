#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "iostream"
#include "whatsappio.h"

#define BUFFER_SIZE 256

using std::string;
using std::stoi;
using std::cout;
using std::cin;
using std::strcmp;

/**
 * check valid name
 * @param name name
 * @return if valid
 */
bool isValidName(string name){
    for (char s : name){
        if (!isalpha(s) && !isdigit(s))
            return false;
    }
    return true;
}

// todo this func and its uses
void printDebug(string print){
    cerr << print << "\n";
}

/**
 * reading from the server
 * @param buf the buffer
 * @param fd file descriptor
 */
void readData(char* buf, int fd){
    unsigned int bcount = 0;
    ssize_t br = 0;
    char* bufferPt = buf;
    char end = '\0';
    while(end != '\n') {
        if ((br = read(fd, bufferPt, BUFFER_SIZE - bcount)) > 0) {
            bcount += br;
            bufferPt += br;
        }
        if (br == 0) { // server disconnected
            printDebug("server disconnected");
            break;
        }
        if (br < 0)
            print_error("reading (client) error", ERROR_CODE);
        end = buf[bcount - 1];
    }
}

/**
 * start the communication with the server
 * @param sockfd the file descriptor
 */
void communicate(int sockfd) {
    fd_set fdSet{};
    FD_ZERO(&fdSet);
    FD_SET(sockfd, &fdSet);
    FD_SET(STDIN_FILENO, &fdSet);
    while (true) {
        fd_set readFds = fdSet;
        printDebug("enter your text: ");
        if (select(sockfd + 1, &readFds, nullptr, nullptr, nullptr) < 0) {
            print_error("client select failed", ERROR_CODE);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(sockfd, &readFds)) { // detect messages from the server (i.e. exit)
            char msg[BUFFER_SIZE];
            memset(msg, 0, sizeof(msg));
            readData(msg, sockfd);
            if (strcmp("close all", msg) == 0) { // todo parse command?
                printDebug("client exit");
                close(sockfd);
                exit(EXIT_SUCCESS);
            }
        }
        else if (FD_ISSET(STDIN_FILENO, &readFds)) { // detect input from the keyboard
            string command;
            getline(cin, command);
            cout << "you wrote: " << command << "\n"; // todo parse command
            command.append("\n");
            if ((send(sockfd, command.c_str(), command.length(), 0)) < 0){
                print_error("send failed", ERROR_CODE);
                close(sockfd);
                exit(EXIT_FAILURE);
            }
        }
    }
}

// todo: exit in all errors?
/**
 * the main function
 * @param argc num of args
 * @param argv the args
 * @return 0
 */
int main(int argc, char* argv[]) {

    if (argc != 4) { // check number of args
        print_client_usage();
        exit(EXIT_FAILURE);
    }
    if (!isNum(argv[3])) { // check valid port number
        print_client_usage();
        exit(EXIT_FAILURE);
    }
    if (!isValidName(argv[1])){ // check valid name
        print_client_usage();
        exit(EXIT_FAILURE);
    }

    // define server variables
    struct sockaddr_in serverAddr{};
    struct hostent *server;
    int clientFd;
    bzero((char *) &serverAddr, sizeof(serverAddr)); // reset the client struct

    // check valid ip: -1 is an error, 0 is an invalid ip
    if (inet_pton(AF_INET, argv[2], &(serverAddr.sin_addr)) < 1){
        print_client_usage();
        exit(EXIT_FAILURE);
    }
    uint16_t port = static_cast<uint16_t>(stoi(argv[3]));

    // init server variables
    if ((server = gethostbyname(argv[2])) == nullptr){
        print_error("client get host failed", ERROR_CODE);
        exit(EXIT_FAILURE);
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    bcopy((char *)server->h_addr, (char *) &serverAddr.sin_addr.s_addr, (size_t)server->h_length);

    // create client socket
    if ((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        print_error("client create socket failed", ERROR_CODE);
        exit(EXIT_FAILURE);
    }
    printDebug("socket (client) success");

    // connect to the server through the socket
    if (connect(clientFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0){
        print_error("client connect failed", ERROR_CODE);
        close(clientFd);
        exit(EXIT_FAILURE);
    }
    printDebug("connect (client) success");

    // get connection message from the server
    char bufferClient[BUFFER_SIZE];
    memset(bufferClient, 0, sizeof(bufferClient));
    readData(bufferClient, clientFd);
    cout << bufferClient; // prints connected successfully

    // send client name to server
    string name = argv[1];
    name.append("\n");
    if ((send(clientFd, name.c_str(), name.length(), 0)) < 0){
        print_error("send failed", ERROR_CODE);
        close(clientFd);
        exit(EXIT_FAILURE);
    }

    // start the communication with the server
    communicate(clientFd);

    return 0;
}
