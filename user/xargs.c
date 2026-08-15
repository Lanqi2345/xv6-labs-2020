#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define BUFSIZE 512

void run_command(char* args[])
{
    int pid=fork();

    if(pid<0)
    {
        fprintf(2, "xargs: fork failed\n");
        exit(1);
    }

    if(pid==0)//子进程
    {
        exec(args[0],args);

        //exec成功后不会返回
        fprintf(2, "xargs: exec %s failed\n", args[0]);
        exit(1);
    }

    wait(0);//等待子进程结束

}



//执行每一行
void run_line(char* buffer,int length,char* args[],int base_argc)
{
    int argument_count=base_argc;
    char *p;

    buffer[length]='\0';//设置结束的标志
    p=buffer;

    while(*p!='\0')
    {
        //去掉参数前多余的
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        if (argument_count >= MAXARG - 1) 
        {
            fprintf(2, "xargs: too many arguments\n");
            exit(1);
        }

        args[argument_count++] = p;

        //到这个参数的结尾
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;

        if (*p != '\0')
        {
            *p = '\0';
            p++;
        }
        

    }

    //exec要求参数数组最后一个元素必须是空指针
    args[argument_count] = 0;

    run_command(args);
}

int main(int argc,char *argv[])
{

    if(argc<2)
    {
        fprintf(2,"Usage: xargs command [arguments...]\n");
        exit(1);
    }

    char buffer[BUFSIZE];
    char *args[MAXARG];

    int base_argc=argc-1;//去掉命令里的xargs

    if(base_argc>=MAXARG)
    {
        fprintf(2,"xargs: too many arguments\n");
        exit(1);
    }

    for(int i=0;i<base_argc;i++)
    {
        args[i]=argv[i+1];
    }

    int result;//读到的字符数
    char character;//读到的字符
    int length=0;//当前输入行的长度

    while((result=read(0,&character,1))==1)//从管道里读取的
    {
        if(character=='\n')//有换行符
        {
            run_line(buffer,length,args,base_argc);
            length=0;

        }
        else
        {
            if(length>=BUFSIZE-1)
            {
                fprintf(2,"xargs: input too long\n");
                exit(1);
            }

            buffer[length++]=character;
        }
        
    }

    if (result < 0) {
        fprintf(2, "xargs: read failed\n");
        exit(1);
    }

    if(length>0)//最后一行
    {
        run_line(buffer,length,args,base_argc);
    }

    exit(0);

}