#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if(argc<2)
    {
        fprintf(2, "usage: sleep <ticks>\n");//stderr 2，标准错误，报错、用法提示专用
        exit(1);//0代表正常退出，其他代表有错误

    }

    int ticks=atoi(argv[1]);

    if(sleep(ticks)<0)//返回 0：表示操作成功。返回 -1 (或小于 0 的值)：表示操作失败。
    {
        fprintf(2, "sleep: system call failed\n");
        exit(1);
    }

    exit(0);
}