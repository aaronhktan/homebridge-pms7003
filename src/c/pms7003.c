#include "pms7003.h"

#include <stdio.h>
#include <unistd.h>   // POSIX APIs
#include <fcntl.h>    // open(), read(), write()
#include <termios.h>  // For terminal devices, aka serial port
#include <stdint.h>   // For uint8_t

#include <sys/epoll.h> // For epoll

#define MAX_EVENTS 1
static int uartfd = -1;
static int epollfd = -1;

int PMS7003_init() {
  // O_NOCTTY to make the serial port not the controlling terminal for the process
  uartfd = open("/dev/serial0", O_RDONLY | O_NOCTTY);
  if (uartfd == -1) {
    debug_print(stderr, "%s", "Couldn't open /dev/serial0\n");
    return ERROR_DRIVER;
  }

  // Configure file descriptor for serial 0
  struct termios opts;
  tcgetattr(uartfd, &opts); // Get attributes for the serial terminal port
  opts.c_iflag = IGNPAR;  // Ignore framing/parity errors
  opts.c_oflag = 0;    // Output modes, don't need any of these
  opts.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // 9600bd, 8-bit char, ignore modem status lines, enable receiving
  opts.c_lflag = 0;    // Local modes, don't need any of these
  tcsetattr(uartfd, TCSANOW, &opts); // Set the options immediately
  tcflush(uartfd, TCIFLUSH);  // Flush the terminal
 
  // Make sure changes propagated fine
  if (uartfd == -1) {
    debug_print(stderr, "%s", "Failed after configuring file descriptor");
    return ERROR_DRIVER;
  }

  // Create epoll file descriptor to watch for events on uart fd
  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    debug_print(stderr, "%s", "Couldn't create epoll file descriptor\n");
    return ERROR_DRIVER;
  } 

  // Add uartfd to epoll
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = uartfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, uartfd, &event)) {
    debug_print(stderr, "%s", "Couldn't add uartfd to epoll\n");
    return ERROR_DRIVER;
  }

  return NO_ERROR;
}

int PMS7003_deinit() {
  close(uartfd);
  close(epollfd);

  return NO_ERROR;
}

int PMS7003_read(int timeout_ms, pms7003_data *data) {
  if (timeout_ms < 0) {
    debug_print(stderr, "%s", "Invalid timeout specified\n");
    return ERROR_INVAL;
  }

  struct epoll_event events[MAX_EVENTS];
  int num_events = epoll_wait(epollfd, events, MAX_EVENTS, timeout_ms);
  if (!num_events) {
    debug_print(stderr, "%s", "Nothing on UART port in timeout specified\n");
    return ERROR_TIMEOUT;
  }

  // Wait until timeout_ms have passed for a message on UART
  // Read characters until we see 0x42, which is the first byte in the
  // data frame of UART
  uint8_t rx_buf[32];
  do {
    read(uartfd, (void *)&rx_buf, 1);
  } while (rx_buf[0] != 0x42);

  // Once we have the start character, we can read the other 31 bytes
  // of the frame. 
  int rx_length = read(uartfd, (void *)&rx_buf[1], 31);

  if (rx_length < 0) {
    debug_print(stderr, "%s", "Unable to read from UART\n");
    return ERROR_DRIVER;
  } else if (rx_length == 0) {
    debug_print(stderr, "%s", "No data received...\n");
    return ERROR_DRIVER;
  }

  // First two bytes are start fixed start characters
  if (rx_buf[0] != 0x42 || rx_buf[1] != 0x4d) {
    debug_print(stderr, "%s", "Framing error, sadness :(\n");
    return ERROR_DATA;
  }

  // Next two bytes tell us length of data, should be 2 * 13 + 2
  if ((rx_buf[2] << 8 | rx_buf[3]) != 28) {
    debug_print(stderr, "Incorrect length, expected %d, received %d\n",
      28, rx_buf[2] << 8 | rx_buf[3]);
    return ERROR_DATA;
  }

  uint16_t pm1_0_s = rx_buf[4] << 8 | rx_buf[5];
  uint16_t pm2_5_s = rx_buf[6] << 8 | rx_buf[7];
  uint16_t pm10_s = rx_buf[8] << 8 | rx_buf[9];
  uint16_t pm1_0 = rx_buf[10] << 8 | rx_buf[11];
  uint16_t pm2_5 = rx_buf[12] << 8 | rx_buf[13];
  uint16_t pm10 = rx_buf[14] << 8 | rx_buf[15];

  uint16_t bucket0_3 = rx_buf[16] << 8 | rx_buf[17];
  uint16_t bucket0_5 = rx_buf[18] << 8 | rx_buf[19];
  uint16_t bucket1_0 = rx_buf[20] << 8 | rx_buf[21];
  uint16_t bucket2_5 = rx_buf[22] << 8 | rx_buf[23];
  uint16_t bucket5_0 = rx_buf[23] << 8 | rx_buf[25];
  uint16_t bucket10 = rx_buf[26] << 8 | rx_buf[27];
  
  debug_print(stdout, "PM1.0 standard concentration: %dμg\n", pm1_0_s);
  debug_print(stdout, "PM2.5 standard concentration: %dμg\n", pm2_5_s);
  debug_print(stdout, "PM10 standard concentration: %dμg\n", pm10_s);
  debug_print(stdout, "PM1.0 concentration: %dμg\n", pm1_0);
  debug_print(stdout, "PM2.5 concentration: %dμg\n", pm2_5);
  debug_print(stdout, "PM10 concentration: %dμg\n", pm10);
  debug_print(stdout, "Particles > 0.3µm in 0.1L air: %d\n", bucket0_3);
  debug_print(stdout, "Particles > 0.5µm in 0.1L air: %d\n", bucket0_5);
  debug_print(stdout, "Particles > 1.0µm in 0.1L air: %d\n", bucket1_0);
  debug_print(stdout, "Particles > 2.5µm in 0.1L air: %d\n", bucket2_5);
  debug_print(stdout, "Particles > 5.0µm in 0.1L air: %d\n", bucket5_0);
  debug_print(stdout, "Particles > 10µm in 0.1L air: %d\n", bucket10);

  // Bytes 29 and 30 are not used except for in the check code
  uint16_t calculated_check = 0; 
  for (int i = 0; i < 30; i++) {
    calculated_check += rx_buf[i]; 
  } 
  uint16_t check = rx_buf[30] << 8 | rx_buf[31];
  debug_print(stderr, "Calculated check code: 0x%x; check code: 0x%x\n",
    calculated_check, check);

  if (calculated_check != check) {
    debug_print(stderr, "Calculated check 0x%x does not match 0x%x\n",
      calculated_check, check);
    return ERROR_CHECK; 
  }

  // Assign struct members and return
  data->pm1_0_s = pm1_0_s;
  data->pm2_5_s = pm2_5_s;
  data->pm10_s = pm10_s;
  data->pm1_0 = pm1_0;
  data->pm2_5 = pm2_5;
  data->pm10 = pm10;

  data->bucket0_3 = bucket0_3;
  data->bucket0_5 = bucket0_5; 
  data->bucket1_0 = bucket1_0; 
  data->bucket2_5 = bucket2_5; 
  data->bucket5_0 = bucket5_0;
  data->bucket10 = bucket10;

  return NO_ERROR;
}
