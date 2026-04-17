
/*
C program that initializes and configures a UART interface.
The program sets the baud rate, data bits, stop bits, and parity for the UART communication. 
It also includes error handling.
*/

#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h> 
#include <unistd.h> //Posix standard library, holds definition for STDIN_FILENO, STDOUT_FILENO, and other constants


/* -------------------- MACROS -------------------- */

// Default UART device (BeagleBone UART1 pins: P9.24 (TX), P9.26 (RX))
#define UART_DEVICE         "/dev/ttyS1"
// Used for comparisons instead of just 0
#define NUL          0
// Delay value in microseconds (10 ms)        
#define MILLIS_SECONDS_10  10000

/* -------------------- TYPE DEFINITIONS -------------------- */
// struct termios to easy structure definition for terminal and UART configurations.
typedef struct termios termios_t;

/* -------------------- FUNCTION PROTOTYPES -------------------- */
void ERROR_CHECK(int ret, char *message);
void configure_terminal(termios_t *terminal_st);
int configure_uart(termios_t *uart_st, const char *device);



int main(int argc, char *argv[]){

    // Defining termios structs for standard input/output and UART configurations.
    termios_t stdio_st;
    termios_t uart_st;
    termios_t original_st; //holds default terminal settings.


    int uart_fd;
    unsigned char input_char = 'R';

    // Save the original terminal settings to restore them later
    ERROR_CHECK(tcgetattr(STDIN_FILENO, &original_st), "Failed to get original terminal attributes");


    //configure terminal for standard input/output
    configure_terminal(&stdio_st);

    //configure our uart interface
    if (argc > NUL) {
        printf("Using UART device: %s\n", argv[1]);
        uart_fd = configure_uart(&uart_st, argv[1]);
    } else {
        printf("No UART device specified. Using default: %s\n\r", UART_DEVICE);
         uart_fd = configure_uart(&uart_st, UART_DEVICE);
    }
  

	printf("Enter message via Terminal to transmit to UART....\n\r");
    printf("Enter message via UART to display on the Terminal Console...\n\r");
	printf("Enter 'q' to terminate the program!\n\r");

	printf(".....\n\r");

        
    while (input_char!='q')
    {

	// a little delay
        usleep(MILLIS_SECONDS_10); // Sleep for 10 milliseconds

        // if new data is available on the serial port, print it out
        int n = read(uart_fd, &input_char, 1);
        if (n > 0) {
            ERROR_CHECK(write(STDOUT_FILENO, &input_char, 1), "Failed to write to STDOUT");
        } else if (n < 0 && errno != EAGAIN) {
            perror("UART read error");
        }

        // if new data is available on the console, send it to the serial port
        n = read(STDIN_FILENO,&input_char,1);
        if (n > 0) {
            ERROR_CHECK(write(uart_fd,&input_char,1), "Failed to write to UART");
        } else if (n < 0 && errno != EAGAIN) {
            perror("UART read error");
        }

            
    }

    //gracefully close the UART file descriptor before exiting
    close(uart_fd);
    
    // Restore the original terminal settings before exiting
    ERROR_CHECK(tcsetattr(STDIN_FILENO, TCSANOW, &original_st), "Failed to restore original terminal attributes");

    return 0;
}


/**
 * @brief Checks the return value of a system call and handles errors.
 *
 * This function verifies if a system call returned an error (i.e., a negative value).
 * If an error is detected, it prints a descriptive error message using perror()
 * and terminates the program using the current errno value.
 *
 * @param[in] ret Return value from a system call.
 * @param[in] message Custom error message to display.
 *
 * @note This function will terminate the program if an error occurs.
 */
void ERROR_CHECK(int ret, char *message){

    if (ret < NUL) {
        perror(message);
        exit(errno);
    }
}


/**
 * @brief Configures the terminal (STDIN) for raw, non-blocking input.
 *
 * This function sets the terminal into non-canonical mode, disabling line buffering
 * and echo. It allows character by character input processing. It also enables
 * non-blocking mode so that read() calls do not block execution if no data is available.
 *
 * Configuration details:
 * - Disables canonical mode (ICANON)
 * - Disables echo (ECHO)
 * - Disables signal interpretation (ISIG)
 * - Sets minimum read size to 1 byte
 * - No timeout (VTIME = 0)
 *
 * @param[out] terminal_st Pointer to a termios structure to store configuration.
 *
 */
void configure_terminal(termios_t *terminal_st){

    memset(terminal_st,0,sizeof(*terminal_st));

    terminal_st->c_iflag=0;
    terminal_st->c_oflag=0;
    terminal_st->c_cflag=0;
    terminal_st->c_lflag=0; //Disables canonical mode (line buffering) and echoing of input characters
    terminal_st->c_cc[VMIN] =1; //Wait for at least 1 byte //MIGHT NOT WORK THOUGH SINCE ITS IN NON BLOCKING MODE
    terminal_st->c_cc[VTIME] =0; //No timeout

    // ERROR_CHECK(tcsetattr(STDIN_FILENO,TCSANOW,terminal_st), "Failed to set terminal attributes (TCSANOW) for STDIN");
    ERROR_CHECK(tcsetattr(STDIN_FILENO,TCSAFLUSH,terminal_st), "Failed to set terminal attributes (TCSAFLUSH) for STDIN");

    ERROR_CHECK(fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK), "Failed to set non-blocking mode for STDIN");
}


/**
 * @brief Opens and configures a UART device.
 *
 * This function opens the specified UART device file and configures it using
 * POSIX termios settings for serial communication.
 *
 * UART configuration:
 * - Baud rate: 115200
 * - Data bits: 8 (CS8)
 * - Parity: None
 * - Stop bits: 1
 * - Mode: Non-blocking
 *
 * @param[out] uart_st Pointer to a termios structure used for UART configuration.
 * @param[in] device Path to the UART device (e.g., "/dev/ttyS1").
 *
 * @return int File descriptor of the opened UART device. If >=0  (valid file descriptor) and if <0 Failure (program exits via ERROR_CHECK)

 */
int configure_uart(termios_t *uart_st, const char *device){
    int tty_fd;

    tty_fd=open(device, O_RDWR | O_NONBLOCK);
    if (tty_fd < 0) {
        ERROR_CHECK(tty_fd, "Failed to open UART device. Please check the provided device path again.\n");
    }

    memset(uart_st,0,sizeof(*uart_st));
    uart_st->c_cflag = CS8 | CLOCAL | CREAD; // 8 data bits, No parity, 1 stop bit.

    uart_st->c_iflag=0;
    uart_st->c_oflag=0;
    uart_st->c_lflag=0;

    uart_st->c_cc[VMIN] =0; //Wait for at least 1 byte
    uart_st->c_cc[VTIME] =0; // No time out  //timeout is like 0.5 secs

    
    
    ERROR_CHECK(cfsetospeed(uart_st,B115200), "Failed to set output baud rate"); // Set output baud rate to 115200
    ERROR_CHECK(cfsetispeed(uart_st,B115200), "Failed to set input baud rate"); // Set input baud rate to 115200

    ERROR_CHECK(tcsetattr(tty_fd,TCSANOW,uart_st), "Failed to set UART attributes");
    return tty_fd;
}
