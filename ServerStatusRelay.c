#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 50
#define NumberOfFans 8

char* getData(char* type, char* info, int index, char* formattedBuffer) {
    char buffer[BUFFER_SIZE];
    if (strcmp(type, "cpu") == 0) {
        if (strcmp(info, "temp") == 0) {
            sprintf(formattedBuffer, "/sys/class/thermal/thermal_zone%d/temp", index);
            int fd = open(formattedBuffer, O_RDONLY);
            if (fd==-1) {
                printf("failed to open file!\n");
                return (char*)1; //failed to open cpu temp file
            }
            memset(buffer, 0, sizeof(buffer)); //if we don't zero out the buffers before my manual string editing later doesn't work ;(
            memset(formattedBuffer, 0, sizeof(buffer));
            read(fd, buffer, 49); //same deal for only reading 49 a \n is appended at the end (also why we don't put a \n in the sprintf below!)
            sprintf(formattedBuffer, "cpu temp %d %s", index, buffer);
            formattedBuffer[strlen(formattedBuffer)-4] = '\n'; //chop of last 3 digits of the string (since the value is recorded in millicelcius this is like dividing by 1000 and then flooring)
            formattedBuffer[strlen(formattedBuffer)-3] = '\0'; //jank? yes! but working? also yes!
            close(fd);
        } else {
            return (char*)2; //failed to find temp in cpu file
        }
    } else if (strcmp(type, "gpu") == 0) {
        if (strcmp(info, "vram") == 0) {
            system("rocm-smi --showmemuse --csv > rocm_output.txt");
            int fd = open("rocm_output.txt", O_RDONLY);
            if (fd==-1) {
                printf("failed to open file!\n");
                return (char*)1; //failed to open rocm output 
            }
            buffer[0] == 'c'; //to prevent the first chacter in the buffer randomly being ','
            for (int i = 0; i < (3 + 2 * index); i++) { //loop 3 times to get to the after the 3rd comma
                while (buffer[0] != ',') {
                    read(fd, buffer, 1);
                    //printf("current char being analysed: %c\n", buffer[0]);
                }
                buffer[0] = 'c'; //as we just found a comma we need to change it to something else so when we come back round next time we don't immediately see it again.
            }
            read(fd, buffer, 3); //percentage goes from 0 - 100 so we read the next 3 bytes

            //chop off anything after the next comma (comma also gets chopped)
            int i = 0;
            while (buffer[i] != ',') {
                //printf("current char being analysed (2): %c\n", buffer[i]);
                i++;
            }
            buffer[i] = '\0';

            sprintf(formattedBuffer, "gpu vram %d %s\n", index, buffer); //use sprtinf to remove potential trailing junk
            close(fd);
        } else if (strcmp(info, "temp") == 0) {
            system("rocm-smi --showtemp --csv > rocm_output.txt");
            int fd = open("rocm_output.txt", O_RDONLY);
            if (fd==-1) {
                printf("failed to open file!\n");
                return (char*)1; //failed to open rocm output
            }
            buffer[0] == 'c'; //to prevent the first chacter in the buffer randomly being ','
            for (int i = 0; i < (5 + 3 * index); i++) { //loop 3 times to get to the after the 3rd comma
                while (buffer[0] != ',') {
                    read(fd, buffer, 1);
                    //printf("current char being analysed: %c\n", buffer[0]);
                }
                buffer[0] = 'c'; //as we just found a comma we need to change it to something else so when we come back round next time we don't immediately see it again.
            }
            read(fd, buffer, 2); //temp is displayed as xx.x so we read 2 to get the whole number part
            buffer[2] = '\0'; //add null terminator so it detects as a string properly

            sprintf(formattedBuffer, "gpu temp %d %s\n", index, buffer); //use sprtinf to remove potential trailing junk
            close(fd);
        } else {
            return (char*)2; //could not parse gpu info desired
        }
    } else {
        return (char*)3; //could not parse whether command is for gpu or cpu
    }

    printf("formattedBuffer: %s", formattedBuffer);
    return formattedBuffer;
}

void UpdateFanspeed(int FanNumber, int temp, int fd) {
    char message[100];
    int speed = 100;
    if (0 < temp < 40) {
        speed = 0;
    } else if (40 <= temp < 70) {
        speed = 15;
    } else if (70 <= temp < 90) {
        speed = 50;
    } else {
        speed = 100;    
    }
    sprintf(message, "f %d %d\n", speed, FanNumber);
    int n = write(fd, message, strlen(message));
    printf("Wrote %d bytes: %s", n, message);
}

int main() {
    const char *portname = "/dev/ttyACM0";  // Change this to your port
    int fd;
    int n;
    struct termios options;

    // Open the serial port for reading and writing
    fd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK); //O_RDWR - read and write, O_NOCTTY - do not connect to terminal, O_NONBLOCK - non blocking
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    fcntl(fd, F_SETFL, 0); // Clear ALL file status flags, the ones we just set in open, we want to have blocking for later

    if (tcgetattr(fd, &options) != 0) { // Get current serial port settings and write them to options struct
        //cleanup in case of failure to read
        perror("tcgetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Configure port settings
    cfsetispeed(&options, B115200); //B115200 is a defined value from termios
    cfsetospeed(&options, B115200);

    // Configure control modes (c_cflag)
    options.c_cflag &= ~PARENB;    // No parity
    options.c_cflag &= ~CSTOPB;    // 1 stop bit
    options.c_cflag &= ~CSIZE;     // Clear data size bits
    options.c_cflag |= CS8;        // 8 data bits
    options.c_cflag &= ~CRTSCTS;   // Disable hardware flow control
    options.c_cflag &= ~HUPCL;     // Don't hang up on last close
    options.c_cflag |= CREAD;      // Enable receiver
    options.c_cflag |= CLOCAL;     // Ignore modem control lines

    // Configure input modes (c_iflag)
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP);
    options.c_iflag &= ~INLCR;
    options.c_iflag &= ~IGNCR;
    options.c_iflag &= ~ICRNL;
    options.c_iflag &= ~IXON;
    options.c_iflag &= ~IXOFF;
    options.c_iflag &= ~IXANY;
    options.c_iflag &= ~IUCLC;
    options.c_iflag &= ~IMAXBEL;
    options.c_iflag &= ~IUTF8;
    options.c_iflag &= ~(INPCK | IGNPAR);
    options.c_iflag &= ~OPOST;
    options.c_iflag &= ~OLCUC;
    options.c_iflag &= ~ONLCR;
    options.c_iflag &= ~OCRNL;
    options.c_iflag &= ~ONOCR;
    options.c_iflag &= ~ONLRET;
    options.c_iflag &= ~OFILL;
    options.c_iflag &= ~OFDEL;

    // Configure output modes (c_oflag)
    options.c_oflag &= ~OPOST;
    options.c_oflag &= ~ONLCR;
    options.c_oflag &= ~OCRNL;
    options.c_oflag &= ~ONOCR;
    options.c_oflag &= ~ONLRET;
    options.c_oflag &= ~OFILL;
    options.c_oflag &= ~OFDEL;

    // Configure local modes (c_lflag)
    options.c_lflag &= ~ISIG;      // No signal generation
    options.c_lflag &= ~ICANON;    // Non-canonical mode
    options.c_lflag &= ~ECHO;      // Disable echo
    options.c_lflag &= ~ECHOE;
    options.c_lflag &= ~ECHOK;
    options.c_lflag &= ~ECHONL;
    options.c_lflag &= ~NOFLSH;    // No flush after interrupt
    options.c_lflag &= ~TOSTOP;
    options.c_lflag &= ~IEXTEN;

    // Set input parameters (min and time)
    options.c_cc[VMIN] = 1;        // Block until at least 1 char arrives
    options.c_cc[VTIME] = 0;       // No inter-character timeout

    if (tcsetattr(fd, TCSANOW, &options) != 0) { //TCSANOW - apply changes now
        perror("tcsetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }

    char data[BUFFER_SIZE] = {0};
    char command[BUFFER_SIZE] = {0};
    int temp = -1;

    getData("gpu", "temp", 0, data);
    sscanf(data, "%*s %*s %*d %d", &temp);

    for (int i = 1; i <= NumberOfFans; i++) {    
        UpdateFanspeed(i, temp, fd);
    }

    tcdrain(fd); //blocks until everything written to the port is transmitted


    /*
    char buffer[100] = {0};

    printf("Waiting for response...\n");
    n = read(fd, buffer, sizeof(buffer) - 1);  // Leave room for null terminator

    if (n > 0) {
        buffer[n] = '\0';  // Null-terminate the string
        printf("Received %d bytes: %s", n, buffer);
    } else if (n == 0) {
        printf("No data received within timeout.\n");
    } else {
        perror("read");
    }
    */

    // Close the port
    close(fd);

    return 0;
}