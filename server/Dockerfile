FROM ubuntu:latest
RUN apt-get update && apt-get install -y gcc

WORKDIR /dir
COPY server/server.c .
COPY user.data .
RUN gcc -Wall -Wextra -pthread server.c -o server.elf
CMD ["./server.elf", "120"]