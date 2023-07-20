#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

/*-------------------------
通过阅读ls.c可以获得文件系统的大致处理逻辑如下：
1.有一个文件句柄fd，通过执行open函数可以获取其句柄
2.执行fstat可以获取fd句柄指向的文件信息
3.执行read可以获取fd中的dirent序列，序列的详细信息在下方注释，dirent结构中就有name
---------------------------*/

void
_find(char *path, char *target)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    /*----------------------------------------------------------
    文件在硬件上是以数据序列形式并列存储的，而文件目录的树形结构是通过各种形式实现的链接来完成的。
    在根目录之下，存在一个dirent结构的序列，亦即一个有序的集合，这个集合可以从根目录的文件句柄中，用read进行遍历读取。
    并且，这个集合总共有66个dirent结构，包括64个可以存储文件或者文件目录的dirent结构，
    以及1个标示当前目录和1个标示上一层目录的dirent结构。而当我们创建了a文件夹之后，直接占用已经初始化的但未被使用的dirent；
    在a的目录下写入b文件，系统向存储设备申请一块新的空间写入b文件并将b文件挂到a目录之下。
    -------------------------------------------------------------*/
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){//用fstat从句柄fd中读取当前路径的stat，相应的文件信息保存在st中
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    strcpy(buf, path);//对输入的参数path进行拼接，指针p指向了最后的斜杠
    p = buf+strlen(buf);
    *p++ = '/';

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, ".."))
            //de.inum==0表示这是一块已经初始化并且可以用来创建文件或者文件夹的位置，但是没有文件存在在这个位置上，所以要跳过
            //剩余两个strcmp判断语句的原因是：
            //如果是一个目录，由于在xv6的文件系统中一个目录下属的文件中含有本目录.和上级目录.. ，因此我们也要将这两个目录忽视并往下继续运行。
            continue;
        memmove(p, de.name, DIRSIZ);//拼接de.name到buf末尾，获得fd指向的目录下的一个文件完整路径
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){
            printf("ls: cannot stat %s\n", buf);
            continue;
        }
        switch(st.type){
            case T_FILE:
                if(!strcmp(de.name,target)){
                    printf("%s\n", buf);
                }
                break;
            case T_DIR:
                _find(buf,target);
                break;
        }
    }
    close(fd);
}

int
main(int argc, char *argv[]){
    if (argc != 3) {
        fprintf(2, "args error. \n");
        exit(1);
    }
    
    _find(argv[1], argv[2]);
    exit(0);
}