#include <stdio.h> 
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/select.h>
// Settings
#define DEFAULT_BAUDRATE 460800
#define MAX_CHANNEL_COUNT 44
#define READ_BUFFER_SIZE 256
// Global variables
volatile sig_atomic_t write_buffer_offset = INT_MAX;
volatile sig_atomic_t remaining_writes = 0;
volatile sig_atomic_t write_counter = 0;
volatile sig_atomic_t read_counter = 0;
volatile sig_atomic_t sample_counter = 0;
int adc_data[MAX_CHANNEL_COUNT];
int adc_count = 0;
int cur_adc = -1;
uint8_t command[(MAX_CHANNEL_COUNT + 2) * 1000];
// Print usage information
void print_usage(char *program_name) {
	fprintf(stderr, "Usage: %s device\n", program_name);
	fprintf(stderr, "\tdevice - path to serial device (e.g. /dev/ttyS0 or /dev/ttyUSB0)\n");
	fprintf(stderr, "\n");
}
// Open serial port
int open_serial_device(char *path) {
	// Open device
	int fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd == -1) return -1;
	// Get current options
	struct termios options;
	tcgetattr(fd, &options);
	// Set baudrate
	cfsetspeed(&options, DEFAULT_BAUDRATE);
	// Set mode (8N1)
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	// Disable hardware flow control (if available)
#ifdef CNEW_RTSCTS
	options.c_cflag &= ~CNEW_RTSCTS;
#elifdef CRTSCTS
	options.c_cflag &= ~CRTSCTS;
#endif
	// Set new options
	tcsetattr(fd, TCSANOW, &options);
	// Return handle
	return fd;
}
// Alarm handler
void alarm_handler(int sig) {
	// Check for timeout
	static int first_run = 1;
	if (first_run) {
		first_run = 0;
	} else if (!sample_counter) {
		fprintf(stderr, "Timeout\n");
		exit(-2);
	}
	// Send next command
	if (write_buffer_offset >= sizeof(command)) {
		write_buffer_offset = 0;
	} else {
		remaining_writes++;
	}
	// Display debug info
	fprintf(stderr, "Writing %i bps, reading %i bps, %i samples per second\n", write_counter, read_counter, sample_counter);
	// Reset performance counter
	read_counter = 0;
	write_counter = 0;
	sample_counter = 0;
}
// Print ADC data
void print_adc_data() {
	int i;
	for (i = 0; i < adc_count; i++) {
		if (i) {
			printf(",%i", adc_data[i]);
		} else {
			printf("%i", adc_data[i]);
		}
	}
	printf("\n");
}
// Main function
int main(int argc, char **argv) {
	if (argc < 2) {
		print_usage(argv[0]);
	} else {
		// Open serial port
		char *device = argv[1];
		int device_fd = open_serial_device(device);
		if (device_fd == -1) {
			fprintf(stderr, "Failed to open %s: %s\n", device, strerror(errno));
			return -1;
		}
		// Setup alarm signal handler
		{
			struct sigaction sig;
			sig.sa_handler = alarm_handler;
			sigemptyset(&sig.sa_mask);
			sig.sa_flags = SA_RESTART;
			sigaction(SIGALRM, &sig, NULL);
		}
		// Setup timer
		timer_t timer;
		if (timer_create(CLOCK_MONOTONIC, NULL, &timer)) {
			perror("timer_create() failed\n");
			close(device_fd);
			return -1;
		}
		{
			struct itimerspec timer_spec;
			timer_spec.it_interval.tv_sec = 1;
			timer_spec.it_interval.tv_nsec = 0;
			timer_spec.it_value.tv_sec = 0;
			timer_spec.it_value.tv_nsec = 1;
			if (timer_settime(timer, 0, &timer_spec, NULL) < 0) {
				perror("timer_settime() failed");
				close(device_fd);
				return -1;
			}
		}
		// Generate USART command
		{
			int i;
			memset(command, 0xFF, sizeof(command));
			for (i = 0; i < sizeof(command); i += MAX_CHANNEL_COUNT + 2) {
				command[i] = 0;
			}
		}
		// Main loop
		while (1) {
			// Wait device ready for reading or writing
			fd_set fds_r, fds_w;
			FD_ZERO(&fds_r);
			FD_ZERO(&fds_w);
			FD_SET(device_fd, &fds_r);
			FD_SET(device_fd, &fds_w);
			int retval = select(FD_SETSIZE, &fds_r, &fds_w, NULL, NULL);
			// Check for errors
			if (retval < 0) {
				if (errno == EINTR) continue;
				perror("select() failed");
				timer_delete(timer);
				close(device_fd);
				return -1;
			}
			// Read data
			if (FD_ISSET(device_fd, &fds_r)) {
				uint8_t buffer[READ_BUFFER_SIZE];
				int bytes_count;
				while ((bytes_count = read(device_fd, buffer, sizeof(buffer))) > 0) {
					read_counter += bytes_count;
					int i;
					for (i = 0; i < bytes_count; i++) {
						if (buffer[i] == 0xFF) {
							if (adc_count) {
								print_adc_data();
								sample_counter++;
								adc_count = 0;
							}
							cur_adc = -1;
						} else {
							if ((cur_adc > -1) && (cur_adc < MAX_CHANNEL_COUNT)) {
								if (buffer[i] != 0xFF) {
									adc_data[cur_adc] = buffer[i];
									adc_count++;
								}
							}
							cur_adc++;
						}
					}
				}
			}
			// Write data
			if (FD_ISSET(device_fd, &fds_w) && (write_buffer_offset < sizeof(command))) {
				int bytes_count;
				while ((bytes_count = write(device_fd, command + write_buffer_offset, sizeof(command) - write_buffer_offset)) > 0) {
					write_counter += bytes_count;
					write_buffer_offset += bytes_count;
				}
				if ((write_buffer_offset >= sizeof(command)) && remaining_writes) {
					write_buffer_offset = 0;
					remaining_writes--;
				}
			}
		}
		// Cleanup
		timer_delete(timer);
		close(device_fd);
	}
	return 0;
}