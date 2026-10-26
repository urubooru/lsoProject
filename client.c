#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define PORT 5050
#define PORTSTR "5050"

int main(int argc, char* argv[]){
  if(argc != 2){
    printf("Usage: %s <ip/hostname>\n", argv[0]);
    return -1;
  }

  //resolve address
  printf("Connecting to %s...\n", argv[1]);
  struct addrinfo hints={0}, *res;
  hints.ai_socktype=SOCK_STREAM;
  hints.ai_family=AF_INET;
  char port_str[6]=PORTSTR;
  if(getaddrinfo(argv[1], port_str, &hints, &res) != 0){
    perror("Invalid address");
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

  //login or register
  printf("Enter username:\n(if the username has not been registered it will create a new user)\n");
  char username[16];
  fgets(username, sizeof(username), stdin);
  username[strcspn(username, "\n")] = 0; //remove newline
  send(clientSocket, username, strlen(username), 0);
  printf("Enter password:\n");
  char password[16];
  fgets(password, sizeof(password), stdin);
  password[strcspn(password, "\n")] = 0; //remove newline
  send(clientSocket, password, strlen(password), 0);

  char buffer[1024];
  int bytesReceived;
  bytesReceived = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
  if(bytesReceived <= 0){
    printf("Server disconnected\n");
    close(clientSocket);
    return -1;
  }
  buffer[bytesReceived] = '\0';
  printf("Server: %s\n", buffer);
  if(strcmp(buffer, "Success") != 0){
    close(clientSocket);
    return -1;
  } else {
    printf("Login/registration successful\n");
  }
  

  //main loop
  //printf("Use WASD to move or Q to quit:\n");
  while(1){
    //receive data from server
    bytesReceived = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
    if(bytesReceived <= 0){
      printf("Server disconnected\n");
      break;
    }
    buffer[bytesReceived] = '\0';
    printf("Server: %s\n", buffer);
    if(strcmp(buffer, "Game Over")==0){
      printf("Game Over\n");
      break;
    }

    //send data to server
    printf("Enter command: ");
    fgets(buffer, sizeof(buffer), stdin);
    send(clientSocket, buffer, strlen(buffer), 0);
  }
  
  close(clientSocket);
  printf("Client terminated\n");
  return 0;
}