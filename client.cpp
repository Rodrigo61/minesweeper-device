#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BOARD_DIM_COUNT 2
#define MAX_BOARD_SZ 255*255
using namespace std;

int board_w;
int board_h;

int read_board(char board[255])
{
    int fd = open("/dev/minesweeper", O_RDONLY);
    if(fd < 0)
        return !printf("No device found! fd = %d\n", fd);

    char buf[MAX_BOARD_SZ + BOARD_DIM_COUNT]; 
    ssize_t read_count = read(fd, buf, MAX_BOARD_SZ + BOARD_DIM_COUNT);
    board_w = (int)buf[0];
    board_h = (int)buf[1];
    copy(buf + BOARD_DIM_COUNT, buf + (board_w * board_h) + BOARD_DIM_COUNT, board);
    close(fd);
    return read_count;
}

void print_board(char buf[255])
{
    unsigned int board_size = board_w * board_h;
    for (char i = 0; i < board_size; i++)
    {
        printf("%c", buf[i]);
        if (((i + 1) % board_w) == 0)
        {
            printf("\n");
        }
    }
    printf("\n");
}

int main()
{
    int choice;
    while (printf("0 to read, 1 to write, 2 to exit\n"), scanf("%d", &choice), choice != 2)
    {
        if (choice == 0) 
        {
            char board[255];
            char board_w, board_h;
            ssize_t read_count = read_board(board);
            printf("(%d, %d)\n", board_w, board_h);
            if(read_count >= 0)
            {
                print_board(board);
            }
            else
                printf("Read failed.\n");  
        }
        else
        {
            int fd = open("/dev/minesweeper", O_WRONLY);
            if(fd < 0)
                return !printf("No device found! fd = %d\n", fd);

            int i, j;
            printf("type coord i, j: ");
            scanf("%d%d", &i, &j);
            string play = to_string(i * board_w + j);
            ssize_t count = write(fd, play.c_str(), play.size());
            if(count >= 0)
                printf("count = %d\n", count);
            else
                printf("Write failed.\n");

            close(fd);   
        }
    }
}