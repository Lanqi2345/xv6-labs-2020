#include "kernel/types.h"
#include "user/user.h"

void sieve(int input_fd)
{
    int prime;
    int number;

    //读第一个数字
    if(read(input_fd,&prime,sizeof(int))==0)// 如果返回0，左边已经关闭写端，流水线结束
    {
        close(input_fd);
        exit(0);
    }

    printf("prime %d\n", prime);

    int next_pipe[2];

    if(pipe(next_pipe) < 0)
    {
        fprintf(2, "primes: pipe failed\n");
        close(input_fd);
        exit(1);
    }

    int pid = fork();

    if(pid < 0) {
        fprintf(2, "primes: fork failed\n");
        close(input_fd);
        close(next_pipe[0]);
        close(next_pipe[1]);
        exit(1);
    }

    if(pid == 0)
    {
        //子进程,读取 next_pipe
        close(next_pipe[1]);

        //子进程关闭上一层管道
        close(input_fd);

        sieve(next_pipe[0]);

    } 
    else
    {
    
        // 只写入next_pipe，不读取
        close(next_pipe[0]);

        // 筛选
        while (read(input_fd, &number, sizeof(int)) == sizeof(int))
        {
            // 不能被当前质数整除的数字传给下一层
            if (number % prime != 0)
            {
                if (write(next_pipe[1], &number, sizeof(int))!= sizeof(int))
                {
                    fprintf(2, "primes: write failed\n");
                    close(input_fd);
                    close(next_pipe[1]);
                    exit(1);
                }
            }
        }

        // 左侧数据读取完毕
        close(input_fd);

        //关闭写端
        close(next_pipe[1]);

        //等待下一层以及后续整个流水线结束
        wait(0);
        exit(0);
    }


}


int main(int argc, char *argv[])
{
    int first_pipe[2];

    if (pipe(first_pipe) < 0)
    {
        fprintf(2, "primes: pipe failed\n");
        exit(1);
    }

    int pid=fork();

    if (pid < 0)
    {
        fprintf(2, "primes: fork failed\n");
        close(first_pipe[0]);
        close(first_pipe[1]);
        exit(1);
    }

    if(pid == 0)
    {
        //只读取
        close(first_pipe[1]);
        sieve(first_pipe[0]);
    } 
    else
    {
        // 筛选
        close(first_pipe[0]);
        for(int i=2;i<=35;i++)
        {
            if (write(first_pipe[1], &i, sizeof(int))!= sizeof(int))
            {
                fprintf(2, "primes: write failed\n");
                close(first_pipe[1]);
                exit(1);
            }

        }

        // 左侧数据读取完毕
        close(first_pipe[1]);

        //等待后续整个流水线结束
        wait(0);
        exit(0);
    }



}