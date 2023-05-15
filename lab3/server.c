#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


#define BIND_IP_ADDR "0.0.0.0"
#define BIND_PORT 8000
#define MAX_RECV 10000
#define MAX_FILE 1000000
#define MAX_SEND 5000
#define MAX_EVENT_NUM 200
#define MAX_THREAD_NUM 20
#define MAX_CACHE_NUM 10
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"

//define cache
typedef struct{
    int fd;
    char* content;
    ssize_t len;
}Cache;

static Cache cache[MAX_CACHE_NUM];
static int CACHE_NUM = 0;
//初始化
void cache_init(){
    for(int i = 0;i<MAX_CACHE_NUM;i++){
        cache[i].fd = -1;
        cache[i].content = (char*)malloc(MAX_FILE*sizeof(char));
        cache[i].len = 0;
    }
}


pthread_t pthread_ing[MAX_THREAD_NUM];//16个线程
int epollfdPOOL[MAX_THREAD_NUM];//存储每个线程的epoll文件描述符


//HTTP请求解析函数,用于从客户端套接字中读取请求并解析出请求的文件路径
int parse_request(int clnt_sock, char* req, struct stat *fstatus){//客户端套接字、请求缓冲区、指针指向文件状态结构体
    ssize_t req_len = (ssize_t)0;//the length of the request buffer
    const char end_ind[4] = {'\r', '\n', '\r', '\n'}; 
    int ind = 0;//indentify the end of an HTTP request. once ind reaches 4, end
    for (ssize_t cur_len = read(clnt_sock, req + req_len, (size_t)MAX_RECV - (size_t)req_len);;cur_len = read(clnt_sock, req + req_len, (size_t)MAX_RECV - (size_t)req_len)){
        if (cur_len == (ssize_t)-1){
            if (errno == EINTR) 
                continue;
            else{
                perror("read req at parse_request"); 
                return -2;
            }
        }
        if (cur_len == (ssize_t)0) 
            continue;
        for (int i = req_len; i < req_len + cur_len; ++i)//处理每次read()的部分
            if (req[i] == end_ind[ind]){
                if (++ind == 4) 
                    break;
            }else 
                ind = 0;
        req_len += cur_len;
        if (ind == 4) 
            break;
        if (req_len == MAX_RECV) 
            break;
    }
    //when exceeding MAX_RECV
    if (ind != 4){
        char *temp_buf = malloc(MAX_RECV * sizeof(char)); 
        if (temp_buf ==  NULL){ 
            fprintf(stderr, "malloc temp_buf at parse_request\n"); 
            return -2; 
        }
        for (ssize_t cur_len = read(clnt_sock, temp_buf, (size_t)MAX_RECV);;cur_len = read(clnt_sock, temp_buf, (size_t)MAX_RECV)){
            if (cur_len == (ssize_t)-1){
                if (errno == EINTR) 
                    continue;
                else{ 
                    perror("read temp_buf at parse_request"); 
                    free(temp_buf); 
                    return -2;
                }
            }
            if (cur_len == (ssize_t)0) 
                continue;
            for (int i = 0; i < cur_len; ++i)
                if (temp_buf[i] == end_ind[ind]){
                    if (++ind == 4) 
                        break;
                }else 
                    ind = 0;
            if (ind == 4) 
                break;
        }
        free(temp_buf);
    }
    //判断是否为GET /
    if (req_len < (ssize_t)5 || req[0] != 'G' || req[1] != 'E' || req[2] != 'T' || req[3] != ' ' || req[4] != '/') 
        return -2;
    ssize_t s1 = (ssize_t)3, s2 = (ssize_t)5;
    int k = 0;
    while (s2 < req_len){
        if (req[s2] == ' ') 
            break;
        if (req[s2] == '/'){
            if (req[s2 - (ssize_t)1] == '.' && req[s2 - (ssize_t)2] == '.' && req[s2 - (ssize_t)3] == '/') 
                --k;
            else 
                ++k;
            if (k < 0) 
                return -2; 
        }
        ++s2;
    }
    if (req[s2] == ' '){
        req[s2] = '\0'; 
        req[s1] = '.';
        int req_fd = open(req + s1, O_RDONLY);
        if (req_fd == -1) 
            return -1;//打不开文件就404
        if (stat(req + s1, fstatus) == -1) {
            perror("stat at parse_request");
            return -2;
        } 
        if (!S_ISREG(fstatus->st_mode)) 
            return -2;
        return req_fd;
    }else 
        return -2;
}

size_t WRITE(int fd, const void *buf, size_t count){//文件描述符、buffer指针、要写入缓冲区的字节数
    ssize_t written_len = 0;
    for (ssize_t cur_len = write(fd, buf + written_len, count);; cur_len = write(fd, buf + written_len, count)){
        if (cur_len == -1){
            if (errno == EINTR) 
                continue;
            else{ 
                perror("write at WRITE"); 
                return written_len;
            }
        }
        written_len += cur_len;
        if ((size_t)cur_len == count) 
            break;
        count -= (size_t)cur_len;
    }
    return (size_t)written_len;
}

int checkcache(int fd){
    for(int i = 0;i<CACHE_NUM;i++){
        if(fd == cache[i].fd){
            return i;
        }
    }
    cache[CACHE_NUM++].fd = fd;
    ssize_t read_len = 0;
    while((read_len = read(fd, cache[CACHE_NUM].content, MAX_FILE)) > 0){
        cache[CACHE_NUM].len += read_len;
    }
    return -1;
}

void handle_clnt(int clnt_sock){
    char *req_buf = (char *)malloc(MAX_RECV * sizeof(char));
    if (!req_buf) { 
        fprintf(stderr, "malloc req_buf at handle_clnt\n"); 
        close(clnt_sock); 
        return; 
    }
    char *response = (char *)malloc(MAX_SEND * sizeof(char));
    if (!response) { 
        fprintf(stderr, "malloc response at handle_clnt\n"); 
        free(req_buf); 
        close(clnt_sock); 
        return; 
    }
    struct stat fstatus;
    int req_file_d = parse_request(clnt_sock, req_buf, &fstatus);
    if (req_file_d == -1){//404 status
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_404, (size_t)0);
        size_t response_len = strlen(response); 
        if (WRITE(clnt_sock, response, response_len) != response_len) 
            fprintf(stderr, "write at response 404\n");
    }else if (req_file_d == -2){//500 status
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_500, (size_t)0);
        size_t response_len = strlen(response);
        if (WRITE(clnt_sock, response, response_len) != response_len) 
            fprintf(stderr, "write at response 500\n");
    }else{//200 status
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_200, (size_t)(fstatus.st_size));
        size_t response_len = strlen(response);
        if (WRITE(clnt_sock, response, response_len) != response_len) { 
            fprintf(stderr, "write at response 200\n"); 
            goto END;
        }
        //从cache中检测是否有req_file_fd，如果有直接从cache输出对应的内容，如果没有则添加到cache中
        int i = checkcache(req_file_d);
        if(i == -1){
            off_t offset = 0;
            while (offset < fstatus.st_size){
                ssize_t cur_len = sendfile( clnt_sock, req_file_d , &offset , fstatus.st_size - offset );
                if (cur_len == -1) { 
                    perror("sendfile"); 
                    goto END; 
                }
            }
        }else{
            //WRITE(clnt_sock, cache[i].content, cache[i].len);
            off_t offset = 0;
            while (offset < fstatus.st_size){
                ssize_t cur_len = sendfile( clnt_sock, cache[i].fd , &offset , fstatus.st_size - offset );
                if (cur_len == -1) { 
                    perror("sendfile"); 
                    goto END; 
                }
            }
        }
    }
END:
    close(clnt_sock);
    close(req_file_d);
    free(req_buf);
    free(response);
}

void *handle_epoll(void *no_use){
    pthread_t selfid = pthread_self();
    struct epoll_event *events = (struct epoll_event *)malloc(MAX_EVENT_NUM * sizeof(struct epoll_event));
    if (events == NULL){ 
        fprintf(stderr, "malloc at handle_epoll\n"); 
        return NULL;
    } 
    int epollfd = -1;
    for (int i = 0; i < MAX_THREAD_NUM; ++i){ 
        if (selfid == pthread_ing[i]){
            epollfd = epollfdPOOL[i];
            break;
        }
    }
    if (epollfd == -1) { 
        fprintf(stderr, "epollfd at handle_epoll\n"); 
        return NULL;
    } 
    for (int nfd;;){
        if ((nfd = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1)) == -1) {
            perror("epoll_wait"); 
            continue;
        }
        for (int i = 0; i < nfd; ++i){
            handle_clnt(events[i].data.fd);
        }
    }
    free(events);
    return NULL;
}


int main(){
    cache_init();
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (serv_sock==-1) 
        handle_error("socket at main");
    int FLAG = 1;
    int i = 0;
    int clnt_sock;
    int k = 0;
    struct sockaddr_in clnt_addr; 
    socklen_t clnt_size = sizeof(clnt_addr);
    struct epoll_event EPOLL; 
    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &FLAG, sizeof(FLAG)) == -1) 
        handle_error("setsocketopt at main");
    
    struct sockaddr_in serv_addr; 
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;//IPv4
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);//Address
    serv_addr.sin_port = htons(BIND_PORT);//Port
    
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) 
        handle_error("bind at main");
    if (listen(serv_sock, SOMAXCONN)==-1) 
        handle_error("listen at main");

    signal(SIGPIPE, SIG_IGN);

    pthread_attr_t pthreadattr;
    pthread_attr_init(&pthreadattr);
    pthread_attr_setdetachstate(&pthreadattr,PTHREAD_CREATE_DETACHED);


    while (i < MAX_THREAD_NUM){
        if ((epollfdPOOL[i] = epoll_create1(0)) == -1) 
            handle_error("epoll_create1 at main");
        if (pthread_create(&pthread_ing[i], &pthreadattr, handle_epoll, NULL) != 0) 
            handle_error("pthread_create at main");
        i++;
    }
    do{
        if ((clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_size)) == -1){
            perror("accept at main");
            continue;
        }
        EPOLL.data.fd = clnt_sock;
        EPOLL.events = EPOLLIN | EPOLLONESHOT;
        epoll_ctl(epollfdPOOL[k], EPOLL_CTL_ADD, clnt_sock, &EPOLL);
        k = (k + 1)% MAX_THREAD_NUM;
    }while(1);

    return 0;
}
