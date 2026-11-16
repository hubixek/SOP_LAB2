#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mman.h>

int CreateRecipe(char * recipePath) {
    int fd = open(recipePath,O_RDWR | O_CREAT | O_EXCL,0777);
    if (fd==-1) {
        if (errno == EEXIST) {
            int fd = open(recipePath,O_RDWR);
            if (lseek(fd,0,SEEK_END)!=2048) {
                fprintf(stderr,"Ta ksiega to nie przepis!\n");
                exit(EXIT_FAILURE);
            }
            return fd;
        }
        exit(EXIT_FAILURE);
    }
    char fillBuf[64];
    for (int i=0;i<63;i++) {
        fillBuf[i] = ' ';
    }
    fillBuf[63] = '\n';
    for (int i =0;i<32;i++) {
        if (write(fd,fillBuf,sizeof(fillBuf)) == -1) exit(EXIT_FAILURE);
    }
    return fd;
}
int FifoFileDescriptor(char* fifoPath) {
    int fd =  open(fifoPath,O_RDONLY);
    if (fd==-1) {
        fprintf(stderr,"Nie ma babci na wieży %s\n", fifoPath);
        exit(EXIT_FAILURE);
    }
    struct stat fifoStat;
    fstat(fd,&fifoStat);
    if (!S_ISFIFO(fifoStat.st_mode)) {
        fprintf(stderr,"Nie ma babci na wieży %s\n", fifoPath);
        exit(EXIT_FAILURE);
    }
    return fd;
}
int ProcessMessage(char * message, int * n, char*t ) {
    char res[64];
    if (message[2] != ':' || message[0] < '0' || message[0]> '9'
        || message[1] < '0' || message[1]> '9') {
        fprintf(stderr,"Babcia się zagadała: \n");
        return 1;
    }
    char num[3]; num[0] = message[0]; num[1] = message[1]; num[2] = '\0';
    *n = atol(num);
    if (*n <0 || *n > 31) {
        fprintf(stderr,"Babcia się zagadała: \n");
        return 1;
    }
    if (strlen(message)<67) {
        for (int i =3;i<strlen(message);i++) {
            res[i-3] = message[i];
        }
        for (int i = strlen(message)-3;i<64;i++) {
            res[i] = ' ';
        }
    }
    else {
        for (int i = 3;i<67;i++) {
            res[i-3] = message[i];
        }
    }
    res[63] = '\n';
    fprintf(stdout,"Krok %s zapisany\n",num);
    memcpy(t,res,64);
    return 0;
}
void DivideMesseges(int fd, int fileFd) {
    int t; char buf[1]; int length=0;
    char przepis[256]; char proccessedM[64];
    while (1) {
        if ((t = read(fd,buf,sizeof(buf)))==0) {
            break;
        }
        if (t==-1) exit(EXIT_FAILURE);
        if (buf[0] == '\n') {
            przepis[length] = '\0';
            length=0; int n;
            if (ProcessMessage(przepis,&n,proccessedM)==1) continue;
            lseek(fileFd,n*64,SEEK_SET);
            write(fileFd,proccessedM,64);
            continue;
        }
        przepis[length] = buf[0];
        length++;
    }
}
void HandleChunk(char *data, int r, char *msgBuf, int *len, int recipeFd) {
    char line[64];
    int n;
    for (int k = 0; k < r; k++) {
        if (data[k] == '\n') {
            msgBuf[*len] = '\0';
            *len = 0;
            if (ProcessMessage(msgBuf, &n, line) == 1) continue;
            lseek(recipeFd, n * 64, SEEK_SET);
            write(recipeFd, line, 64);
            continue;
        }
        if (*len < 255) {
            msgBuf[*len] = data[k];
            (*len)++;
        }
    }
}
void Listen(struct pollfd fds[5], int count, int recipeFd, char **paths){
    char buffs[5][256]; int lengths[5] = {0};
    int finished = 0;
    while (finished<count) {
        int ret = poll(fds,count,3000);
        if (ret==-1) {
            if (errno == EINTR) {
                continue;
            }
            exit(EXIT_FAILURE);
        }
        if (ret==0) {
            printf("Babcie dolewają herbatki…\n");
        }
        for (int i=0;i<count;i++) {
            if (fds[i].fd==-1) {
                continue;
            }
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                char buf[64];
                int r = read(fds[i].fd, buf, sizeof(buf));
                if (r == -1) exit(EXIT_FAILURE);
                if (r == 0) {
                    close(fds[i].fd);
                    printf("Babcia z wieży %s skończyła dyktować\n", paths[i]);
                    fds[i].fd = -1;
                    finished++;
                    continue;
                }
                HandleChunk(buf, r, buffs[i], &lengths[i], recipeFd);
            }

        }
    }


}
int main(int argc, char*argv[]) {
    if (argc<3 || argc > 7) {
        exit(EXIT_FAILURE);
    }
    int fd = CreateRecipe(argv[1]);
    struct pollfd fds[5];

    for (int i=0;i<argc-2;i++) {
        fds[i].fd = FifoFileDescriptor(argv[i+2]);
        fds[i].events = POLLIN;
    }
    Listen(fds, argc-2, fd, argv+2);

    char* m = mmap(NULL,2048,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
    if (m == MAP_FAILED) exit(EXIT_FAILURE);
    int pomylki=0;
    for (int i=0;i<=2048-11;i++) {
        if (memcmp(m+i,"raw~berries",11)==0) {
            m[i+2] = 's';
            m[i+3] = 'p';
            pomylki++;
        }
    }
    printf("Brudy księżniczki Joanny wyczyszczone: %d\n",pomylki);
    int K=0;
    for (int i=0;i<32;i++) {
        char out[64];
        memcpy(out, m + i*64, 63);
        int len = 63;
        while (len>0 && out[len-1]==' ') len--;
        out[len] = '\0';
        if (len==0) {
            K++;
        }
        else {
            printf("%d: %s\n",i,out);
        }
    }
    if (K>0) {
        printf("Brakuje %d kroków, tort sie zapadnie!\n",K);
    }
    else {
        printf("Tort gotowy!\n");
    }
    munmap(m,2048);
    close(fd);
    exit(EXIT_SUCCESS);
}