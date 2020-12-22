#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BOARD_DIM_COUNT 2
#define MAX_BOARD_SZ 255
#define FULL_PERM_CHAR_DEVICES "cd /dev && ls -l | grep \"^crwxrwxrwx\" | grep -oE '[^ ]+$'"
using namespace std;

int board_w;
int board_h;
int device_fd;
char buf[255];

int read_board(char board[255])
{
    char buf[MAX_BOARD_SZ + BOARD_DIM_COUNT]; 
    ssize_t read_count = read(device_fd, buf, MAX_BOARD_SZ + BOARD_DIM_COUNT);
    board_w = (int)buf[0];
    board_h = (int)buf[1];
    printf("(%d, %d)\n", board_w, board_h);
    copy(buf + BOARD_DIM_COUNT, buf + (board_w * board_h) + BOARD_DIM_COUNT, board);
    return read_count;
}

void print_board(char buf[255])
{
    int board_size = board_w * board_h;
    for (int i = 0; i < board_size; i++)
    {
        printf("%c", buf[i]);
        if (((i + 1) % board_w) == 0)
        {
            printf("\n");
        }
    }
    printf("\n");
}

/**
 * Read the board from the device and print in the console. Return false if the read failed.
*/
bool read_and_print_board() 
{
    char board[255];
    ssize_t read_count = read_board(board);
    if(read_count >= 0)
    {
        printf("-----------\n");
        printf("Minesweeper\n");
        printf("-----------\n");
        print_board(board);
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
    string play = to_string(i * board_w + j);
    printf("%s (%d)\n", play.c_str(), (int)play.size());
    ssize_t count = write(device_fd, play.c_str(), play.size());
    if(count < 0)
    {
        printf("Write failed.\n");
        return false;
    }

    return true;
}

string exec_command(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

string choose_char_device()
{
    string allResults = exec_command(FULL_PERM_CHAR_DEVICES);
    string currentStr = allResults.c_str();
    list<string> possibleDevices;
    stringstream ss(currentStr);
    string line;

    while(getline(ss,line,'\n')){
        possibleDevices.push_back(line);
    }
    
    printf("Choose a registered char device that you want to play on (type the number):\n");
    list<string>::iterator it;
    int count = 1;

    for(it = possibleDevices.begin(); it != possibleDevices.end() ; ++it){
        cout << count << " - " << (*it) << endl;
        ++count;    
    }

    int deviceNumber;
    cin >> deviceNumber;
    it = possibleDevices.begin();
    advance(it, deviceNumber-1);
    return (*it);
}

void open_device(string device)
{
    ostringstream devicePath;
    devicePath << "/dev/" << device;
    cout << "\n" << endl;
    device_fd = open(devicePath.str().c_str(), O_RDWR);
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
    string choosenDevice = choose_char_device();
    open_device(choosenDevice);

    while (true)
    {
        if (!read_and_print_board())
            break;

        int i, j;
        printf("Type the coord i, j (no comma): ");
        scanf("%d%d", &i, &j);
        if (!play(i, j))
            break;
    }

    close_device();
}