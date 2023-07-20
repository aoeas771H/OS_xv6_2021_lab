#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define WP 1 //定义管道写端为1
#define RP 0 //定义管道读端为0

/*
pipe函数定义中的fd参数是一个大小为2的一个数组类型的指针。该函数成功时返回0，
并将一对打开的文件描述符值填入fd参数指向的数组。失败时返回 -1并设置errno。
通过pipe函数创建的这两个文件描述符 fd[0] 和 fd[1] 分别构成管道的两端，
往 fd[1] 写入的数据可以从 fd[0] 读出。并且 fd[1] 一端只能进行写操作，
fd[0] 一端只能进行读操作，不能反过来使用。要实现双向数据传输，可以使用两个管道。 
默认情况下，这一对文件描述符都是阻塞的。
此时，如果我们用read系统调用来读取一个空的管道，则read将被阻塞，直到管道内有数据可读；
如果我们用write系统调用往一个写满的管道中写数据，则write也将被阻塞，直到管道有足够的空闲空间可用(read读取数据后管道中将清除读走的数据)。
当然，用户可自行将 fd[0] 和 fd[1] 设置为非阻塞的。 
如果管道的写端文件描述符 fd[1] 的引用计数减少至0，即没有任何进程需要往管道中写入数据，
则对该管道的读端文件描述符 fd[0] 的read操作将返回0(管道内不存在数据的情况)，即读到了文件结束标记(EOF，End Of File)；
反之，如果管道的读端文件描述符 fd[0] 的引用计数减少至0，即没有任何进程需要从管道读取数据，
则针对该管道的写端文件描述符 fd[1] 的write操作将失败，并引发SIGPIPE信号(往读端被关闭的管道或socket连接中写数据)。
*/

int
main(int argc, char *argv[]){
    char data = 'm';
    int pip_c_p[2],pip_p_c[2];
    pipe(pip_c_p);
    pipe(pip_p_c);
    int pid = fork(), exit_status = 0; //fork函数相当于创建了另一个子进程，其进程的寄存器，代码段等信息跟父进程相同
    //如何理解一次调用两次返回？原因就在于创建另一个进程之后OS会进行进程调度，两次返回不在同一个进程中返回
    if(pid < 0){ //创建进程失败
        fprintf(2,"fork faild.");
        close(pip_c_p[WP]);
        close(pip_c_p[RP]);
        close(pip_p_c[WP]);
        close(pip_p_c[RP]);
        exit(-1);
    }
    else if(pid == 0){ //子进程接收管道消息并打印
    //在读取数据前，需要先关闭管道的写端，否则会出现管道中没有数据导致读取无限等待的情况。
        close(pip_p_c[WP]);
        close(pip_c_p[RP]);
        if (read(pip_p_c[RP], &data, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child read() error!\n");
            exit_status = 1; //标记出错
        } 
        else {
            fprintf(1, "%d: received ping\n", getpid());
        }

        if (write(pip_c_p[WP], &data, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }

        close(pip_p_c[RP]);
        close(pip_c_p[WP]);

        exit(exit_status);
    } else { //父进程
        close(pip_p_c[RP]);
        close(pip_c_p[WP]);

        if (write(pip_p_c[WP], &data, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent write() error!\n");
            exit_status = 1;
        }

        if (read(pip_c_p[RP], &data, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent read() error!\n");
            exit_status = 1; //标记出错
        } 
        else {
            fprintf(1, "%d: received pong\n", getpid());
        }

        close(pip_p_c[WP]);
        close(pip_c_p[RP]);

        exit(exit_status);
    }
}