#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define WALL 0
#define PATH 1
#define SIZE 20

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

int updateUserMap(User* user, Map map) {
  int left,right,up,down; // 4 flags
  left = right = up = down = 0;

  if(user->x == 0){
    up=0;
  } else if(user->x == SIZE-1){
    down=0;
  }

  if(user->y == 0){
    left=0;
  } else if(user->y == SIZE-1){
    right=0;
  }

  //if chain of doom
  if(left && right && up && down){
    user->localMap[(user->x)-1][(user->y)-1] = map.map[(user->x)-1][(user->y)-1];
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)-1][(user->y)+1] = map.map[(user->x)-1][(user->y)+1];
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
    user->localMap[(user->x)+1][(user->y)-1] = map.map[(user->x)+1][(user->y)-1];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
    user->localMap[(user->x)+1][(user->y)+1] = map.map[(user->x)+1][(user->y)+1];
  } else if(left && right && up && !down){
    user->localMap[(user->x)-1][(user->y)-1] = map.map[(user->x)-1][(user->y)-1];
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)-1][(user->y)+1] = map.map[(user->x)-1][(user->y)+1];
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
  } else if(left && right && !up && down){
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
    user->localMap[(user->x)+1][(user->y)-1] = map.map[(user->x)+1][(user->y)-1];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
    user->localMap[(user->x)+1][(user->y)+1] = map.map[(user->x)+1][(user->y)+1];
  } else if(left && !right && up && down){
    user->localMap[(user->x)-1][(user->y)-1] = map.map[(user->x)-1][(user->y)-1];
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)+1][(user->y)-1] = map.map[(user->x)+1][(user->y)-1];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
  } else if(!left && right && up && down){
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)-1][(user->y)+1] = map.map[(user->x)-1][(user->y)+1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
    user->localMap[(user->x)+1][(user->y)+1] = map.map[(user->x)+1][(user->y)+1];
  } else if(left && right && !up && !down){
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
  } else if(left && !right && up && !down){
    user->localMap[(user->x)-1][(user->y)-1] = map.map[(user->x)-1][(user->y)-1];
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
  } else if(!left && right && up && !down){
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)-1][(user->y)+1] = map.map[(user->x)-1][(user->y)+1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
  } else if(!left && right && !up && down){
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
    user->localMap[(user->x)+1][(user->y)+1] = map.map[(user->x)+1][(user->y)+1];
  } else if(left && !right && !up && down){
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)+1][(user->y)-1] = map.map[(user->x)+1][(user->y)-1];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
  } else if(!left && !right && up && down){
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
  }  // this part of the code is for different sizes and should never be used in the current version of the game
  else if(!left && !right && !up && down){ 
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)+1][(user->y)] = map.map[(user->x)+1][(user->y)];
  } else if(!left && !right && up && !down){
    user->localMap[(user->x)-1][(user->y)] = map.map[(user->x)-1][(user->y)];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
  } else if(!left && right && !up && !down){
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
    user->localMap[(user->x)][(user->y)+1] = map.map[(user->x)][(user->y)+1];
  } else if(left && !right && !up && !down){
    user->localMap[(user->x)][(user->y)-1] = map.map[(user->x)][(user->y)-1];
    user->localMap[(user->x)][(user->y)] = map.map[(user->x)][(user->y)];
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

int main(int argc, char* argv[]){
  Map map;
  //random generation of the map
  generateRandomMap(map);
  //debug code
  outputMap(map);

  //create and bind socket to port 5050
  int serverSocket;
  struct sockaddr_in serverAddr={0};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(5050);
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverSocket = socket(AF_INET, SOCK_STREAM, 0); //TCP>UDP, only text
  bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
  int opt=1;
  setsockopt(serverSocket,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

  return 0;
}