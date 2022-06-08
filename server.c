#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// 10명까지 접속 허용, epoll 이벤트 개수 20개까지
#define MAX_EVENTS 20
#define CLIENTS 10

/**
 * @brief Double Linked List를 위한 구조체
 * prev : 이전 노드
 * nxt : 다음 노드
 * fd : file discriptor
 *
 */
struct Node {
    struct Node *prev;
    struct Node *nxt;
    int fd;
};
/**
 * @brief Node 구조체를 생성하는 함수
 *
 * @return struct Node*
 */
struct Node *create_node(int);

/**
 * @brief Linked List의 마지막에 입력된 int의 fd를 키로 갖는 node를 추가하는 함수
 *
 */
void add_node(struct Node *, int);

/**
 * @brief int 에 해당하는 fd를 갖는 노드를 지우는 함수
 *
 */
void delete_node(struct Node *, int);

/**
 * @brief Linked List에서 해당 int에 해당하는 fd를 가진 노드를 제외하고 전원에게 char * 을 전송하는 함수
 *
 */
void send_all(struct Node *, char *, int);

// clinet의 번호를 저장하는 배열
int conv[100];
// 들어온 순서대로 카운터를 매긴다
int cnt = 1;
// 에러 처리 함수
void errProc(const char *);

/**
 * @brief 해당 fd에 대응하는 번호를 리턴하는 함수
 *
 * @return int
 */
int convert(int);

int main(int argc, char **argv) {
    // listen socket file discriptor 와 connect socket file discriptor 선언
    int listenSd, connectSd;
    // 서버와 클라이언트 주소 구조체 선언
    struct sockaddr_in srvAddr, clntAddr;
    // 클라이언트 addr 길이와 read 버퍼와 send 버퍼 길이 선언
    int clntAddrLen, readLen, sendLen;
    // read버퍼와 send 버퍼 선언
    char rBuff[BUFSIZ], sBuff[BUFSIZ];
    // 반복문에 사용할 i
    int i;
    // Linked List의 head 선언
    struct Node head = *create_node(-1);
    // epoll file discriptor, 발생한 이벤트 개수를 나타내는 read, 읽은 fd를 나타내는 readfd 선언
    int epfd, ready, readfd;
    // epoll_event 구조체 ev, event 처리를위한 events 선언
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    // conv 배열 초기화
    memset(conv, 0, sizeof(conv));
    // 포트번호를 받지 않으면 error
    if (argc != 2) {
        printf("Input Error need Port");
        return -1;
    }

    printf("Server Start...\n");
    // epoll 생성
    epfd = epoll_create(1);
    if (epfd == -1) errProc("epoll_create");
    // socket 생성
    listenSd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSd == -1) errProc("socket");
    // 서버 주소 설정
    memset(&srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(atoi(argv[1]));
    // 소켓 바인드
    if (bind(listenSd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) == -1)
        errProc("bind");
    // CLIENTS개 listen
    if (listen(listenSd, CLIENTS) < 0) errProc("listen");
    // ev를 EPOLLIN 플래그 적용
    ev.events = EPOLLIN;
    // 파일 디스크립터 설정
    ev.data.fd = listenSd;
    // epfd 에 fd 등록
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenSd, &ev) == -1) {
        errProc("epoll_ctl");
    }
    // client 주소 사이즈 저장
    clntAddrLen = sizeof(clntAddr);
    while (1) {
        printf("Monitoring ... \n");
        // epoll에 이벤트가 발생하는 것을 기다린다.
        ready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            else
                errProc("epoll_wait");
        }
        // 이벤트 발생시 ready 까지 events
        for (i = 0; i < ready; i++) {
            // listen socket 일 경우 connection 연결
            if (events[i].data.fd == listenSd) {
                connectSd = accept(listenSd, (struct sockaddr *)&clntAddr, &clntAddrLen);
                if (connectSd == -1) {
                    fprintf(stderr, "Accept Error");
                    continue;
                }
                // 접속에 관한 출력 처리
                // 서버에 출력
                fprintf(stderr, "A client(%d) is connected...\n", convert(connectSd));
                // sBuff에 저장
                sendLen = sprintf(sBuff, "A client(%d) is connected...", convert(connectSd));
                sBuff[sendLen] = '\0';
                // 모든 클라이언트에 전송
                send_all(&head, sBuff, -1);
                // 해당 노드를 Linked List에 추가
                add_node(&head, connectSd);
                // 해당 connect된 소켓의 디스크립터 등록
                ev.data.fd = connectSd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connectSd, &ev) == -1) errProc("epoll_ctl");
            } else { // IO
                // 해당 이벤트를 발생시킨 fd를 저장
                readfd = events[i].data.fd;
                // 읽은 데이터를 rBuff에 저장
                readLen = read(readfd, rBuff, sizeof(rBuff) - 1);
                // 0 일 경우 종료
                if (readLen == 0) {
                    fprintf(stderr, "Client (%d)is Disconnected\n", convert(readfd));
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, readfd, &ev) == -1) {
                        errProc("epoll_ctl");
                    }
                    // 노드 삭제
                    delete_node(&head, readfd);
                    // fd close
                    close(readfd);
                    // 종료 메세지 전달 후 LinkedList에 등록된 모든 노드에 전송
                    sendLen = sprintf(sBuff, "Client (%d)is Disconnected", convert(readfd));
                    sBuff[sendLen] = '\0';
                    send_all(&head, sBuff, -1);
                    continue;
                }
                rBuff[readLen] = '\0';
                // Client의 메세지 전송 및 서버 출력
                printf("Client(%d) : %s\n", convert(events[i].data.fd), rBuff);
                sendLen = sprintf(sBuff, "Client(%d) : %s", convert(events[i].data.fd), rBuff);
                sBuff[sendLen] = '\0';
                send_all(&head, sBuff, events[i].data.fd);
            }
        }
    }
    // 종료시 listen 소켓과 epoll을 닫아줌
    close(listenSd);
    close(epfd);
    return 0;
}

// 예외 처리 함수
void errProc(const char *str) {
    fprintf(stderr, "%s: %s", str, strerror(errno));
    exit(1);
}
// fd에 해당하는 값을 리턴해주는 함수
int convert(int x) {
    if (conv[x] == 0) {
        conv[x] = cnt++;
    }
    return conv[x];
}

// 모든 노드에 메세지를 보내는 함수
void send_all(struct Node *head, char *sBuff, int cid) {
    struct Node *current = head->nxt;
    while (current != NULL) {
        if (cid != current->fd)
            write(current->fd, sBuff, strlen(sBuff));
        current = current->nxt;
    }
}

// 노드 를 생성하는 함수
struct Node *create_node(int fd) {
    struct Node *ret;
    ret = (struct Node *)malloc(sizeof(struct Node));
    ret->prev = NULL;
    ret->nxt = NULL;
    ret->fd = fd;
    return ret;
}

// 재귀를 통해 끝을 찾고 추가하는 함수
void add_node(struct Node *current, int fd) {
    if (current->nxt == NULL) {
        printf("fd(%d) added\n", fd);
        struct Node *tmp = create_node(fd);
        tmp->prev = current;
        tmp->nxt = NULL;
        current->nxt = tmp;
        return;
    }
    add_node(current->nxt, fd);
}

// 재귀를 통해 fd를 가진 노드를 찾고 삭제후 메모리를 반환하는 함수
void delete_node(struct Node *current, int fd) {
    if (current->fd == fd) {
        printf("fd(%d) deleted\n", fd);
        if (current->prev != NULL)
            current->prev->nxt = current->nxt;
        if (current->nxt != NULL)
            current->nxt->prev = current->prev;
        free(current);
        return;
    }
    if (current->nxt == NULL) return;
    delete_node(current->nxt, fd);
}
