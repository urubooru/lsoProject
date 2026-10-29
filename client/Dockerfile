FROM ubuntu:latest
RUN apt-get update && apt-get install -y gcc

WORKDIR /dir
COPY client/client.c .
RUN gcc -Wall -Wextra client.c -o client.elf
CMD ["./client.elf", "server", "5050"]