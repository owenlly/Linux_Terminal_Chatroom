#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define FUN 10
typedef struct sockaddr SA;
char name[30];
int sockfd; //客户端socket
struct sockaddr_in addr;
char IP[15],PORT[10]; //服务器IP、服务端口
char *fun[FUN] = {"查看功能命令:\t\t\\fun","退出聊天室:\t\t\\exit","发送表情:\t\t\\emoji","查看聊天室成员:\t\t\\member",
                "私聊:\t\t\t\\send_to","上传文件:\t\t\\send_file","下载文件:\t\t\\download",
                "查看聊天记录:\t\t\\message","写博客:\t\t\t\\write_blog","读博客:\t\t\t\\read_blog"};
pthread_t threadSend, threadReceive;

/* 客户端功能说明
   表情包：输入命令 \emoji
   查看所有成员：输入命令 \member
   私发消息：输入命令 \sendto 已完成
   退出聊天室：输入命令 \exit
   发送文件：输入命令 \sendfile 已完成
   下载文件：输入命令 \download 已完成
   聊天记录：输入命令 \message 已完成
   查看功能命令：输入命令 \fun
   写博客：输入命令 \write_blog
   读博客：输入命令 \read_blog
*/
/* ***********************************函数申明************************************* */
//功能部分
void Send_file(void *Socket);//用于发送文件
int Download(void *Socket, char *buf);//用于receive线程下载文件
void Send_To(void *Socket);
void write_blog(void *Socket);
void read_blog(void *Socket);
//基础部分
void* Send(void* Socket); //用于发送信息的进程
void* Receive(void* Socket); //用于接收信息的进程
void init(); //初始化
void start(); //开始服务

/* ***********************************主函数************************************* */
int main(){
    //输入服务器ip
    printf("输入服务器IP（按回车为默认127.0.0.1）");
    fgets(IP, sizeof(IP), stdin);
    if(IP[0] == '\n'){
        strcpy(IP, "127.0.0.1\n");
    }
    IP[strlen(IP) - 1] = '\0';

    //输入端口
    printf("输入端口号（按回车为默认10222号端口）");
    fgets(PORT, sizeof(PORT), stdin);
    if(PORT[0] == '\n'){
        strcpy(PORT, "10222\n");
    }
    PORT[strlen(PORT) - 1] = '\0';

    start();
    pthread_join(threadSend, NULL);
    pthread_join(threadReceive, NULL);
    close(sockfd);
    return 0;
}

/* *********************************功能部分函数具体实现************************************* */



/* *********************************基础部分函数具体实现************************************* */
void* Receive(void* Socket)
{
    int *SocketCopy = Socket;
    char Receiver[1024];

    while(1){
        memset(Receiver, 0, sizeof(Receiver)); //初始化接收缓冲区

        int receiverEnd = 0;
        receiverEnd  = recv (*SocketCopy, Receiver, sizeof(Receiver), 0);
        if (receiverEnd < 0) {
            perror("接收出错");
            exit(-1);
        } else if (receiverEnd == 0) {
            perror("服务器已关闭，断开连接");
            exit(-1);
        } else if (Receiver[0] == 'D' && Receiver[1] == 'L'){//下载标识符
            Download(SocketCopy, Receiver);
        }
        else {
            Receiver[receiverEnd] = '\0';
            fputs(Receiver, stdout);
        }
    }
}

void* Send(void* Socket)
{
    char Sender[100];
    int *SocketCopy = Socket;
    while(1)
    {
        memset(Sender, 0, sizeof(Sender)); //初始化发送缓冲区
        fgets(Sender, sizeof(Sender), stdin); //读取需要发送的数据

        if (send(*SocketCopy, Sender, sizeof(Sender), 0) < 0){ //防止发送失败
            perror("发送出错！再试一次！");
            continue;
        }
        else {
            if(strcmp(Sender, "\\exit\n") == 0) //检测到退出聊天室命令
                exit(0);
        }
        if (strcmp(Sender, "\\send_file\n") == 0){
            Send_file(SocketCopy);//调用发送文件的函数
        } else if(strcmp(Sender, "\\send_to\n") == 0){
            Send_To(SocketCopy);
        } else if(strcmp(Sender, "\\write_blog\n") == 0){
            write_blog(SocketCopy);
        } else if(strcmp(Sender, "\\read_blog\n") == 0){
            read_blog(SocketCopy);
        }

    }
}

void read_blog(void *Socket){
    char Sender[5],title[50],like[10];
    int *SocketCopy = Socket;
    memset(Sender, 0, sizeof(Sender));
    memset(title, 0, sizeof(title));
    memset(like, 0, sizeof(like));
    fgets(Sender, sizeof(Sender), stdin); //读取需要发送的数据
    send(*SocketCopy, Sender, strlen(Sender), 0);

    fgets(title, sizeof(title), stdin);
    send(*SocketCopy, title, strlen(title), 0);

    fgets(like, sizeof(like), stdin);
    send(*SocketCopy, like, strlen(like), 0);

}

void write_blog(void * Socket){
    char title[50], context[200], choice[5];
    int *SocketCopy = Socket;
    int fd = *SocketCopy;
    int flag = 1;
    memset(title, 0, sizeof(title));
    memset(choice, 0, sizeof(choice));
    fgets(title, sizeof(title), stdin); //读取需要发送的数据
    send(fd, title, strlen(title), 0);
    fgets(choice, sizeof(choice), stdin); //读取需要发送的数据
    send(fd, choice, strlen(choice), 0);
    while(flag){
        memset(context, 0, sizeof(context));
        fgets(context, sizeof(context), stdin);
        send(fd, context, strlen(context), 0);
        if (strcmp(context, "\\finish\n") == 0) flag = 0;
    }

}

void Send_To(void *Socket){
    char Sender[5];
    int *SocketCopy = Socket;

    memset(Sender, 0, sizeof(Sender));
    fgets(Sender, sizeof(Sender), stdin); //读取需要发送的数据
    send(*SocketCopy, Sender, strlen(Sender), 0);


}



//改动点：5.17
void Send_file(void *Socket){
    char file[100] = {0};
    char buf[100] = {0};
    int *SocketCopy = Socket;
    int read_len;
    int send_len;
    int file_len = 0;//记录文件长度
    FILE *fp;

    fgets(file, sizeof(file), stdin); 
    file[strlen(file) - 1] = '\0';
    if ((fp = fopen(file,"r")) == NULL) {//打开文件
        perror("打开文件失败\n");
        send(*SocketCopy, "FAIL", strlen("FAIL"), 0);
        return;
    }

    //改动点5.17
    send(*SocketCopy, file, 100, 0);//5.17-发送文件名
    bzero(buf, 100);

    while ((read_len = fread(buf, sizeof(char), 100, fp)) > 0 ) {
        send_len = send(*SocketCopy, buf, read_len, 0);
        if ( send_len < 0 ) {
            perror("发送文件失败\n");
            return;
        }
        bzero(buf, 100);
    }
    fclose(fp);
    send(*SocketCopy, "OK!", strlen("OK!"), 0);//传输完成的标志
    printf("文件%s已发送完成\n",file);
}

//改动点：5.5
int Download(void *Socket, char Receiver[100]){
    char file[100] = {0};//记录文件名
    int *SocketCopy = Socket;
    int write_len;
    int recv_len;
    FILE *fp;

    //改动点：5.17
    send(*SocketCopy, "OK", strlen("OK"), 0);//不send一个信息过去，DL和文件内容会阻塞到一起，无法进入下面的while循环

    recv(*SocketCopy, file, 100, 0);

    if ((fp = fopen(file,"w")) == NULL) {//打开文件
        perror("文件创建失败\n");
        return 0;
    }
    while (recv_len = recv(*SocketCopy, Receiver, 100, 0)) {
        if(recv_len < 0) {
            printf("文件下载失败\n");
            break;
        }
        //debug
        printf("#");
        //改动点5.23
        int i = 0;
        for(i = 0; i < recv_len; i++)   //暴力检查传输完成标志
            if(strcmp(&Receiver[i], "OK!") == 0)
                break;

        write_len = fwrite(Receiver, sizeof(char), (i < recv_len)? i : recv_len, fp);
        if(i < recv_len){
            printf("\n文件%s下载完成\n", file);
            fclose(fp);
            return 1;
        }
    

        if (write_len < recv_len) {
            printf("文件拷贝失败\n");
            return 0;
        }
        bzero(Receiver, 100);
    }
    return 1;
}


void init(){

    //步骤1：创建socket并填入地址信息
    sockfd = socket(PF_INET,SOCK_STREAM,0);
    if (sockfd < 0) {
        perror("套接字创建失败！");
        exit(-1);
    }
    addr.sin_family = PF_INET;
    addr.sin_port = htons(atoi(PORT));
    addr.sin_addr.s_addr = inet_addr(IP);

    //步骤2：连接服务器
    if (connect(sockfd,(SA*)&addr,sizeof(addr)) < 0){
        perror("无法连接到服务器!");
        exit(-1);
    }
}

void start(){
    char nameInfo[100]; //用来接收输入名字后服务器的反馈信息
    int flag = 1;
    int i;
    printf("\n欢迎来到由Linux小霸王开发的聊天室程序，enjoy it\n\n");
    printf("我们提供的功能有：\n");
    for (i = 0; i < FUN; i++) puts(fun[i]);//输出功能列表
    printf("\n");
    //步骤1：输入客户端的名字
    memset(nameInfo, 0, sizeof(nameInfo));
    //输入名字并发送给服务器
    while(flag){
        printf("请输入您的名字：");
        fgets(name, sizeof(name), stdin);
        name[strlen(name) - 1] = '\0'; //fgets函数比gets函数安全，同样支持空格，但会把换行符读进去。

        //步骤2：建立连接并发送
        init();
        send(sockfd, name, strlen(name), 0);

        //步骤3：接收服务器的反馈信息到rec
        recv(sockfd, nameInfo, sizeof(nameInfo), 0);
        nameInfo[strlen(nameInfo)] = '\0';

        //判断命名是否成功，若得到refuse，则name清零，继续循环操作输入名字，直到
        if (strcmp(nameInfo, "change") == 0) {
            printf("该名字已存在，尝试换一个吧！\n");
            memset(name, 0 , sizeof(name));
        } else {
            flag = 0;
            fputs(nameInfo, stdout);
            //步骤4：启动收发函数线程
            pthread_create(&threadSend, 0, Send, &sockfd); //专门用于发送信息的线程
            pthread_create(&threadReceive, 0, Receive, &sockfd); //专门用于接收信息的线程
        }
    }
}



