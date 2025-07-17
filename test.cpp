#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

int main() {
    struct stat fileStat;
    const char* path = "www/test.html";
    if (open(path, O_RDONLY))
    {std::cout << "good\n";}

    if (stat(path, &fileStat) == 0) {
        printf("File size: %lld bytes\n", (long long)fileStat.st_size);
        printf("Last modified: %ld\n", fileStat.st_mtime);
        printf("Permissions: %o\n", fileStat.st_mode & 0777);
    } else {
        perror("stat");
    }

    return 0;
}