#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAP_SIZE 20
#define BUFFER_SIZE 1024

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
int receiveLine(int socket, char *buffer) {
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
      buffer[length++] = ch;
    }
  }
}

//read a line from stdin and store in buffer
int readStdinLine(char *buffer) {
  size_t length = 0; //length of the line read so far
  int tooLong = 0; //flag to indicate if the line is too long for the buffer
  while (1) {
    char ch; //character read from stdin
    ssize_t received = read(STDIN_FILENO, &ch, 1);
    if (received < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (received == 0) {
      if (length == 0 && !tooLong) return 0;
      buffer[length] = '\0';
      return tooLong ? -1 : 1;
    }
    if (ch == '\n') {
      buffer[length] = '\0';
      return tooLong ? -1 : 1;
    }
    if (ch == '\r') continue;
    buffer[length++] = ch;
  }
}

//send a line and a newline to a socket
int sendLine(int socket, const char* line) {
  size_t length = strlen(line);
  return sendAll(socket, line, length) == 0 && sendAll(socket, "\n", 1) == 0 ? 0 : -1;
}

//read an integer
static int parseIntField(const char** cursor, int* value) {
  char* end;
  long parsed = strtol(*cursor, &end, 10);
  if (*cursor == end || (*end != ' ' && *end != '\0')) {
    return -1;
  }
  *value = (int)parsed;
  *cursor = *end == ' ' ? end + 1 : end;
  return 0;
}

//read a name
static int parseNameField(const char* cursor, char* name, size_t capacity) {
  size_t length = strcspn(cursor, " \t\r\n");
  if (length == 0 || length >= capacity || cursor[length] != '\0') return -1;
  memcpy(name, cursor, length);
  name[length] = '\0';
  return 0;
}

//receive local information (map)
int receiveLocalInfo(int socket, const char* header) {
  char *sizeEnd;
  char line[BUFFER_SIZE];
  char terrain[MAP_SIZE][MAP_SIZE + 1];

  if (strncmp(header, "LOCAL ", 6) != 0) {
    char output[]="Received an invalid local map header\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Received an invalid local map header\n");
    return -1;
  }

  errno = 0;
  long mapSize = strtol(header + 6, &sizeEnd, 10);
  if (sizeEnd == header + 6 || errno == ERANGE || *sizeEnd != '\0' || mapSize != MAP_SIZE){
    char output[]="Received an invalid local map header\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Received an invalid local map header\n");
    return -1;
  }

  for (int row = 0; row < MAP_SIZE; row++) {
    if (receiveLine(socket, line) != 1 || strncmp(line, "MAPROW ", 7) != 0 || strlen(line) != 7 + MAP_SIZE) {
      char output[]="Received an invalid local map row\n";
      write(STDERR_FILENO, output, sizeof(output));
      //fprintf(stderr, "Received an invalid local map row\n");
      return -1;
    }
    memcpy(terrain[row], &line[7], MAP_SIZE);
    terrain[row][MAP_SIZE] = '\0';
  }

  if (receiveLine(socket, line) != 1 || strcmp(line, "END") != 0) {
    char output[]="Received an incomplete local map update\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Received an incomplete local map update\n");
    return -1;
  }

  char output[]="\nLocal terrain (# unknown, X wall, O path, @ you):\n";
  write(STDOUT_FILENO, output, sizeof(output));
  //printf("\nLocal terrain (# unknown, X wall, O path, @ you):\n");
  for (int row = 0; row < MAP_SIZE; row++) {
    char* line=terrain[row];
    const char newLine = '\n';
    write(STDOUT_FILENO, line, MAP_SIZE);
    write(STDOUT_FILENO, &newLine, sizeof(char));
    //printf("%s\n", terrain[row]);
  }
  fflush(stdout);
  return 0;
}

//function to receive global information
int receiveGlobalInfo(int socket, const char* header) {
  struct playerInfo{
    int id;
    int x;
    int y;
    char name[16];
  }players[1024];
  int mapSize;
  int playerCount;
  const char *headerFields;

  if (strncmp(header, "GLOBAL ", 7) != 0) {
    char output[]="Received an invalid global map header\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Received an invalid global map header\n");
    return -1;
  }
  headerFields = header + 7;
  if (parseIntField(&headerFields, &mapSize) < 0 || parseIntField(&headerFields, &playerCount) < 0 ||
      *headerFields != '\0' || mapSize != MAP_SIZE || playerCount < 0 || playerCount > 1024) {
    char output[]="Received an invalid global map header\n";
    write(STDERR_FILENO, output, sizeof(output));
    return -1;
  }

  char line[BUFFER_SIZE];
  char ownership[MAP_SIZE][MAP_SIZE + 1];
  for (int row = 0; row < MAP_SIZE; row++) {
    if (receiveLine(socket, line) != 1 || strncmp(line, "OWNROW ", 7) != 0 || strlen(line) != 7 + MAP_SIZE) {
      char output[]="Received an invalid ownership row\n";
      write(STDERR_FILENO, output, sizeof(output));
      //fprintf(stderr, "Received an invalid ownership row\n");
      return -1;
    }
    memcpy(ownership[row], &line[7], MAP_SIZE);
    ownership[row][MAP_SIZE] = '\0';
  }

  for (int i = 0; i < playerCount; i++) {
    const char *playerFields;
    if (receiveLine(socket, line) != 1 || strncmp(line, "PLAYER ", 7) != 0) {
      char output[]="Received an invalid player position\n";
      write(STDERR_FILENO, output, sizeof(output));
      //fprintf(stderr, "Received an invalid player position\n");
      return -1;
    }
    playerFields = line + 7;
    if (parseIntField(&playerFields, &players[i].id) < 0 || parseIntField(&playerFields, &players[i].x) < 0 || 
        parseIntField(&playerFields, &players[i].y) < 0 || 
        parseNameField(playerFields, players[i].name, sizeof(players[i].name)) < 0 || 
        players[i].x < 0 || players[i].x >= MAP_SIZE || players[i].y < 0 || players[i].y >= MAP_SIZE) {
      char output[]="Received an invalid player position\n";
      write(STDERR_FILENO, output, sizeof(output));
      return -1;
    }
  }

  if (receiveLine(socket, line) != 1 || strcmp(line, "END") != 0) {
    char output[]="Received an invalid player position\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Received an incomplete global map update\n");
    return -1;
  }

  char buf1[]="Global ownership (. unclaimed, 0-9 player ID, * ID 10+):\n";
  write(STDOUT_FILENO, buf1, sizeof(buf1));
  //printf("Global ownership (. unclaimed, 0-9 player ID, * ID 10+):\n");
  for (int row = 0; row < MAP_SIZE; row++){
    char* line=ownership[row];
    const char newLine = '\n';
    write(STDOUT_FILENO, line, MAP_SIZE);
    write(STDOUT_FILENO, &newLine, sizeof(char));
    //printf("%s\n", ownership[row]);
  }

  char buf2[]="Players:\n";
  write(STDOUT_FILENO, buf2, sizeof(buf2));
  //printf("Players:\n");
  for (int i = 0; i < playerCount; i++){
    //going to leave this printf here honestly because formatting this would take alot of strcat
    printf("  %s (id %d): (%d, %d)\n", players[i].name, players[i].id, players[i].x, players[i].y);
  }
  fflush(stdout);
  return 0;
}

int main(int argc, char* argv[]){
  if(argc != 3){
    char outputpart1[]="Usage: ";
    char outputpart2[]=" <ip/hostname> <port>\n";
    write(STDOUT_FILENO, outputpart1, sizeof(outputpart1));
    write(STDOUT_FILENO, argv[0], sizeof(argv[0]));
    write(STDOUT_FILENO, outputpart2, sizeof(outputpart2));

    //printf("Usage: %s <ip/hostname> <port>\n", argv[0]);
    return -1;
  }

  //resolve address
  
  char outputpart1[]="Connecting to ";
  char outputpart2[]="...\n";
  write(STDOUT_FILENO, outputpart1, sizeof(outputpart1));
  write(STDOUT_FILENO, argv[1], sizeof(argv[1]));
  write(STDOUT_FILENO, outputpart2, sizeof(outputpart2));
  struct addrinfo hints={0}, *res;
  hints.ai_socktype=SOCK_STREAM;
  hints.ai_family=AF_INET;
  if(getaddrinfo(argv[1], argv[2], &hints, &res) != 0){
    char output[]="Invalid address\n";
    write(STDOUT_FILENO, output, sizeof(output));
    //printf("Invalid address");
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
  char conn[]="Connected to server\n";
  write(STDOUT_FILENO, conn, sizeof(conn));
  //printf("Connected to server\n");

  //set a timeout for the server to send "Ready" after connection before sending other info
  struct timeval readyTimeout = {.tv_sec = 5, .tv_usec = 0};
  //SO_RCVTIMEO is used to set the timeout, https://man7.org/linux/man-pages/man7/socket.7.html
  if(setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &readyTimeout, sizeof(readyTimeout)) < 0){
    perror("setsockopt receive timeout");
    close(clientSocket);
    return -1;
  }

  //recieve the ready message
  char buffer[BUFFER_SIZE];
  int lineResult = receiveLine(clientSocket, buffer);
  if (lineResult != 1 || strcmp(buffer, "Ready") != 0) {
    char output[]="Server did not become ready for this connection\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Server did not become ready for this connection\n");
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
  char buf3[]="Enter username:\n(if the username has not been registered it will create a new user)\n";
  write(STDOUT_FILENO, buf3, sizeof(buf3));
  //printf("Enter username:\n(if the username has not been registered it will create a new user)\n");
  char username[16];
  if(readStdinLine(username) != 1){
    char output[]="Failed to read username\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Failed to read username\n");
    close(clientSocket);
    return -1;
  }
  if(sendLine(clientSocket, username) < 0){
    perror("send username");
    close(clientSocket);
    return -1;
  }
  char buf4[]="Enter password:\n";
  write(STDOUT_FILENO, buf4, sizeof(buf4));
  //printf("Enter password:\n");
  char password[16];
  if(readStdinLine(password) != 1){
    char output[]="Failed to read password\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Failed to read password\n");
    close(clientSocket);
    return -1;
  }
  if (sendLine(clientSocket, password) < 0) {
    perror("send password");
    close(clientSocket);
    return -1;
  }

  //receive the authentication result
  lineResult = receiveLine(clientSocket, buffer);
  if(lineResult != 1){
    char output[]="Server disconnected\n";
    write(STDOUT_FILENO, output, sizeof(output));
    //printf("Server disconnected\n");
    close(clientSocket);
    return -1;
  }
  char buf5[]="Server: ";
  const char newLine = '\n';
  write(STDOUT_FILENO, buf5, sizeof(buf5));
  write(STDOUT_FILENO, buffer, sizeof(buffer));
  write(STDOUT_FILENO, &newLine, sizeof(char));
  //printf("Server: %s\n", buffer);
  if(strcmp(buffer, "Success") != 0){
    char output[]="Wrong credentials\n";
    write(STDOUT_FILENO, output, sizeof(output));
    //printf("Wrong credentials\n");
    close(clientSocket);
    return -1;
  } else {
    char output[]="Login/registration successful\n";
    write(STDOUT_FILENO, output, sizeof(output));
    //printf("Login/registration successful\n");
  }

  //recieve line and check if the server is ready to send the data
  lineResult = receiveLine(clientSocket, buffer);
  if (lineResult != 1 || strcmp(buffer, "Ready") != 0) {
    char output[]="Server did not become ready after login\n";
    write(STDERR_FILENO, output, sizeof(output));
    //fprintf(stderr, "Server did not become ready after login\n");
    close(clientSocket);
    return -1;
  }

  //game logic
  char command[BUFFER_SIZE];
  size_t commandLength = 0;
  int running = 1;
  int receivedInitialLocalInfo = 0;
  int receivedInitialGlobalInfo = 0;
  char buf6[]="Enter command (W/A/S/D to move, Q to quit): ";
  write(STDOUT_FILENO, buf6, sizeof(buf6));
  //printf("Enter command (W/A/S/D to move, Q to quit): ");
  fflush(stdout);
  while (running) {
    //fd set to check multiple sockets with a select later on
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientSocket, &readSet);
    FD_SET(STDIN_FILENO, &readSet);
    int maxFd = clientSocket > STDIN_FILENO ? clientSocket : STDIN_FILENO;

    //select blocks until an IO event  happens, only need to read
    int ready = select(maxFd + 1, &readSet, NULL, NULL, NULL);
    if (ready < 0) {
      if (errno == EINTR) continue;
      perror("select");
      break;
    }

    //isset returns 1 only if arg n1 IS READY to execute IO operations, if its not ready it returns 0!
    if(FD_ISSET(clientSocket, &readSet)){
      lineResult = receiveLine(clientSocket, buffer);
      if(lineResult != 1) {
        char output[]="\nServer disconnected\n";
        write(STDOUT_FILENO, output, sizeof(output));
        //printf("\nServer disconnected\n"); //when we dont recieve anything
        break;
      }
      if(strncmp(buffer, "GLOBAL ", 7)==0){
        if (receiveGlobalInfo(clientSocket, buffer) < 0) break;
        int isInitialGlobalInfo = !receivedInitialGlobalInfo;
        receivedInitialGlobalInfo = 1;
        if (!isInitialGlobalInfo || receivedInitialLocalInfo) {
          char output[]="Enter command (W/A/S/D to move, Q to quit): ";
          write(STDOUT_FILENO, output, sizeof(output));
          //printf("Enter command (W/A/S/D to move, Q to quit): ");
          fflush(stdout);
        }
        continue;
      }
      if(strncmp(buffer, "LOCAL ", 6)==0){
        if (receiveLocalInfo(clientSocket, buffer) < 0) break;
        int isInitialLocalInfo = !receivedInitialLocalInfo;
        receivedInitialLocalInfo = 1;
        if (!isInitialLocalInfo || receivedInitialGlobalInfo) {
          char output[]="Enter command (W/A/S/D to move, Q to quit): ";
          write(STDOUT_FILENO, output, sizeof(output));
          fflush(stdout);
        }
        continue;
      }
      if(strncmp(buffer, "GAMEOVER ", 9) == 0){
        int winnerId;
        int winnerScore;
        int playerScore;
        char winnerName[16];
        const char *resultFields = buffer + 9;
        if (parseIntField(&resultFields, &winnerId) < 0 ||
            parseIntField(&resultFields, &winnerScore) < 0 ||
            parseIntField(&resultFields, &playerScore) < 0 ||
            parseNameField(resultFields, winnerName, sizeof(winnerName)) < 0) {
          char output[]="Received an invalid game result\n";
          write(STDERR_FILENO, output, sizeof(output));
          //fprintf(stderr, "Received an invalid game result\n");
          break;
        }
        /* itoa not implemented
        char output[]="\nGame over. Your score: ";
        char score[]=itoa(playerScore, 10);
        write(STDOUT_FILENO, output, sizeof(output));
        write(STDOUT_FILENO, score, sizeof(score));
        write(STDOUT_FILENO, &newLine, sizeof(char));*/
        printf("\nGame over. Your score: %d.\n", playerScore);
        if (winnerId < 0) { //tie
          /* itoa not implemented
          char output[]="The game ended in a tie at this number of cells: ";
          char score[]=itoa(winnerScore, 10);
          write(STDOUT_FILENO, output, sizeof(output));
          write(STDOUT_FILENO, score, sizeof(score));
          write(STDOUT_FILENO, &newLine, sizeof(char));*/
          printf("The game ended in a tie at %d cells.\n", winnerScore);
        } else {
          //leaving this printf unbothered pleas
          printf("Winner: %s (player %d) with %d cells.\n", winnerName, winnerId, winnerScore);
        }
        break;
      }
      if (strcmp(buffer, "Game Over") == 0) {
        char output[]="\nGame Over\n";
        write(STDOUT_FILENO, output, sizeof(output));
        //printf("\nGame Over\n");
        break;
      }
      char output[]="\nServer: ";
      write(STDOUT_FILENO, output, sizeof(output));
      write(STDOUT_FILENO, buffer, strlen(buffer));
      write(STDOUT_FILENO, &newLine, sizeof(char));
      //printf("\nServer: %s\n", buffer);
    }

    if (FD_ISSET(STDIN_FILENO, &readSet)) {
      char input;
      ssize_t bytesRead = read(STDIN_FILENO, &input, 1);
      if (bytesRead < 0) {
        if (errno == EINTR) continue;
        perror("read command");
        break;
      }
      if (bytesRead == 0) break;

      if (input == '\n') {
        command[commandLength] = '\0';
        if(commandLength == 0){
          char output[]="Enter command (W/A/S/D to move, Q to quit): ";
          write(STDOUT_FILENO, output, sizeof(output));
          //printf("Enter command (W/A/S/D to move, Q to quit): ");
          fflush(stdout);
          continue;
        }
        if(strcmp(command, "q") == 0 || strcmp(command, "Q") == 0){
          running = 0;
          continue;
        }
        if(sendLine(clientSocket, command) < 0){
          perror("send command");
          running = 0;
          continue;
        }
        commandLength = 0;
      }else if(input != '\r'){
          command[commandLength++] = input;
      }
    }
  }
  
  close(clientSocket);
  char terminate[]="Client terminated\n";
  write(STDOUT_FILENO, terminate, sizeof(terminate));
  //printf("Client terminated\n");
  return 0;
}