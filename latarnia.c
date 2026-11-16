#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(const char *name)
{
    fprintf(stderr, "USAGE: %s <plik_dziennika> <fifo_czujnika>\n", name);
    exit(EXIT_FAILURE);
}
int IsFifo(int fd) {
    struct stat fs;
    fstat(fd,&fs);
    if (S_ISFIFO(fs.st_mode)) return 1;
    return 0;
}
int CreateRaport(char* raportPath) {
    int fd = open(raportPath,O_RDWR | O_CREAT | O_EXCL| O_APPEND,0644);
    if (fd==-1) {
        if (errno==EEXIST) {
            fd = open(raportPath,O_RDWR | O_APPEND);
        }
        else {
            ERR("open");
        }
    }
    return fd;
}
int CreateFifo(char* fifoPath) {
    int fd = open(fifoPath,O_RDWR | O_NONBLOCK);
    if (fd==-1) {
        ERR("open");
    }
    if (!IsFifo(fd)) ERR("open");
    return fd;
}
void ReadFifo(int fifoFd, int dziennikFd) {
    char buf[1]; int r; char dopisz[2]; dopisz[1] = '\n';
    while (1) {
        if ((r = read(fifoFd,buf,sizeof(buf)))==0) {
            break;
        }
        if (r == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            ERR("read");
        }
        if (buf[0]== 'F' || buf[0] == 'W' || buf[0] == 'S') {
            dopisz[0] = buf[0];
            if (write(dziennikFd,dopisz,2)==-1) ERR("write");
        }
    }
}
void ShowRaport(int dziennikFd) {
    int size = lseek(dziennikFd,0,SEEK_END);
    if (size==0) {
        printf("Raport: F=0 W=0 S=0 | bajty=0\n");
        return;
    }
    char*m = mmap(NULL,size,PROT_READ,MAP_SHARED,dziennikFd,0);
    if (m == MAP_FAILED) ERR("mmap");
    int F =0; int W = 0; int S = 0;
    for (int i=0;i<size;i++) {
        switch (m[i]) {
        case 'W': W++;
            break;
        case 'S': S++;
            break;
        case 'F': F++;
            break;
        default:;
        }
    }
    printf("Raport: F=%d W=%d S=%d | bajty=%d\n",F,W,S,size);
    munmap(m,size);
}
void Listen(struct pollfd fds[2], int dziennikFd) {
    while (1) {
        int ret = poll(fds,2,5000);
        if (ret==-1) {
            if (errno == EINTR) {
                continue;
            }
            ERR("poll");
        }
        if (ret==0) {
            printf("Nasluch...\n");
        }
            if (fds[0].fd == -1) {
                continue;
            }
            if (fds[0].revents & POLLIN) {
                ReadFifo(fds[0].fd,dziennikFd);
            }
            if (fds[1].revents & POLLIN) {
                char c;
                if (read(STDIN_FILENO,&c,1) ==-1) ERR("read");
                if (c=='s') {
                    ShowRaport(dziennikFd);
                }
                if (c=='q') {
                    return;
                }
        }
    }
}
int main(int argc, char **argv)
{
    if (argc != 3)
        usage(argv[0]);
    int raportFd = CreateRaport(argv[1]);
    struct pollfd fds[2];
    fds[0].fd = CreateFifo(argv[2]); fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;; fds[1].events = POLLIN;
    Listen(fds,raportFd);
    close(raportFd);
    close(fds[0].fd);
    return EXIT_SUCCESS;
}
