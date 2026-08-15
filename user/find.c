#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

//获取路径最后一部分的名称
char* basename(char *path)
{
    char *p;
    p=path+strlen(path);

    while(p>path&&*(p-1)!='/')
        p--;

    return p;
}

void find(char *path,char *target)
{

    int fd;
    char *p;
    char buf[512];
    struct stat st;
    struct dirent de;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    //获取当前路径的文件类型等信息
    if(fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    //检查当前路径最后的名称是否匹配
    if(strcmp(basename(path), target) == 0)
    {
        printf("%s\n", path);
    }

    switch(st.type)
    {
        case T_FILE://普通文件
            break;

        case T_DIR://目录
            if(strlen(path)+1+DIRSIZ+1>sizeof(buf))
            {
                fprintf(2, "find: path too long\n");
                break;
            }
            
            strcpy(buf,path);
            p=buf+strlen(buf);
            *p='/';
            p++;
            
            //读取目录下的每一个文件或目录
            while(read(fd, &de, sizeof(de)) == sizeof(de))
            {
                if(de.inum == 0)
                    continue;

                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = '\0';


                //避免无限递归调用
                if(strcmp(p,".")==0 || strcmp(p,"..")==0)
                    continue;

                //递归调用find函数
                find(buf, target);
            }

            break;

        
        
    }
      
    close(fd);

}

int main(int argc, char *argv[]) 
{
    if(argc!=3)
    {
        fprintf(2,"usage: find <path> <name>\n");
        exit(1);
    }

    find(argv[1],argv[2]);

    exit(0);
}