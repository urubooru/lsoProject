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
#include <time.h>
#include <sys/select.h>

#define WALL 0
#define PATH 1
#define SIZE 20
#define PORT 5050

const char newLine = '\n';

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
  Map *map;
  int gameLength;
};
typedef struct gameArgs GameArgs;

struct clientArgs {
  int socket;
  User *users;
  Map *map;
};
typedef struct clientArgs ClientArgs;

//way better than using 4 flags for each direction
void revealAround(User* u, const Map* m) {
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++) {
      int nx = u->x + dx, ny = u->y + dy;
      if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE)
        u->localMap[nx][ny] = m->map[nx][ny];
    }
}

//completely random generation of the map, with a 50% chance of being a wall or a path
int generateRandomMap(Map* mapOut) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      // Randomly assign WALL or PATH
      mapOut->map[i][j] = (rand() % 2 == 0) ? WALL : PATH;
    }
  }
  return 0;
}

//Check for any isolated PATH cells and convert them to WALL
int cleanUpMap(Map* mapIn) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if (mapIn->map[i][j] == PATH) {
        int wallCount = 0;
        // Check adjacent cells
        if (i > 0 && mapIn->map[i - 1][j] == WALL) wallCount++; // Up
        if (i < SIZE - 1 && mapIn->map[i + 1][j] == WALL) wallCount++; // Down
        if (j > 0 && mapIn->map[i][j - 1] == WALL) wallCount++; // Left
        if (j < SIZE - 1 && mapIn->map[i][j + 1] == WALL) wallCount++; // Right
        if (wallCount == 4) {
          mapIn->map[i][j] = WALL; // Convert isolated PATH to WALL
        }
      }
    }
  }
  return 0;
}
// simple function to output the map to the console, for debugging purposes
int outputMap(const Map* mapIn) {
  const char X = 'X';
  const char O = 'O';
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if (mapIn->map[i][j] == WALL) {
        write(STDOUT_FILENO, &X, sizeof(char));
        //printf("X"); // Represent WALL with X
      } else {
        write(STDOUT_FILENO, &O, sizeof(char));
        //printf("O"); // Represent PATH with O
      }
    }
    write(STDOUT_FILENO, &newLine, sizeof(char));
    //printf("\n");
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
      memset(&tmpUsr, 0, sizeof(tmpUsr));
      strcpy(tmpUsr.name, name);
      strcpy(tmpUsr.password, password);
      tmpUsr.id=id;
      tmpUsr.isConnected=0;
      u[id]=tmpUsr;
      for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++)
          u[id].localMap[x][y] = -1;
      char buf1[]="Loaded user: ";
      write(STDOUT_FILENO, buf1, sizeof(buf1));
      write(STDOUT_FILENO, name, strlen(name));
      write(STDOUT_FILENO, &newLine, sizeof(char));
      //printf("Loaded user: %s\n", name);
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


int activePlayers = 0;
int gameOver = 0;
int winnerId = -1;
int winnerScore = 0;
pthread_mutex_t usersMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gameFinishedCondition = PTHREAD_COND_INITIALIZER;
pthread_cond_t activePlayersCondition = PTHREAD_COND_INITIALIZER;
//create a mutex and 2 conditional variables
void startGame(User* users, const Map* map, int gameLength) {
  struct timespec deadline;
  if(clock_gettime(CLOCK_REALTIME, &deadline) < 0){ //close game if we cant retrieve current time
    perror("clock_gettime game deadline");
    pthread_mutex_lock(&usersMutex);
    for (int i = 0; i < id; i++) users[i].score = 0;
    for(int x = 0; x < SIZE; x++){
      for (int y = 0; y < SIZE; y++) {
        int owner = map->ownerShip[x][y];
        if (owner >= 0 && owner < id) users[owner].score++;
      }
    }
    gameOver = 1;
    pthread_cond_broadcast(&gameFinishedCondition);
    pthread_mutex_unlock(&usersMutex);
    return;
  }
  deadline.tv_sec += gameLength;

  pthread_mutex_lock(&usersMutex);
  while(!gameOver){
    int waitResult = pthread_cond_timedwait(&gameFinishedCondition, &usersMutex, &deadline);
    if (waitResult == ETIMEDOUT) break;
    if (waitResult != 0) {
      char buf2[]="Loaded user: ";
      write(STDERR_FILENO, buf2, sizeof(buf2));
      write(STDERR_FILENO, strerror(waitResult), strlen(strerror(waitResult)));
      write(STDERR_FILENO, &newLine, sizeof(char));
      //fprintf(stderr, "pthread_cond_timedwait: %s\n", strerror(waitResult));
      break;
    }
  }

  for (int i = 0; i < id; i++) users[i].score = 0;
  for (int x = 0; x < SIZE; x++) {
    for (int y = 0; y < SIZE; y++) {
      int owner = map->ownerShip[x][y];
      if (owner >= 0 && owner < id) users[owner].score++;
    }
  }

  winnerId = -1;
  winnerScore = 0;
  int tied = 0;
  for (int i = 0; i < id; i++) {
    if (users[i].score > winnerScore) {
      winnerId = i;
      winnerScore = users[i].score;
      tied = 0;
    } else if (users[i].score == winnerScore && users[i].score > 0) {
      tied = 1;
    }
  }
  if (tied) winnerId = -1;
  gameOver = 1;
  pthread_cond_broadcast(&gameFinishedCondition);
  pthread_mutex_unlock(&usersMutex);
}
//thread function to run the game in a separate thread
void* runGame(void* arg) {
  GameArgs* gameArgs = arg;
  startGame(gameArgs->users, gameArgs->map, gameArgs->gameLength);
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

//function to send local information 
int sendLocalInfo(int socket, User* user, const Map* map) {
  int localMap[SIZE][SIZE];

  pthread_mutex_lock(&usersMutex);
  revealAround(user, map);
  memcpy(localMap, user->localMap, sizeof(localMap));
  int playerX = user->x;
  int playerY = user->y;
  pthread_mutex_unlock(&usersMutex);

  char buffer[SIZE * 12 + 32];
  //format the buffer to write LOCAL $SIZE with a format string
  int length = snprintf(buffer, sizeof(buffer), "LOCAL %d\n", SIZE);
  if (length < 0 || (size_t)length >= sizeof(buffer) || sendAll(socket, buffer, (size_t)length) < 0) return -1;

  for (int x = 0; x < SIZE; x++) {
    memcpy(buffer, "MAPROW ", 7);
    for (int y = 0; y < SIZE; y++) {
      if(x == playerX && y == playerY){
        buffer[7+y] = '@';
      } else if(localMap[x][y]==WALL){
        buffer[7+y] = 'X';
      } else if(localMap[x][y]==PATH){
        buffer[7+y] = 'O';
      } else{
        buffer[7+y] = '#';
      }
    }
    buffer[7 + SIZE] = '\n';
    if (sendAll(socket, buffer, 8 + SIZE) < 0) return -1;
  }

  return sendAll(socket, "END\n", 4);
}

//function to send global info to all users
int sendGlobalInfo(int socket, User* users, User* user, const Map* map) {
  struct playerSnapshot {
    int id;
    int x;
    int y;
    char name[sizeof(user->name)];
  } players[1024];
  int ownerShip[SIZE][SIZE];
  int playerCount = 0;

  pthread_mutex_lock(&usersMutex);
  memcpy(ownerShip, map->ownerShip, sizeof(ownerShip));
  for (int i = 0; i < id; i++) {
    if (!users[i].isConnected) continue;
    players[playerCount].id = users[i].id;
    players[playerCount].x = users[i].x;
    players[playerCount].y = users[i].y;
    memcpy(players[playerCount].name, users[i].name, sizeof(players[playerCount].name));
    playerCount++;
  }
  pthread_mutex_unlock(&usersMutex);

  char buffer[SIZE * 12 + 32];
  //format with a formatstring to insert in buffer GLOBAL $SIZE $playercount
  int length = snprintf(buffer, sizeof(buffer), "GLOBAL %d %d\n", SIZE, playerCount);
  if (length < 0 || (size_t)length >= sizeof(buffer) || sendAll(socket, buffer, (size_t)length) < 0) return -1;

  for (int x = 0; x < SIZE; x++) {
    memcpy(buffer, "OWNROW ", 7);
    for (int y = 0; y < SIZE; y++) {
      int owner = ownerShip[x][y];
      if(owner<0){
        buffer[7+y]='.';
      } else if(owner<10){
        buffer[7+y]=(char)('0'+owner);
      } else{
        buffer[7+y]='*';
      }
    }
    buffer[7 + SIZE] = '\n';
    if (sendAll(socket, buffer, 8 + SIZE) < 0) return -1;
  }

  for (int i = 0; i < playerCount; i++) {
    //format the string to tell PLAYER $ID $X $Y $NAME
    length = snprintf(buffer, sizeof(buffer), "PLAYER %d %d %d %s\n", players[i].id, players[i].x, players[i].y, players[i].name);
    if (length < 0 || (size_t)length >= sizeof(buffer) || sendAll(socket, buffer, (size_t)length) < 0) return -1;
  }

  return sendAll(socket, "END\n", 4);
}

//function to place a player in the map, can place him only if the place is not occupied
int placePlayer(User* user, User* users, Map* map) {
  for (int offset = 0; offset < SIZE * SIZE; offset++) {
    int cell = (rand() + offset) % (SIZE * SIZE);
    int x = cell / SIZE;
    int y = cell % SIZE;
    if (map->map[x][y] != PATH) continue;

    int occupied = 0;
    for (int i = 0; i < id; i++) {
      if (&users[i] != user && users[i].isConnected && users[i].x == x && users[i].y == y) {
        occupied = 1;
        break;
      }
    }
    if (occupied) continue;

    user->x = x;
    user->y = y;
    map->ownerShip[x][y] = user->id;
    user->isConnected = 1;
    return 0;
  }
  return -1;
}

//function to move the player, using a switch case, returns 1 if correct or 0 in case of error/path blocked
int movePlayer(User* user, const char* command, Map* map) {
  int x = user->x;
  int y = user->y;
  switch (command[0]) {
    case 'w': case 'W': x--; break;
    case 's': case 'S': x++; break;
    case 'a': case 'A': y--; break;
    case 'd': case 'D': y++; break;
    default: return 0;
  }
  if (command[1] != '\0' || x < 0 || x >= SIZE || y < 0 || y >= SIZE || map->map[x][y] != PATH) return 0;

  user->x = x;
  user->y = y;
  map->ownerShip[x][y] = user->id;
  return 1;
}

//send game results via sendall after having determined the winner
int sendGameResult(int socket, User* users, User* user) {
  int finalScore;
  int finalWinnerId;
  int finalWinnerScore;
  char winnerName[sizeof(user->name)] = "none";

  pthread_mutex_lock(&usersMutex);
  finalScore = user->score;
  finalWinnerId = winnerId;
  finalWinnerScore = winnerScore;
  if (winnerId >= 0 && winnerId < id)
    memcpy(winnerName, users[winnerId].name, sizeof(winnerName));
  pthread_mutex_unlock(&usersMutex);

  char message[128];
  //format the string to be equal to GAMEOVER $WinnerID $WinnerScore $FinalScore $WinnerName
  int length = snprintf(message, sizeof(message), "GAMEOVER %d %d %d %s\n", finalWinnerId, finalWinnerScore, finalScore, winnerName);
  if (length < 0 || (size_t)length >= sizeof(message)) return -1;
  return sendAll(socket, message, (size_t)length);
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

//simple function to check if a user exists, and if not, create a new user with the given name and password
int authenticateOrRegister(User* users, const char* name, const char* password) {
  if (name[0] == '\0' || password[0] == '\0' || strchr(name, ' ') != NULL || strchr(password, ' ') != NULL)
    return 0;

  int result = 0;
  pthread_mutex_lock(&usersMutex);
  if (gameOver) {
    pthread_mutex_unlock(&usersMutex);
    return 0;
  }
  for (int i = 0; i < id; i++) {
    if (strcmp(users[i].name, name) == 0) {
      result = strcmp(users[i].password, password) == 0;
      if (result && users[i].isConnected) result = 0;
      if (result) {
        users[i].isConnected = 1;
        activePlayers++;
      }
      pthread_mutex_unlock(&usersMutex);
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
  for (int x = 0; x < SIZE; x++)
    for (int y = 0; y < SIZE; y++)
      user->localMap[x][y] = -1;
  id++;
  activePlayers++;
  pthread_mutex_unlock(&usersMutex);
  return 1;
}

//function to disconnect a player
void disconnectPlayer(User* user) {
  if (user == NULL) return;
  pthread_mutex_lock(&usersMutex);
  if (user->isConnected){
    user->isConnected = 0;
    activePlayers--;
    pthread_cond_broadcast(&activePlayersCondition);
  }
  pthread_mutex_unlock(&usersMutex);
}

//thread function to handle client connection, login and communicate with the client
void* handleClient(void* arg) {
  ClientArgs* clientArgs = arg;
  int clientSocket = clientArgs->socket;
  User* users = clientArgs->users;
  Map *map = clientArgs->map;
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
    if (authResult == 1) {
      User* authenticatedUser = NULL;
      pthread_mutex_lock(&usersMutex);
      for (int i = 0; i < id; i++) {
        if (strcmp(users[i].name, username) == 0) {
          authenticatedUser = &users[i];
          break;
        }
      }
      pthread_mutex_unlock(&usersMutex);
      disconnectPlayer(authenticatedUser);
    }
    close(clientSocket);
    return NULL;
  }

  //fetch the user that the thread is handling
  User* user = NULL;
  pthread_mutex_lock(&usersMutex);
  for (int i = 0; i < id; i++) {
    if (strcmp(users[i].name, username) == 0) {
      user = &users[i];
      break;
    }
  }

  //place the user
  int placed = user != NULL && placePlayer(user, users, map) == 0;
  pthread_mutex_unlock(&usersMutex);
  if (!placed) {
    if (user == NULL){
      char buf3[]= "Authenticated user not found: ";
      write(STDERR_FILENO, buf3, sizeof(buf3));
      write(STDERR_FILENO, username, strlen(username));
      write(STDERR_FILENO, &newLine, sizeof(char));
      //fprintf(stderr, "Authenticated user not found: %s\n", username);
    }
    disconnectPlayer(user);
    close(clientSocket);
    return NULL;
  }

  if (sendAll(clientSocket, "Ready\n", 6) < 0) {
    disconnectPlayer(user);
    close(clientSocket);
    return NULL;
  }

  if (sendLocalInfo(clientSocket, user, map) < 0 || sendGlobalInfo(clientSocket, users, user, map) < 0) {
    if (errno != EPIPE && errno != ECONNRESET) perror("send initial info");
    disconnectPlayer(user);
    close(clientSocket);
    return NULL;
  }

  //create a timespec structure to handle the global info every 5 seconds
  struct timespec nextMapSend;
  if (clock_gettime(CLOCK_MONOTONIC, &nextMapSend) < 0) {
    perror("clock_gettime");
    disconnectPlayer(user);
    close(clientSocket);
    return NULL;
  }
  nextMapSend.tv_sec += 5;

  while (1) {
    pthread_mutex_lock(&usersMutex);
    int finished = gameOver;
    pthread_mutex_unlock(&usersMutex);
    if(finished){
      if(sendGameResult(clientSocket, users, user) < 0 && errno != EPIPE && errno != ECONNRESET) perror("send game result");
      break;
    }

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
      perror("clock_gettime");
      break;
    }

    //this if only calls if everything has happened before the select
    if(now.tv_sec > nextMapSend.tv_sec || (now.tv_sec == nextMapSend.tv_sec && now.tv_nsec >= nextMapSend.tv_nsec)){
      if(sendGlobalInfo(clientSocket, users, user, map)<0){
        if(errno != EPIPE && errno != ECONNRESET) perror("send global info");
        break;
      }
      if(clock_gettime(CLOCK_MONOTONIC, &nextMapSend)<0){
        perror("clock_gettime");
        break;
      }
      nextMapSend.tv_sec += 5;
      continue;
    }

    //create a timeout window for the select which will let us send global info every 5 secs
    struct timeval timeout = {0};
    timeout.tv_sec = nextMapSend.tv_sec - now.tv_sec;
    if(nextMapSend.tv_nsec >= now.tv_nsec){
      timeout.tv_usec = (nextMapSend.tv_nsec - now.tv_nsec) / 1000;
    } else{
      timeout.tv_sec--;
      timeout.tv_usec = 1000000 + (nextMapSend.tv_nsec - now.tv_nsec) / 1000;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientSocket, &readSet);
    int ready = select(clientSocket + 1, &readSet, NULL, NULL, &timeout);
    if(ready < 0){
      if (errno == EINTR) continue;
      perror("select client socket");
      break;
    }
    //select is ready therefore 5 seconds have passed, send global info
    if (ready == 0) { 
      if (sendGlobalInfo(clientSocket, users, user, map) < 0) {
        if (errno != EPIPE && errno != ECONNRESET) perror("send global info");
        break;
      }
      if (clock_gettime(CLOCK_MONOTONIC, &nextMapSend) < 0) {
        perror("clock_gettime");
        break;
      }
      nextMapSend.tv_sec += 5;
      continue;
    }

    //recieve line command
    lineResult = receiveLine(clientSocket, buffer, sizeof(buffer));
    if (lineResult == 0) break;
    if (lineResult < 0) {
      if (errno != 0) perror("recv client command");
      break;
    }

    
    pthread_mutex_lock(&usersMutex);
    if (gameOver) {
      pthread_mutex_unlock(&usersMutex);
      if(sendGameResult(clientSocket, users, user) < 0 && errno != EPIPE && errno != ECONNRESET)
        perror("send game result");
      break;
    }
    int commandResult;
    commandResult = movePlayer(user, buffer, map);
    pthread_mutex_unlock(&usersMutex);

    //in case of right command write a response using a formatstring or if wrong case specify that instaed
    char response[1024];
    if (commandResult == 1) {
      snprintf(response, sizeof(response), "Moved %c", buffer[0]);
    } else if (strlen(buffer) == 1 && (buffer[0] == 'w' || buffer[0] == 'W' || buffer[0] == 'a' || buffer[0] == 'A' ||
              buffer[0] == 's' || buffer[0] == 'S' || buffer[0] == 'd' || buffer[0] == 'D')) {
      snprintf(response, sizeof(response), "Blocked %c", buffer[0]);
    } else {
      snprintf(response, sizeof(response), "%s", buffer);
    }
    size_t responseLength = strlen(response);
    response[responseLength++] = '\n';
    if (sendAll(clientSocket, response, responseLength) < 0) {
      if (errno != EPIPE && errno != ECONNRESET) perror("send");
      break;
    }
    if (sendLocalInfo(clientSocket, user, map) < 0) {
      if (errno != EPIPE && errno != ECONNRESET) perror("send local info");
      break;
    }
  }

  disconnectPlayer(user);
  close(clientSocket);
  return NULL;
}

int main(int argc, char* argv[]){
  if(argc!=2){
    char output[] = "Usage: ./server <gameLength>";
    write(STDOUT_FILENO, output, sizeof(output));
    //printf("Usage: ./server <gameLength>");
    return 1;
  }
  User u[1024];
  loadUsers(u);
  //printUsers(u);

  //random generation of the map
  Map map;
  srand((unsigned int)time(NULL));
  generateRandomMap(&map);
  cleanUpMap(&map);
  for (int x = 0; x < SIZE; x++)
    for (int y = 0; y < SIZE; y++)
      map.ownerShip[x][y] = -1;
  //outputMap(&map);

  char* end;
  errno = 0;
  long gameLength = strtol(argv[1], &end, 10);
  if (errno != 0 || *argv[1] == '\0' || *end != '\0' ||
      gameLength < 0 || gameLength > INT_MAX) {
    char output[] = "Invalid game length: ";
    write(STDERR_FILENO, output, sizeof(output));
    write(STDERR_FILENO, argv[1], strlen(argv[1]));
    write(STDERR_FILENO, &newLine, sizeof(char));
    //fprintf(stderr, "Invalid game length: %s\n", argv[1]);
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
  if (gameArgs == NULL) {
    perror("malloc game arguments");
    close(serverSocket);
    return 1;
  }
  gameArgs->users = u;
  gameArgs->map = &map;
  gameArgs->gameLength = (int)gameLength;

  //create a new thread to run the game
  pthread_t gameThread;
  int threadError = pthread_create(&gameThread, NULL, runGame, gameArgs);
  if(threadError != 0){
    char output[] = "pthread_create: ";
    write(STDERR_FILENO, output, sizeof(output));
    write(STDERR_FILENO, strerror(threadError), strlen(strerror(threadError)));
    write(STDERR_FILENO, &newLine, sizeof(char));
    //fprintf(stderr, "pthread_create: %s\n", strerror(threadError));
    free(gameArgs);
    close(serverSocket);
    return 1;
  }
  //create attributes and set them to detached
  pthread_attr_t clientThreadAttributes;
  threadError = pthread_attr_init(&clientThreadAttributes);
  if (threadError != 0) {
    char output[] = "pthread_attr_init: ";
    write(STDERR_FILENO, output, sizeof(output));
    write(STDERR_FILENO, strerror(threadError), strlen(strerror(threadError)));
    write(STDERR_FILENO, &newLine, sizeof(char));
    //fprintf(stderr, "pthread_attr_init: %s\n", strerror(threadError));
    close(serverSocket);
    return 1;
  }
  threadError = pthread_attr_setdetachstate(&clientThreadAttributes, PTHREAD_CREATE_DETACHED);
  if (threadError != 0) {
    char output[] = "pthread_attr_setdetachstate: ";
    write(STDERR_FILENO, output, sizeof(output));
    write(STDERR_FILENO, strerror(threadError), strlen(strerror(threadError)));
    write(STDERR_FILENO, &newLine, sizeof(char));
    //fprintf(stderr, "pthread_attr_setdetachstate: %s\n", strerror(threadError));
    pthread_attr_destroy(&clientThreadAttributes);
    close(serverSocket);
    return 1;
  }

  while (1) {
    pthread_mutex_lock(&usersMutex);
    int finished = gameOver;
    pthread_mutex_unlock(&usersMutex);
    if (finished) break;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(serverSocket, &readSet);
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 250000};
    int ready = select(serverSocket + 1, &readSet, NULL, NULL, &timeout);
    if (ready < 0) {
      if (errno == EINTR) continue;
      perror("select server socket");
      break;
    }
    if (ready == 0) continue;

    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket < 0) {
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }

    ClientArgs* clientArgs = malloc(sizeof(*clientArgs));
    if (clientArgs == NULL) {
      perror("malloc client arguments");
      close(clientSocket);
      continue;
    }
    clientArgs->socket = clientSocket;
    clientArgs->users = u;
    clientArgs->map = &map;

    //actually create the thread and handle the client
    pthread_t clientThread;
    threadError = pthread_create(&clientThread, &clientThreadAttributes, handleClient, clientArgs);
    if (threadError != 0) {
      char output[] = "pthread_create: ";
      write(STDERR_FILENO, output, sizeof(output));
      write(STDERR_FILENO, strerror(threadError), strlen(strerror(threadError)));
      write(STDERR_FILENO, &newLine, sizeof(char));
      //fprintf(stderr, "pthread_create: %s\n", strerror(threadError));
      close(clientSocket);
      free(clientArgs);
      continue;
    }
  }

  close(serverSocket);
  pthread_attr_destroy(&clientThreadAttributes);

  pthread_mutex_lock(&usersMutex);
  while (activePlayers > 0) {
    int waitResult = pthread_cond_wait(&activePlayersCondition, &usersMutex);
    if (waitResult != 0) {
      char output[] = "pthread_cond_wait: ";
      write(STDERR_FILENO, output, sizeof(output));
      write(STDERR_FILENO, strerror(threadError), strlen(strerror(threadError)));
      write(STDERR_FILENO, &newLine, sizeof(char));
      //fprintf(stderr, "pthread_cond_wait: %s\n", strerror(waitResult));
      break;
    }
  }
  pthread_mutex_unlock(&usersMutex);

  threadError = pthread_join(gameThread, NULL);
  if (threadError != 0) {
    char output[] = "pthread_join: ";
    write(STDERR_FILENO, output, sizeof(output));
    write(STDERR_FILENO, strerror(threadError), strlen(strerror(threadError)));
    write(STDERR_FILENO, &newLine, sizeof(char));
    //fprintf(stderr, "pthread_join: %s\n", strerror(threadError));
    return 1;
  }
  return 0;
}