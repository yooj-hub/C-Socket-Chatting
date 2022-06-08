#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
void errProc();
// Sig Child에 대한 핸들러
void child_handler();
int main(int argc, char** argv) {
    int clntSd;
    struct sockaddr_in srvAddr;
    int clntAddrLen, readLen, recvByte, maxBuff;
    char wBuff[BUFSIZ];
    char rBuff[BUFSIZ];
    pid_t pid;
    // sigaction 변수 선언
    struct sigaction act;

    // ip address 와 port가 둘다 입력되지 않으면 error
    if (argc != 3) {
        printf("Usage: %s [IP Address] [port]\n", argv[0]);
    }
    // sigaction에 핸들러 등록 및 마스크, 플래그 설정
    act.sa_handler = child_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    // SIGHCHLD에 act 등록
    sigaction(SIGCHLD, &act, 0);

    // socket 생성후 file discriptor를 clntSd에 저장
    clntSd = socket(AF_INET, SOCK_STREAM, 0);
    if (clntSd == -1) errProc();
    printf("==== client program =====\n");

    // clntAddr에 ip, port 정보 저장
    memset(&srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(argv[1]);
    srvAddr.sin_port = htons(atoi(argv[2]));

    // tcp server와 connect
    if (connect(clntSd, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) == -1) {
        close(clntSd);
        errProc();
    }
    pid = fork();

    if (pid == 0) { // child 출력을 담당
        while (1) {
            readLen = read(clntSd, rBuff, sizeof(rBuff) - 1);
            // rBuff의 마지막에 \0을 추가한다.
            // 서버 종료시 종료
            if (readLen == 0) {
                printf("Server Exited\n");
                break;
            }
            rBuff[readLen] = '\0';
            printf("%s\n", rBuff);
        }

    } else
        while (1) { // parent 입력 담당
            // fgets 를 통하여 stdin에 들어온 정보를 wBuff에 저장
            fgets(wBuff, BUFSIZ - 1, stdin);
            readLen = strlen(wBuff);
            if (readLen < 2) continue;
            // server에 wBuff 내용 전송
            wBuff[readLen - 1] = '\0';
            send(clntSd, wBuff, readLen - 1, 0);
            recvByte = 0;
            maxBuff = BUFSIZ - 1;
        }

    close(clntSd);

    return 0;
}

void errProc() {
    fprintf(stderr, "Error :%s\n", strerror(errno));
    exit(errno);
}

void child_handler(int sig) {
    int status;
    // SIGCHLD 발생시 wait
    pid_t cPid = wait(&status);
    // 종료됐으면 종료된 pid 출력
    if (WIFEXITED(status))
        printf("Child Process %d is terminated\n", cPid);
    // 프로그램 종료
    printf("END\n");
    exit(0);
}