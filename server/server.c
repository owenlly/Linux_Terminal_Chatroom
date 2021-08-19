#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

pthread_t thread;
int sockfd; //服务器socket
static int clientNumber;
char IP[15],PORT[10]="10222";

typedef struct sockaddr SA;
typedef struct {
    int fd;
    char name[30];
    pthread_t thread;
    struct sockaddr_in add;
    socklen_t len;
} user_t;
user_t user[100];

/* ***********************************函数申明************************************* */
//功能发送函数部分
void emoji(char* name, int fd); //表情包功能 最后更新：4.29
void listMember(int fd); //输出成员列表功能 最后更新：5.11
void send_file(char* name, int fd);//tcp传文件 最后更新：5.17
void download(char* name, int fd);//tcp下载文件 最后更新：5.17
void send_to(char *name, int fd);//私发消息 最后更新：5.11
void message(char *name, int fd);//查看聊天记录
void object_detection(char* name, int fd, char *file);//目标识别 最后更新：5.18
void func(int fd); //查看功能
void write_blog(char *name, int fd); //写博客 最后更新：5.18
void read_blog(char *name, int fd); //读博客 最后更新：5.19
//...其余待开发

//基础部分函数
void sig_handler(int signo); //signal的处理函数，功能是关闭套接字
int checkName(char* name); //检查重名的函数
void SendMsgToAll(char* msg, int exception); //群发, 除了exception，默认为-1，即所有人都收得到
void* Receive(void* userStruct); //处理接收信息的线程函数
void init(); //初始化
void start(); //开始服务

/* ***********************************主函数************************************* */
int main(){
    printf("输入端口号（按回车为默认10222号端口）");
    fgets(PORT, sizeof(PORT), stdin);
    if(PORT[0] == '\n'){
        strcpy(PORT, "10222\n");
    }
    PORT[strlen(PORT) - 1] = '\0';
    init();
    start();
    pthread_join(thread, NULL);
    close(sockfd);
    return 0;
}

/* **********************************功能发送函数具体实现************************************* */
void emoji(char* name, int fd)
{
    char* emo_value[3] = {"给了你一个迷之微笑", "哭了", "表示很无语"};
    char choiceList[100];
    char choice[3];
    char emojiMsg[100];
    char errorMsg[50];
    int choiceNumber;
    time_t current_time;//获得当前时间
    memset(choiceList, 0, sizeof(choiceList));
    memset(choice, 0, sizeof(choice));
    memset(emojiMsg, 0, sizeof(emojiMsg));
    sprintf(choiceList, "选择您要发送的表情包（输入序号）：\n1.迷之微笑\n2.哭了\n3.无语\n");
    send(fd, choiceList, strlen(choiceList), 0);
    recv(fd, choice, sizeof(choice), 0);
    choice[strlen(choice) - 1] = '\0';
    choiceNumber = atoi(choice);
    if (choiceNumber <=3 && choiceNumber >=1){
        sprintf(emojiMsg, "%s%s\n",name ,emo_value[choiceNumber - 1]);
        SendMsgToAll(emojiMsg, fd);
        time(&current_time);//获取时间
        FILE *fp;
        if ((fp = fopen(".message","a+")) == NULL) {//打开文件
            perror("创建聊天记录失败\n");
        }
        fputs(ctime(&current_time), fp);
        fputs(emojiMsg, fp);
        fclose(fp);
    } else {
        send(fd, "请重新输入\"\\emoji\"命令选择序号！\n", strlen("请重新输入\"\\emoji\"命令选择序号！\n"), 0);
    }
}

void func(int fd){
    send(fd, "功能命令如下：\n", strlen("功能命令如下：\n"), 0);
    char *fun[10] = {"查看功能命令:\t\t\\fun\n","退出聊天室:\t\t\\exit\n","发送表情:\t\t\\emoji\n","查看聊天室成员:\t\t\\member\n",
                    "私聊:\t\t\t\\send_to\n","上传文件:\t\t\\send_file\n","下载文件:\t\t\\download\n",
                    "查看聊天记录:\t\t\\message\n","写博客:\t\t\t\\write_blog\n","读博客:\t\t\t\\read_blog\n"};
    for (int i = 0; i < 10; ++i) {
        send(fd, fun[i], strlen(fun[i]), 0);
    }
}

void object_detection(char* name, int fd, char* file) {
    char command1[100], command2[100], command3[100];
    sprintf(command1, "cp %s ./.yolo", file);
    system(command1);
    chdir("./.yolo");
    sprintf(command2, "./darknet detect cfg/yolov3-tiny.cfg yolov3-tiny.weights %s", file);
    system(command2);
    system("cp predictions.jpg ..");
    sprintf(command3, "rm -f %s", file);
    system(command3);
    send(fd, "目标识别已完成，请下载predictions.jpg获取识别结果\n", strlen("\n目标识别已完成，请下载predictions.jpg获取识别结果\n"), 0);
    chdir("..");
}

void listMember(int fd){
    char listMsg[50];
    int index = 1;
    for (int i = 0; i < 100; ++i) {
        if (user[i].fd == fd){
            memset(listMsg, 0, sizeof(listMsg));
            sprintf(listMsg, "%d.%s（您）\n", index++, user[i].name);
            send(fd, listMsg, strlen(listMsg), 0);
        } else if (user[i].fd != -1) {
            memset(listMsg, 0, sizeof(listMsg));
            sprintf(listMsg, "%d.%s\n", index++, user[i].name);
            send(fd, listMsg, strlen(listMsg), 0);
        }
    }
}

void send_file(char* name, int fd)
{
    char buf[100] = {0};
    char file[100] = {0};//接受文件名
    int recv_len;//图片每一个batch的接收长度
    int write_len;
    FILE *fp;
    clock_t start, end;

    send(fd, "请输入您想发送的文件\n", strlen("请输入您想发送的文件\n"), 0);

    //改动点5.17
    recv(fd, file, 100, 0);
    if(strcmp(file, "FAIL") == 0)
        return;

    //debug
    printf("文件%s传输中...", file);
    fp = fopen(file, "w");

    start = clock();
    while (recv_len = recv(fd, buf, 100, 0)) {
        if(recv_len < 0) {
            printf("文件接收失败\n");
            break;
        }
        printf("#");
        //改动点5.23
        int i = 0;
        for(i = 0; i < recv_len; i++){
            if(strcmp(&buf[i], "OK!") == 0)
                break;
        }
        //debug
        // printf("\nrecv_len = %d\n", recv_len);
        // printf("\ni = %d\n", i);

        write_len = fwrite(buf, sizeof(char), (i < recv_len)? i : recv_len, fp);
        if(i < recv_len){//改动点5.18 bug
            end = clock();
            printf("\nClient = %d上传文件完成，耗时%fs\n", fd, (double)(end - start)/CLOCKS_PER_SEC);
            fclose(fp);

            int j = 0;
            for(j = 0; file[j] != '\0'; j++);
            if(strcmp(&file[j-3],"jpg") == 0){//.jpg的目标识别
                //改动点5.18-目标识别
                send(fd, "是否要进行图片的目标识别？(yes/no)\n", strlen("是否要进行图片的目标识别？(yes/no)\n"),0);
                bzero(buf, 100);
                recv(fd, buf, 100, 0);
                if(strcmp(buf, "yes\n") == 0)
                    object_detection(name, fd, file);
            }

            return;
        }
        if (write_len < recv_len) {
            printf("文件拷贝出现错误\n");
            break;
        }
        bzero(buf, 100);
    }


}

void download(char* name, int fd)
{
    char buf[100] = {0};
    char file[100] = {0};
    char *file_p;//指向file的指针
    char file_list[100] = {0};//可供下载的文件
    int send_len;//图片每一个batch的发送长度
    int read_len;
    FILE *fp;
    clock_t start, end;

    send(fd, "请输入您想下载的文件\n", strlen("请输入您想下载的文件\n"), 0);

    system("ls > .file_list.txt");
    fp = fopen(".file_list.txt", "r");
    fread(file_list, sizeof(char), 100, fp);
    send(fd, file_list, strlen(file_list), 0);
    fclose(fp);
    system("rm -f .file_list.txt");

    recv(fd, file, 100, 0);
    for(file_p = file; *file_p != '\n'; file_p++);	//暴力检查文件字符串最后的\n，不这样改找不到文件
    *file_p = '\0';
    if ((fp = fopen(file,"r")) == NULL) {//打开文件
        send(fd, "找不到该下载文件\n", strlen("找不到该下载文件\n"), 0);
        perror("找不到该下载文件\n");
        return;
    }
    send(fd, "DL", strlen("DL"), 0);//传输开始的标志

    recv(fd, buf, 100, 0);

    //改动点5.17
    send(fd, file, 100, 0);//发送文件名
    bzero(buf, 100);
    start = clock();
    while ((read_len = fread(buf, sizeof(char), 100, fp)) > 0 ) {
        send_len = send(fd, buf, read_len, 0);
        if ( send_len < 0 ) {
            send(fd, "发送文件失败\n", strlen("发送文件失败\n"), 0);
            perror("发送文件失败\n");
            return;
        }
        bzero(buf, 100);
    }
    fclose(fp);
    end = clock();
    send(fd, "OK!", strlen("OK!"), 0);//传输完成的标志
    printf("\n%s传输至Client = %d完成，耗时%fs\n", file, fd, (double)(end - start)/CLOCKS_PER_SEC);

}

void send_to(char *name, int fd){
    char choice[5], Msg[100], SendtoMsg[150];
    int choiceNumber, i, index = 0;
    memset(choice, 0, sizeof(choice));
    memset(Msg, 0, sizeof(Msg));
    memset(SendtoMsg, 0, sizeof(SendtoMsg));
    send(fd, "请选择您的私聊对象（输入序号）\n", strlen("请选择您的私聊对象（输入序号）\n"), 0);
    listMember(fd);
    recv(fd, choice, sizeof(choice), 0);
    choice[strlen(choice) - 1] = '\0';
    choiceNumber = atoi(choice);
    send(fd, "输入您要发送的内容\n", strlen("输入您要发送的内容\n"), 0);
    recv(fd, Msg, sizeof(Msg), 0);
    for (i = 0; i < 100; ++i) {
        if (user[i].fd != -1) index++;
        if (index == choiceNumber) break;
    }
    if (choiceNumber <= clientNumber && choiceNumber >= 1){
        sprintf(SendtoMsg,"%s对你说：%s",name, Msg);
        send(user[i].fd, SendtoMsg, strlen(SendtoMsg), 0);
    } else {
        send(fd, "用户不存在！请重新输入\"\\send_to\"命令选择序号！\n", strlen("用户不存在！请重新输入\"\\send_to\"命令选择序号！\n"), 0);
    }
}

void message(char *name, int fd){
    int read_len, send_len;
    char buf[100] = {0};
    char *margin = "****************************\n";//为了显示好看
    FILE *fp;

    if ((fp = fopen(".message","r")) == NULL) {//打开文件
        send(fd, "读取聊天记录失败\n", strlen("读取聊天记录失败\n"), 0);
        perror("读取聊天记录失败\n");
        return;
    }
    //debug
    printf("%s读取聊天记录中\n", name);
    send(fd, margin, strlen(margin), 0);
    while ((read_len = fread(buf, sizeof(char), 100, fp)) > 0 ) {

        send_len = send(fd, buf, read_len, 0);
        if ( send_len < 0 ) {
            send(fd, "发送聊天记录失败\n", strlen("发送聊天记录失败\n"), 0);
            perror("发送聊天记录失败\n");
            return;
        }
        bzero(buf, 100);
    }
    send(fd, margin, strlen(margin), 0);
    send_len = send(fd, "聊天记录显示完毕\n", strlen("聊天记录显示完毕\n"), 0);
    fclose(fp);
    return;
}

void write_blog(char *name, int fd){
    FILE *blogFp;
    FILE *readFp;
    char title[50],prompt[150],filename[50],blogInfo[150],context[100], choice[5],command[50];
    int flag = 1;
    time_t currentTime;
    memset(title, 0, sizeof(title));
    memset(prompt, 0, sizeof(prompt));
    memset(filename, 0, sizeof(filename));
    memset(context, 0, sizeof(context));
    memset(choice, 0, sizeof(choice));
    memset(command, 0, sizeof(command));
    send(fd, "欢迎来到小霸王博客，请给您的博客起一个标题吧：\n",
            sizeof("欢迎来到小霸王博客，请给您的博客起一个标题吧：\n"), 0);
    recv(fd, title, sizeof(title), 0);
    title[strlen(title) - 1] = '\0';
    system("mkdir .blog");
    sprintf(command,"mkdir ./.blog/%s",name);
    system(command);
    sprintf(filename,"./.blog/%s/%s.txt",name,title);
    blogFp = fopen(filename,"w");
    time(&currentTime);

    send(fd, "谁可以看？(输入序号)\n1.所有人可见\n2.仅自己可见\n",
            strlen("谁可以看？(输入序号)\n1.所有人可见\n2.仅自己可见\n"), 0);
    recv(fd, choice, sizeof(choice), 0);
    if (strcmp(choice, "1\n") == 0) {
        sprintf(blogInfo, "$all\n标题：%s\n作者：%s\n日期：%s正文：\n", title, name, ctime(&currentTime));
        fputs(blogInfo, blogFp);
        sprintf(prompt, "请输入《%s》的内容：（输入\\finish结束）\n", title);
        send(fd, prompt, strlen(prompt), 0);
        while (flag) {
            recv(fd, context, sizeof(context), 0);
            if (strcmp(context, "\\finish\n") == 0) {
                flag = 0;
            } else fputs(context, blogFp);
            memset(context, 0, sizeof(context));
        }
        fclose(blogFp);

        readFp = fopen(filename, "r");

        char Msg[100];
        memset(Msg, 0, sizeof(Msg));
        sprintf(Msg, "%s上传了一篇博客《%s》，请用\"\\read_blog\"命令查看详细内容。\n", name,title);
        SendMsgToAll(Msg, fd);

        memset(Msg, 0, sizeof(Msg));
        sprintf(Msg, "您上传了一篇博客《%s》，请用\"\\read_blog\"命令查看详细内容。\n",title);
        send(fd, Msg, strlen(Msg),0);

    } else if (strcmp(choice, "2\n") == 0) {
        sprintf(blogInfo, "$private%s\n标题：%s\n作者：%s\n日期：%s正文：\n", name, title, name, ctime(&currentTime));
        fputs(blogInfo, blogFp);
        sprintf(prompt, "请输入《%s》的内容：（输入\\finish结束）\n", title);
        send(fd, prompt, strlen(prompt), 0);
        while (flag) {
            recv(fd, context, sizeof(context), 0);
            if (strcmp(context, "\\finish\n") == 0) {
                flag = 0;
            } else fputs(context, blogFp);
            memset(context, 0, sizeof(context));
        }
        fclose(blogFp);

        char Msg[100];
        memset(Msg, 0, sizeof(Msg));
        sprintf(Msg, "您上传了一篇博客《%s》，请用\"\\read_blog\"命令查看详细内容。\n",title);
        send(fd, Msg, strlen(Msg),0);
    }
}

void read_blog(char *name, int fd){
    char choice[5],command[50];
    int choiceNumber,i,index=0;
    FILE *comStatus;
    memset(choice, 0, sizeof(choice));
    send(fd, "请选择您要查看的用户：(输入序号)\n", strlen("请选择您要查看的用户：(输入序号)\n"), 0);
    listMember(fd);
    recv(fd, choice, sizeof(choice), 0);
    choice[strlen(choice) - 1] = '\0';
    choiceNumber = atoi(choice);
    for (i = 0; i < 100; ++i) {
        if (user[i].fd != -1) index++;
        if (index == choiceNumber) break;
    }
    sprintf(command,"ls ./.blog/%s/",user[i].name);
    char blogList[100];
    memset(blogList, 0, sizeof(blogList));
    comStatus = popen(command, "r");
    fread(blogList, sizeof(char), sizeof(blogList), comStatus);
    if (blogList[0] == '\0') {
        send(fd, "该用户还没有发过博客哦！\n",strlen("该用户还没有发过博客哦！\n"), 0);
        pclose(comStatus);
        return;
    } else {
        char title[50],fileName[50];
        memset(title,0, sizeof(title));
        memset(fileName,0, sizeof(fileName));
        send(fd, "输入您要查看的博客：(不需要带后缀)\n",strlen("输入您要查看的博客：(不需要带后缀)\n"), 0);
        send(fd, blogList, strlen(blogList), 0);
        recv(fd, title, sizeof(title), 0);
        title[strlen(title) - 1] = '\0';
        pclose(comStatus);
        FILE *readFp;
        sprintf(fileName,"./.blog/%s/%s.txt",user[i].name,title);
        readFp = fopen(fileName,"r");
        if (readFp == NULL){
            send(fd, "博客不存在！请重新输入命令\"\\read_blog查找！\"\n",
                    strlen("博客不存在！请重新输入命令\"\\read_blog查找！\"\n"), 0);
            return;
        } else {
            char buf[1024],Msg[50],check[50];
            memset(buf, 0, sizeof(buf));
            memset(Msg, 0, sizeof(Msg));
            memset(check, 0, sizeof(check));
            sprintf(check, "$private%s\n", name);
            fgets(buf, sizeof(buf), readFp);
            if (buf[1]=='p' && strcmp(check,buf) != 0) {
                send(fd, "这是一篇私密博客，您无权访问！\n", sizeof("这是一篇私密博客，您无权访问！\n"),0);
            } else {
                memset(buf, 0, sizeof(buf));
                fread(buf, sizeof(char), sizeof(buf), readFp);
                sprintf(Msg, "《%s》内容如下:\n", title);
                send(fd, Msg, strlen(Msg), 0);
                send(fd, buf, strlen(buf), 0);
                char like[10];
                memset(like, 0, sizeof(like));
                send(fd, "喜欢的话就点个赞吧！（输入like点赞，回车跳过）\n",
                        strlen("喜欢的话就点个赞吧！（输入like点赞，回车跳过）\n"),0);
                recv(fd, like, sizeof(like), 0);
                if (strcmp(like, "like\n") == 0) {
                    char likeMsg[50];
                    sprintf(likeMsg, "%s点赞了您的博客《%s》！\n",name, title);
                    send(user[i].fd, likeMsg, strlen(likeMsg), 0);
                }
            }
        }
        fclose(readFp);
    }
}

/* **********************************基础部分函数具体实现************************************* */
void sig_handler(int signo)
{
    if (signo == SIGINT){
        printf("服务器关闭！\n");
        close(sockfd);
        exit(1);
    }
}

int checkName(char* name){
    for (int i = 0; i < 100; ++i) {
        if (user[i].fd != -1)
            if (strcmp(user[i].name, name) == 0)
                return 1;
    }
    return 0;
}

/*改动点4.25
client发出的消息不会回显给自己*/
void SendMsgToAll(char* msg, int exception){
    for(int i = 0; i < 100; ++i)
        if(user[i].fd != exception && user[i].fd != -1){
            send (user[i].fd, msg, strlen(msg), 0);
        }
    if (exception == -1){
        printf("服务器群发完成！\n");
    } else {
        printf("服务器群发完成！除了Clientfd = %d\n", exception);
    }
}

void start(){
    int i;
    clientNumber = 0;
    char nameRecv[50]; //缓冲区用来接收用户名
    while(1){
        for (i = 0; i < 100; ++i) {
            if (user[i].fd == -1) break;
        }
        //步骤1：连接客户端
        user[i].len = sizeof(user[i].add);
        if (clientNumber< 100){ //如果客户端数未满一百
            user[i].fd = accept(sockfd,
                                (SA*)&user[i].add, &user[i].len);
            if (user[i].fd < 0){
                perror("客户端连接出错");
                continue;
            }
            printf("连接到客户端 Clientfd = %d\n", user[i].fd);
        } else  //如果满了一百
            break;


        //步骤2：接收用户名并查重
        memset(nameRecv, 0, sizeof(nameRecv)); //初始化缓冲区
        recv(user[i].fd, nameRecv, sizeof(nameRecv), 0);

        if (checkName(nameRecv) == 1){
            send(user[i].fd, "change", 6, 0);
            close(user[i].fd);
            user[i].fd = -1;
        } else {
            clientNumber++;
            strcpy(user[i].name, nameRecv);
            char successEnter[100];
            memset(successEnter, 0, sizeof(successEnter));
            sprintf(successEnter, "%s, 欢迎进入聊天室！\n", user[i].name);
            send(user[i].fd, successEnter, sizeof(successEnter), 0);
            char sum[100], enter[100];
            sprintf(enter, "欢迎%s进入聊天室！\n", user[i].name);
            SendMsgToAll(enter, user[i].fd);
            sprintf(sum, "聊天室现在共有%d人！\n", clientNumber);
            SendMsgToAll(sum, -1);

            //步骤3：创建处理接收信息的线程，主线程则继续接收客户端。
            pthread_create(&user[i].thread, 0, Receive, &user[i]);
        }
    }
}

void* Receive(void* userStruct){
    user_t* userInfo = (user_t *)userStruct;
    char receiver[100]; //接收信息缓冲区
    int receiverEnd;
    int ret;

    time_t current_time;//获得当前时间

    // FILE *fp;//存聊天记录
    // char *chat_record = ".message";
    // if ((fp = fopen(chat_record,"w+")) == NULL) {//打开文件
    //     perror("打开文件失败\n");
    // }
    system("rm -f .message");

    while (1){
        memset(receiver, 0, sizeof(receiver));
        receiverEnd = recv(userInfo->fd, receiver, sizeof(receiver), 0);

        if (receiverEnd == 0){
            char exit[100],sum1[100];
            sprintf(exit, "%s退出了聊天室\n", userInfo->name);
            SendMsgToAll(exit, userInfo->fd);
            clientNumber--;
            sprintf(sum1, "聊天室现在共有%d人！\n", clientNumber);
            SendMsgToAll(sum1, userInfo->fd);
            printf("检测到客户端退出 fd = %d\n",userInfo->fd);
            close(userInfo->fd);
            char command[50];
            memset(userInfo, 0, sizeof(user_t));
            userInfo->fd = -1;
            pthread_exit(&ret);
        } else if (receiverEnd < 0){
            perror("接收出错！");
            continue;
        } else if (strlen(receiver) > 0){ //限制条件：接收到的信息长度大于0，防止一些意外出现。。。调了半天发现这里的问题
                if(strcmp(receiver, "\\exit\n") == 0){ //接收到客户端退出信息
                    char exit[100],sum1[100];
                    sprintf(exit, "%s退出了聊天室\n", userInfo->name);
                    SendMsgToAll(exit, userInfo->fd);
                    clientNumber--;
                    sprintf(sum1, "聊天室现在共有%d人！\n", clientNumber);
                    SendMsgToAll(sum1, userInfo->fd);
                    printf("检测到客户端退出 fd = %d\n",userInfo->fd);
                    close(userInfo->fd);
                    char command[50];
                    memset(userInfo, 0, sizeof(user_t));
                    userInfo->fd = -1;
                    pthread_exit(&ret);
                } else if (strcmp(receiver, "\\emoji\n") == 0){ //接收到客户端要发送表情包的信息
                    emoji(userInfo->name, userInfo->fd);
                } else if (strcmp(receiver, "\\member\n") == 0) { //列出成员列表
                    char memberMsg[50];
                    memset(memberMsg, 0, sizeof(memberMsg));
                    sprintf(memberMsg, "现在的聊天室成员有：\n");
                    send(userInfo->fd, memberMsg, strlen(memberMsg), 0);
                    listMember(userInfo->fd);
                } else if (strcmp(receiver, "\\send_file\n") == 0) { //tcp传文件
                    send_file(userInfo->name, userInfo->fd);
                } else if (strcmp(receiver, "\\download\n") == 0) { //tcp下载
                    download(userInfo->name, userInfo->fd);
                } else if (strcmp(receiver, "\\send_to\n") == 0) {
                    send_to(userInfo->name, userInfo->fd); //私发消息
                } else if (strcmp(receiver, "\\message\n") == 0) { //查看聊天记录
                    message(userInfo->name, userInfo->fd);
                } else if (strcmp(receiver, "\\fun\n") == 0) {
                    func(userInfo->fd);
                } else if (strcmp(receiver,"\\write_blog\n") == 0){
                    write_blog(userInfo->name, userInfo->fd);
                } else if (strcmp(receiver,"\\read_blog\n") == 0) {
                    read_blog(userInfo->name, userInfo->fd);
                }
                else { //默认群发
                    char allMsg[150];
                    time(&current_time);//获取时间
                    sprintf(allMsg, "%s对所有人说：%s", userInfo->name, receiver);
                    SendMsgToAll(allMsg, userInfo->fd);
                    //记录在聊天记录中
                    // fwrite(ctime(&current_time), sizeof(char), strlen(ctime(&current_time)), fp);
                    // fwrite(allMsg, sizeof(char), strlen(allMsg), fp);
                    FILE *fp;
                    if ((fp = fopen(".message","a+")) == NULL) {//打开文件
                        perror("创建聊天记录失败\n");
                    }
                    fputs(ctime(&current_time), fp);
                    fputs(allMsg, fp);
                    memset(allMsg, 0, sizeof(allMsg));
                    fclose(fp);
                }
            }
        
    }
    pthread_exit(0);
}

void init(){
    struct sockaddr_in addr;

    //用ctrl+c进行退出服务器
    if (signal(SIGINT, sig_handler) == SIG_ERR){ //SIGINT关联ctrl+c
        perror("signal sigint error");
        exit(1);
    }

    //步骤1：创建套接字并填入地址信息
    sockfd = socket(PF_INET,SOCK_STREAM,0);
    if (sockfd < 0){
        perror("创建socket失败");
        exit(-1);
    }
    addr.sin_family = PF_INET;
    addr.sin_port = htons(atoi(PORT));
    addr.sin_addr.s_addr = INADDR_ANY; //设备可能有多网卡，接收所有可能的地址

    //之前debug一直碰到绑定失败的情况，它由TCP套接字状态TIME_WAIT引起。该状态在套接字关闭后约保留2到4分钟
    //网上给出的解决办法如下
    int on = 1;
    int s = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (s < 0){
        perror("setsockopt");
        exit(-1);
    }

    //步骤2：绑定信息
    if (bind(sockfd,(SA*)&addr,sizeof(addr)) < 0){
        perror("绑定失败");
        exit(-1);
    }

    //步骤3：监听
    if (listen(sockfd,100) < 0){
        perror("设置监听失败");
        exit(-1);
    }
    printf("服务器启动监听...\n");

    for (int i = 0; i < 100; ++i) {
        user[i].fd = -1;
    }
}





