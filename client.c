#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>

#define PORT 5050

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
  if(getaddrinfo(argv[1], PORT, &hints, &res) != 0){
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

  //main loop
  char buffer[1024];
  int bytesReceived;
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

    //send data to server
    printf("Enter command: ");
    fgets(buffer, sizeof(buffer), stdin);
    send(clientSocket, buffer, strlen(buffer), 0);
  }
  
  close(clientSocket);
  printf("Client terminated\n");
  return 0;
}