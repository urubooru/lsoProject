#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>  
#include <pthread.h>

#define WALL 0
#define PATH 1
#define SIZE 35
#define PORT 5050
//map
struct map {
  int map[SIZE][SIZE];
  int ownerShip[SIZE][SIZE];
};
typedef struct map Map;
//user with alot of info which will be used later
struct user {
  int id;
  int x;
  int y;
  char name[16];
  char password[16];
  int score;
  int localMap[SIZE][SIZE];
  int isConnected;
};
typedef struct user User;

struct gameArgs {
  User *users;
  int gameLength;
};
typedef struct gameArgs GameArgs;

struct clientArgs {
  int socket;
  User *users;
};
typedef struct clientArgs ClientArgs;

//way better than using 4 flags for each direction
void revealAround(User *u, const Map *m) {
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++) {
      int nx = u->x + dx, ny = u->y + dy;
      if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE)
        u->localMap[nx][ny] = m->map[nx][ny];
    }
}

//completely random generation of the map, with a 50% chance of being a wall or a path
int generateRandomMap(Map mapIn) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      // Randomly assign WALL or PATH
      mapIn.map[i][j] = (rand() % 2 == 0) ? WALL : PATH;
    }
  }
  return 0;
}

//Check for any isolated PATH cells and convert them to WALL
int cleanUpMap(Map mapIn) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if (mapIn.map[i][j] == PATH) {
        int wallCount = 0;
        // Check adjacent cells
        if (i > 0 && mapIn.map[i - 1][j] == WALL) wallCount++; // Up
        if (i < SIZE - 1 && mapIn.map[i + 1][j] == WALL) wallCount++; // Down
        if (j > 0 && mapIn.map[i][j - 1] == WALL) wallCount++; // Left
        if (j < SIZE - 1 && mapIn.map[i][j + 1] == WALL) wallCount++; // Right
        if (wallCount == 4) {
          mapIn.map[i][j] = WALL; // Convert isolated PATH to WALL
        }
      }
    }
  }
  return 0;
}
// simple function to output the map to the console, for debugging purposes
int outputMap(Map mapIn) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if (mapIn.map[i][j] == WALL) {
        printf("X"); // Represent WALL with X
      } else {
        printf("O"); // Represent PATH with O
      }
    }
    printf("\n");
  }
  return 0;
}

int id=0;
//only works with last line ending with a \n
void loadUsers(User* u){
  int fd = open("./user.data", O_RDONLY);
  if(fd < 0){
    perror("open ./user.data");
    return;
  }
  char buf;
  char name[16];
  char password[16];
  int pwd=0;
  int i=0;
  //in this while, read 1 char at a time, save in name or password, assign a user and increment the id for the next user
  while(1){
    int r;
    if((r=read(fd, &buf, sizeof(char))) < 0){
      perror("Errore read");
      return;
    } else if(r==0){break;}

    if(buf==' '){
      name[i]='\0'; i=0; pwd=1; continue;
    } else if(buf=='\n'){
      password[i]='\0'; pwd=0; i=0;
      User tmpUsr;
      strcpy(tmpUsr.name, name);
      strcpy(tmpUsr.password, password);
      tmpUsr.id=id;
      tmpUsr.isConnected=0;
      u[id]=tmpUsr;
      printf("Loaded user: %s %s\n", name, password);
      id++;
      continue;
    } 
    
    if(pwd==0){
      name[i++]=buf;
    } else if(pwd==1){
      password[i++]=buf;
    }
  }
}

//simple function to print all users, for debugging purposes
void printUsers(User* u){
  for(int i=0; i<id; i++){
    printf("%d %s %s\n", u[i].id, u[i].name, u[i].password);
  }
}
//function to start the game, not implemented yet
void startGame(User* u, int gameLength){
  printf("Game started for %d seconds\n", gameLength);
  sleep(gameLength);
  printf("Game ended\n");
  for(int i=0; i<id; i++){
    if(u[i].isConnected!=0){
      printf("User %s scored %d\n", u[i].name, u[i].score);
    }
  }
}
//thread function to run the game in a separate thread
void* runGame(void* arg) {
  GameArgs *gameArgs = arg;
  startGame(gameArgs->users, gameArgs->gameLength);
  free(gameArgs);
  return NULL;
}
//send all data in a buffer to a socket
int sendAll(int socket, const char* buffer, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    ssize_t result = send(socket, buffer + sent, length - sent, MSG_NOSIGNAL);
    if (result < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (result == 0) return -1;
    sent += (size_t)result;
  }
  return 0;
}
//recieve a line of data from a socket, stopping at a newline character
int receiveLine(int socket, char* buffer, size_t capacity) {
  size_t length = 0;
  while (1) {
    char ch;
    ssize_t received = recv(socket, &ch, 1, 0);
    if (received < 0) {
      if (errno == EINTR) continue;
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

pthread_mutex_t usersMutex = PTHREAD_MUTEX_INITIALIZER; //init mutex on stack(not heap, no free)
//simple function to check if a user exists, and if not, create a new user with the given name and password
int authenticateOrRegister(User* users, const char* name, const char* password) {
  if (name[0] == '\0' || password[0] == '\0' || strchr(name, ' ') != NULL || strchr(password, ' ') != NULL)
    return 0;

  int result = 0;
  pthread_mutex_lock(&usersMutex);
  for (int i = 0; i < id; i++) {
    if (strcmp(users[i].name, name) == 0) {
      result = strcmp(users[i].password, password) == 0;
      pthread_mutex_unlock(&usersMutex);
      if(result==0){
        users[i].isConnected = 1;
      }
      return result;
    }
  }

  int fd = open("./user.data", O_WRONLY | O_APPEND | O_CREAT, 0600); //0600 is rw------- in octal, only owner can read/write
  if (fd < 0) {
    pthread_mutex_unlock(&usersMutex);
    return -1;
  }

  char record[sizeof(users[id].name) + sizeof(users[id].password) + 2];
  strcpy(record, name);
  strcat(record, " ");
  strcat(record, password);
  strcat(record, "\n");

  size_t written = 0;
  written = write(fd, record, (size_t)strlen(record));
  if (written != strlen(record)) {
    close(fd);
    pthread_mutex_unlock(&usersMutex);
    return -1;
  }
  close(fd);

  User* user = &users[id];
  memset(user, 0, sizeof(*user));
  user->id = id;
  strcpy(user->name, name);
  strcpy(user->password, password);
  user->isConnected = 1;
  id++;
  pthread_mutex_unlock(&usersMutex);
  return 1;
}

//thread function to handle client connection, login and communicate with the client
void* handleClient(void* arg) {
  ClientArgs* clientArgs = arg;
  int clientSocket = clientArgs->socket;
  User* users = clientArgs->users;
  free(clientArgs);

  if (sendAll(clientSocket, "Ready\n", 6) < 0) {
    close(clientSocket);
    return NULL;
  }

  char buffer[1024];
  char username[16];
  char password[16];
  int lineResult = receiveLine(clientSocket, username, sizeof(username));
  if (lineResult != 1 || receiveLine(clientSocket, password, sizeof(password)) != 1) {
    close(clientSocket);
    return NULL;
  }

  int authResult = authenticateOrRegister(users, username, password);
  const char* authResponse = authResult == 1 ? "Success\n" : "Error\n";
  if (authResult < 0) perror("authenticate/register user");
  if (sendAll(clientSocket, authResponse, strlen(authResponse)) < 0 || authResult != 1) {
    close(clientSocket);
    return NULL;
  }

  if (sendAll(clientSocket, "Ready\n", 6) < 0) {
    close(clientSocket);
    return NULL;
  }

  while (1) {
    lineResult = receiveLine(clientSocket, buffer, sizeof(buffer));
    if (lineResult == 0) break;
    if (lineResult < 0) {
      if (errno != 0) perror("recv client command");
      break;
    }

    size_t commandLength = strlen(buffer);
    buffer[commandLength++] = '\n';
    if (sendAll(clientSocket, buffer, commandLength) < 0) {
      if (errno != EPIPE && errno != ECONNRESET) perror("send");
      break;
    }
  }

  close(clientSocket);
  return NULL;
}

int main(int argc, char* argv[]){
  if(argc!=2){
    printf("Usage: ./server <gameLength>");
    return 1;
  }
  User u[1024];
  loadUsers(u);
  printUsers(u);

  //random generation of the map
  Map map;
  generateRandomMap(map);
  cleanUpMap(map);
  outputMap(map);

  char* end;
  errno = 0;
  long gameLength = strtol(argv[1], &end, 10);
  if (errno != 0 || *argv[1] == '\0' || *end != '\0' || gameLength < 0) {
    fprintf(stderr, "Invalid game length: %s\n", argv[1]);
    return 1;
  }

  //create and bind socket to port
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {
    perror("socket");
    return 1;
  }

  //SO_REUSEADDR allows fast connections after restarts/crashes
  int opt = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt");
    close(serverSocket);
    return 1;
  }

  //bind socket to port and accept every incoming connection
  struct sockaddr_in serverAddr={0};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  if(bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
    perror("bind");
    close(serverSocket);
    return 1;
  }

  //listen queue of 5
  if(listen(serverSocket, 5) < 0){
    perror("listen");
    close(serverSocket);
    return 1;
  }

  //set game length and users in a struct to pass to the game thread
  GameArgs* gameArgs = malloc(sizeof(*gameArgs));
  gameArgs->users = u;
  gameArgs->gameLength = (int)gameLength;

  //create a new thread to run the game
  pthread_t gameThread;
  int threadError = pthread_create(&gameThread, NULL, runGame, gameArgs);
  if(threadError != 0){
    fprintf(stderr, "pthread_create: %s\n", strerror(threadError));
    free(gameArgs);
    close(serverSocket);
    return 1;
  }
  threadError = pthread_detach(gameThread); //detach allows the thread to return without needing a join
  if(threadError != 0){
    fprintf(stderr, "pthread_detach: %s\n", strerror(threadError));
    pthread_join(gameThread, NULL);
    close(serverSocket);
    return 1;
  }

  //create attributes and set them to detached
  pthread_attr_t clientThreadAttributes;
  threadError = pthread_attr_init(&clientThreadAttributes);
  if (threadError != 0) {
    fprintf(stderr, "pthread_attr_init: %s\n", strerror(threadError));
    close(serverSocket);
    return 1;
  }
  threadError = pthread_attr_setdetachstate(&clientThreadAttributes, PTHREAD_CREATE_DETACHED);
  if (threadError != 0) {
    fprintf(stderr, "pthread_attr_setdetachstate: %s\n", strerror(threadError));
    pthread_attr_destroy(&clientThreadAttributes);
    close(serverSocket);
    return 1;
  }

  //main loop to accept clients and create a new thread for each client
  printf("Listening for clients on port %d\n", PORT);
  while(1){
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket < 0) {
      perror("accept");
      break;
    }

    ClientArgs* clientArgs = malloc(sizeof(*clientArgs));
    clientArgs->socket = clientSocket;
    clientArgs->users = u;

    //actually create the thread and handle the client
    pthread_t clientThread;
    threadError = pthread_create(&clientThread, &clientThreadAttributes, handleClient, clientArgs);
    if (threadError != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(threadError));
      close(clientSocket);
      free(clientArgs);
      continue;
    }
    printf("Client connected\n");
  }

  //cleanup and close the server socket
  pthread_attr_destroy(&clientThreadAttributes);
  close(serverSocket);
  return 0;
}