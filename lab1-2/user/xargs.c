#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[]){
    char buf[512];
    int len;
    char* param[MAXARG];
    //如何把输出到终端的数据作为参数读进来？？？read函数的用法？？？
    /*-------------------------------------------------------
    如果read()函数的第一个参数fd为0，表示要从标准输入读取数据。
    在Unix-like系统中，标准输入的文件描述符为0。
    标准输入通常对应于终端或控制台，因此当程序需要读取用户输入时，可以使用read(0, buf, n)来从终端读取数据。
    而由于echo，find最后的输出都是在终端当中，所以可以用read(0,&buf,1)来一个一个的读取数据。
    ---------------------------------------------------------*/
    if(argc < 1 || argc > MAXARG){
        printf("error: xargs too many args or no args");
        exit(-1);
    }
    if(argc > 1){//这种情况对应echo hello | xargs echo bye要将xargs删去，不作为参数，而把后面的echo bye作为参数
        for(int i=1;i<argc;i++){
            param[i - 1] = argv[i];//由于argv[0]="xargs"所以要把这一项去掉
        }
    }
    else{//这种情况对应echo hello | xargs，查找资料：xargs后面的命令默认是echo，xargs等同于xargs echo
    //所以要将xargs替换为echo存入参数列表
        param[0] = "echo";
    }
    while(1){//将”|”前面输出到缓冲区的参数全部读取到param中
        int i;
        for(i=0;;i++){//读取缓冲区当中的数据
            len = read(0,&buf[i],1);
            if(len == 0)
                break;
            if(buf[i] == '\n'){//每次在终端当中读取一行的数据，所以遇到换行符号就break
                buf[i] = 0;
                break;
            }
        }
        if(i==0)break;
        if(argc > 1){
            param[argc - 1] = buf;//直接将前面的参数全部传进来的原因是：exec能够处理带空格的字符串。
        }
        else{
            param[1] = buf;
        }
        if (fork() == 0) {//根据题目要求，利用fork函数让子进程执行命令，主进程等待
            exec(param[0],param);
            //exec函数用法
            /*--------------------------------------------------------
            exec()函数是用于加载并执行一个新的程序的系统调用。它的函数原型如下：
            int exec(char *path, char **argv);
            1.path：要执行的程序的路径名。需要提供完整的文件路径，包括文件名和可执行文件的位置。
            2.argv：一个字符串数组，以空指针（NULL）结尾，用于传递给新程序的命令行参数。argv[0]为新程序的名称。
            3.函数返回值：如果发生错误，exec()函数返回-1，并且errno变量会被设置为适当的错误代码。
            正常情况下，exec()函数执行成功后，不会返回到调用者，因为它会将当前进程的映像替换为新程序的映像。
            4.注意事项：在调用exec()函数之前，通常会使用fork()函数创建一个新的进程，
            然后在子进程中调用exec()加载新程序，以替换原有的进程映像。这样做可以在执行新程序之前保留父进程的状态和环境。
            ----------------------------------------------------------*/
            exit(0);
        } else {
            wait(0);
        }
    }
    exit(0);
}
