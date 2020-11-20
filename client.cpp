#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ROW_COUNT 10
#define COL_COUNT 10
#define BOARD_SZ ROW_COUNT * COL_COUNT

using namespace std;

int device_fd;
char buf[255];

/**
 * Read the board from the device and print in the console. Return false if the read failed.
*/
bool print_board() 
{
    ssize_t read_count = read(device_fd, buf, BOARD_SZ);
    if(read_count >= 0)
    {
        printf("-----------\n");
        printf("Minesweeper\n");
        printf("-----------\n");
        for (int i = 0; i < read_count; i++)
        {
            if (i != 0 && i % COL_COUNT == 0)
                printf("\n");
            printf("%c", buf[i]);
        }
        printf("\n");
    }
    else
    {
        printf("Read failed.\n");
        return false;
    }
        
    return true;
}

bool play(int i, int j) 
{
    string play = to_string(i * COL_COUNT + j);
    ssize_t count = write(device_fd, play.c_str(), play.size());
    if(count < 0)
    {
        printf("Write failed.\n");
        return false;
    }

    return true;
}

void open_device()
{
    device_fd = open("/dev/minesweeper", O_RDWR);
    if(device_fd < 0)
    {
        printf("No device found! fd = %d\n", device_fd);
        exit(1);
    }
}

void close_device() 
{
    if (device_fd >= 0)
        close(device_fd);
}

int main() 
{
    open_device();

    while (true)
    {
        if (!print_board())
            break;

        int i, j;
        printf("Type the coord i, j (no comma): ");
        scanf("%d%d", &i, &j);
        
        if (!play(i, j))
            break;
    }

    close_device();
}