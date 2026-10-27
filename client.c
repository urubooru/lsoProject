#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

//send all data in a buffer to a socket
int sendAll(int socket, const char* buffer, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    ssize_t result = send(socket, buffer + sent, length - sent, 0);
    if (result < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (result == 0) return -1;
    sent += (size_t)result;
  }
  return 0;
}

//recieve a line from socket and store in buffer, stop at newline
int receiveLine(int socket, char *buffer, size_t capacity) {
  size_t length = 0;
  while (1) {
    char ch;
    ssize_t received = recv(socket, &ch, 1, 0);
    if (received < 0) {
      return -1;
    }
    if (received == 0) return length == 0 ? 0 : -1;
    if (ch == '\n') {
      buffer[length] = '\0';
      return 1;
    }
    if (ch != '\r') {
      if (length + 1 >= capacity) return -1;
      buffer[length++] = ch;
    }
  }
}

//send a line and a newline to a socket
int sendLine(int socket, const char *line) {
  size_t length = strlen(line);
  return sendAll(socket, line, length) == 0 && sendAll(socket, "\n", 1) == 0 ? 0 : -1;
}

int main(int argc, char* argv[]){
  if(argc != 3){
    printf("Usage: %s <ip/hostname> <port>\n", argv[0]);
    return -1;
  }

  //resolve address
  printf("Connecting to %s...\n", argv[1]);
  struct addrinfo hints={0}, *res;
  hints.ai_socktype=SOCK_STREAM;
  hints.ai_family=AF_INET;
  if(getaddrinfo(argv[1], argv[2], &hints, &res) != 0){
    printf("Invalid address");
    return -1;
  }

  //create and connect socket
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if(connect(clientSocket, res->ai_addr, res->ai_addrlen) < 0){
    perror("Connection failed");
    freeaddrinfo(res);
    close(clientSocket);
    return -1;
  }
  freeaddrinfo(res);
  printf("Connected to server\n");

  //set a timeout for the server to send "Ready" after connection before sending other info
  struct timeval readyTimeout = {.tv_sec = 5, .tv_usec = 0};
  //SO_RCVTIMEO is used to set the timeout, https://man7.org/linux/man-pages/man7/socket.7.html
  if(setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &readyTimeout, sizeof(readyTimeout)) < 0){
    perror("setsockopt receive timeout");
    close(clientSocket);
    return -1;
  }

  //recieve the ready message
  char buffer[1024];
  int lineResult = receiveLine(clientSocket, buffer, sizeof(buffer));
  if (lineResult != 1 || strcmp(buffer, "Ready") != 0) {
    fprintf(stderr, "Server did not become ready for this connection\n");
    close(clientSocket);
    return -1;
  }

  readyTimeout.tv_sec = 0;
  readyTimeout.tv_usec = 0;
  //remove the rcvtimeout, recv will act normally afterwards
  if(setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &readyTimeout, sizeof(readyTimeout)) < 0){
    perror("clear receive timeout");
    close(clientSocket);
    return -1;
  }

  //login or register
  printf("Enter username:\n(if the username has not been registered it will create a new user)\n");
  char username[16];
  if(fgets(username, sizeof(username), stdin) == NULL){
    close(clientSocket);
    return -1;
  }
  username[strlen(username) - 1] = '\0'; //remove newline
  if (sendLine(clientSocket, username) < 0) {
    perror("send username");
    close(clientSocket);
    return -1;
  }
  printf("Enter password:\n");
  char password[16];
  if (fgets(password, sizeof(password), stdin) == NULL) {
    close(clientSocket);
    return -1;
  }
  password[strlen(password) - 1] = '\0'; //remove newline
  if (sendLine(clientSocket, password) < 0) {
    perror("send password");
    close(clientSocket);
    return -1;
  }

  //receive the authentication result
  lineResult = receiveLine(clientSocket, buffer, sizeof(buffer));
  if(lineResult != 1){
    printf("Server disconnected\n");
    close(clientSocket);
    return -1;
  }
  printf("Server: %s\n", buffer);
  if(strcmp(buffer, "Success") != 0){
    printf("Wrong credentials\n");
    close(clientSocket);
    return -1;
  } else {
    printf("Login/registration successful\n");
  }
  

  //main loop
  //printf("Use WASD to move or Q to quit:\n");
  while(1){
    //receive data from server
    lineResult = receiveLine(clientSocket, buffer, sizeof(buffer));
    if(lineResult != 1){
      printf("Server disconnected\n");
      break;
    }
    printf("Server: %s\n", buffer);
    if(strcmp(buffer, "Game Over")==0){
      printf("Game Over\n");
      break;
    }

    //send data to server
    printf("Enter command: ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
    buffer[strcspn(buffer, "\n")] = '\0';
    if (sendLine(clientSocket, buffer) < 0) {
      perror("send command");
      break;
    }
  }
  
  close(clientSocket);
  printf("Client terminated\n");
  return 0;
}