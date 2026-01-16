#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

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

    // Write data to the serial port
    const char *message = "f 100 1\n";
    n = write(fd, message, strlen(message));
    printf("Wrote %d bytes: %s", n, message);
    
    tcdrain(fd); //blocks until everything written to the port is transmitted


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

    // Close the port
    close(fd);

    return 0;
}