// to nie ma prawa dzialac
//
//
//
//
// i#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
int main()
{
    DIR* directory;
    struct dirent* entry;
    if((directory=opendir("/home/systemy/project1/katZrodlowy")) == NULL)
    {
        perror("opendir() error");
    }
    else   
    {
        puts("Content: ");
        while((entry = readdir(directory))!=NULL)
        {
            printf(" %s\n", entry->d_name);
        }
        closedir(directory);
    }
    return 0;
}
