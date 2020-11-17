#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ROW_COUNT 10
#define COL_COUNT 10
#define BOARD_SZ ROW_COUNT * COL_COUNT

using namespace std;

int main() 
{
    int choice;
    while (printf("0 to read, 1 to write, 2 to exit\n"), scanf("%d", &choice), choice != 2)
    {
        if (choice == 0) 
        {
            int fd = open("/dev/minesweeper", O_RDONLY);
            if(fd < 0)
                return !printf("No device found! fd = %d\n", fd);

            char buf[255];
            ssize_t read_count = read(fd, buf, BOARD_SZ);
            if(read_count >= 0)
            {
                printf("count = %d\n", read_count);
                for (int i = 0; i < read_count; i++)
                {
                    if (i != 0 && i % COL_COUNT == 0)
                        printf("\n");
                    printf("%c", buf[i]);
                }
                printf("\n");
            }
            else
                printf("Read failed.\n");

            close(fd);   
        }
        else
        {
            int fd = open("/dev/minesweeper", O_WRONLY);
            if(fd < 0)
                return !printf("No device found! fd = %d\n", fd);

            int i, j;
            printf("type coord i, j: ");
            scanf("%d%d", &i, &j);
            string play = to_string(i * COL_COUNT + j);
            ssize_t count = write(fd, play.c_str(), play.size());
            if(count >= 0)
                printf("count = %d\n", count);
            else
                printf("Write failed.\n");

            close(fd);   
        }
    }
}