#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define W 1
#define R 0

void
_primes(int *pip){
    int get,send;
    int pip_r[2];
    pipe(pip_r);
    //形参pip是从上一个进程，也就是左邻居传输的数据管道，要先取出来一个打印出来
    close(pip[W]);
    if(read(pip[R],&get,sizeof(int)) == 0 || get == 35){
        //read returns zero when the write-side of a pipe is closed.
        close(pip[R]);//write—side关闭说明无数据可读，返回即可
        exit(0);
    }
    printf("prime %d\n",get);//要先打印再fork，因为fork创建进程之后就会在进程之间切换了，否则打印的东西会断断续续
    int pid = fork();
    if(pid < 0){
        //fork创建进程失败
        close(pip[R]);
        close(pip_r[W]);
        close(pip_r[R]);
        fprintf(2,"fork failed\n");
        exit(-1);
    }
    else if(pid > 0){
        //父进程,给子进程传递下一组数
        close(pip_r[R]);
        while(read(pip[R],&send,sizeof(int))){
            if(send % get)
                write(pip_r[W],&send,sizeof(int));
        }
        close(pip_r[W]);
        close(pip[R]);
        wait(0);
        exit(0);
    }
    else{
        _primes(pip_r);
        //exit(0);
        close(pip[R]);//关键！前面读pip数据的时候关掉了write side这里不应该退出，应该将read side关闭！
    }
}

int
main(int argc, char *argv[]){
    int pip[2];//起始的第一个管道
    pipe(pip);
    int pid = fork();
    if(pid < 0){
        //fork创建进程失败
        close(pip[W]);
        close(pip[R]);
        fprintf(2,"fork failed\n");
        exit(-1);
    }
    else if(pid > 0){
        //父进程
        printf("prime 2\n");
        close(pip[R]);
        for(int i=3;i<=35;i++){
            if(i % 2)
                write(pip[W],&i,sizeof(int));
        }
        close(pip[W]);
        wait(0);
    }
    else{//子进程
        _primes(pip);
    }
    exit(0);
}