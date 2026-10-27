#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>  

#define WALL 0
#define PATH 1
#define SIZE 35
#define PORT 5050

struct map {
  int map[SIZE][SIZE];
  int ownerShip[SIZE][SIZE];
};
typedef struct map Map;

struct user {
  int id;
  int x;
  int y;
  char name[16];
  char password[16];
  int score;
  int localMap[SIZE][SIZE];
};
typedef struct user User;

void revealAround(User *u, const Map *m) {
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++) {
      int nx = u->x + dx, ny = u->y + dy;
      if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE)
        u->localMap[nx][ny] = m->map[nx][ny];
    }
}

int generateRandomMap(Map mapIn) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      // Randomly assign WALL or PATH
      mapIn.map[i][j] = (rand() % 2 == 0) ? WALL : PATH;
    }
  }
  return 0;
}

int cleanUpMap(Map mapIn) {
  //Check for any isolated PATH cells and convert them to WALL
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

void printUsers(User* u){
  for(int i=0; i<id; i++){
    printf("%d %s %s\n", u[i].id, u[i].name, u[i].password);
  }
}

void startGame(User* u, Map mapIn, int gameLength){
  printf("Game started for %d seconds\n", gameLength);
  sleep(gameLength);
  printf("Game ended\n");
  for(int i=0; i<id; i++){
    printf("User %s scored %d\n", u[i].name, u[i].score);
  }
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

  startGame(u, map, atoi(argv[1]));

  //create and bind socket to port
  int serverSocket;
  struct sockaddr_in serverAddr={0};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);// TODO CHANGE THIS 
  //AND GET A HOSTNAME
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
  int opt=1;
  //Reuseport consente di creare piu socket in listen sulla stessa coppia ip:porta
  setsockopt(serverSocket,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof(opt));

  listen(serverSocket, 5);
  accept(serverSocket, NULL, NULL); //accept connection from client
  printf("Client connected\n");


  return 0;
}