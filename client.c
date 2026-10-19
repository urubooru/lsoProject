#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]){
  if(argc != 2){
    printf("Usage: %s <ip/hostname>\n", argv[0]);
    return -1;
  }

  printf("Connecting to %s...\n", argv[1]);
  struct sockaddr_in serverAddr = {0};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(5050);
  if(inet_pton(AF_INET, argv[1], &serverAddr.sin_addr)<= 0){
    perror("Invalid address");
    return -1;
  }
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if(connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
    perror("Connection failed");
    return -1;
  }

  //set up multicast socket for receiving score updates
  int multicastSocket = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in multicastAddr = {0};
  multicastAddr.sin_family = AF_INET;
  multicastAddr.sin_port = htons(5051);
  if(inet_pton(AF_INET, "", &multicastAddr.sin_addr) <= 0){ // TODO: replace "" with actual multicast address
    perror("Invalid multicast address");
    return -1;
  }
  if(bind(multicastSocket, (struct sockaddr*)&multicastAddr, sizeof(multicastAddr)) < 0){
    perror("Multicast bind failed");
    return -1;
  }
  printf("Connected to server\n");
  return 0;
}