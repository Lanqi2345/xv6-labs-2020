#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int parent_to_child[2],child_to_parent[2];

    char byte='x';//传递的字符

    //创建管道
    if(pipe(parent_to_child)<0 || pipe(child_to_parent)<0)
    {
        fprintf(2, "pingpong: pipe failed\n");
        exit(1);
    }
    
    int pid=fork();

    if(pid==0)//子进程
    {
        close(parent_to_child[1]); //关闭该通道写端
        close(child_to_parent[0]); //关闭该通道读端

        if(read(parent_to_child[0],&byte,1)!=1)
        {
            fprintf(2, "pingpong: child read failed\n");
            exit(1);
        }

        printf("%d: received ping\n",getpid());

        if(write(child_to_parent[1],&byte,1)!=1)
        {
            fprintf(2, "pingpong: child write failed\n");
            exit(1);
        }
        
        close(parent_to_child[0]);
        close(child_to_parent[1]); 
        exit(0);

    }
    else if(pid>0)//父进程
    {
        close(parent_to_child[0]); //关闭该通道读端
        close(child_to_parent[1]); //关闭该通道写端

        if(write(parent_to_child[1],&byte,1)!=1)
        {
            fprintf(2, "pingpong: parent write failed\n");
            exit(1);
        }

        if(read(child_to_parent[0],&byte,1)!=1)
        {
            fprintf(2, "pingpong: parent read failed\n");
            exit(1);
        }

        printf("%d: received pong\n",getpid());
        
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        wait(0);
        exit(0);

    }
    else
    {
        fprintf(2,"fork error\n");
        exit(1);
    }

}