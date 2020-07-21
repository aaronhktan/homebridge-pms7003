#include "pms7003.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>   // POSIX APIs
#include <fcntl.h>    // open(), read(), write()
#include <termios.h>  // For terminal devices, aka serial port
#include <stdint.h>   // For uint8_t

#include <sys/epoll.h> // For epoll

#define MAX_EVENTS 1
static int uartfd = -1;
static int epollfd = -1;

// Check code is first 30 bytes added together
static int verify_checkcode(uint8_t *data) {
  uint16_t calculated_check = 0;
  for (int i = 0; i < 30; i++) {
    calculated_check += data[i];
  }
  uint16_t check = data[30] << 8 | data[31];
  debug_print(stderr, "Calculated check code: 0x%x; check code: 0x%x\n",
    calculated_check, check);

  if (calculated_check != check) {
    debug_print(stderr, "Calculated check 0x%x does not match 0x%x\n",
      calculated_check, check);
    return ERROR_CHECK;
  }

  return NO_ERROR;
}

static int read_data(int fd, uint8_t *data) {
  // Read characters until we see 0x42, which is the first byte in the
  // data frame of UART
  do {
    read(fd, (void *)data, 1);
  } while (data[0] != PMS7003_START_CHAR);

  // Once we have the start character, we can read the other 31 bytes
  // of the frame.
  // For some reason, read(fd, &data[1], 31) doesn't cooperate;
  // using a for loop instead works.
  for (int i = 1; i < 32; i++) {
    read(fd, &data[i], 1);
  }

  // First two bytes are start fixed start characters
  if (data[0] != PMS7003_START_CHAR || data[1] != PMS7003_SECOND_CHAR) {
    debug_print(stderr, "%s\n", "Framing error, sadness :(");
    return ERROR_DATA;
  }

  // Next two bytes tell us length of data, should be 2 * 13 + 2
  if ((data[2] << 8 | data[3]) != PMS7003_DATA_LENGTH) {
    debug_print(stderr, "Incorrect length, expected %d, received %d\n",
      PMS7003_DATA_LENGTH, data[2] << 8 | data[3]);
    return ERROR_DATA;
  }

  // Check calculated check code vs sent check code
  int rv = verify_checkcode(data);
  if (rv) {
    debug_print(stderr, "%s\n", "Calculated check code does not match!");
    return rv;
  }

  return NO_ERROR;
}

static int process_data(uint8_t *data, pms7003_data *out) {
  uint16_t pm1_0_s = data[4] << 8 | data[5];
  uint16_t pm2_5_s = data[6] << 8 | data[7];
  uint16_t pm10_s = data[8] << 8 | data[9];
  uint16_t pm1_0 = data[10] << 8 | data[11];
  uint16_t pm2_5 = data[12] << 8 | data[13];
  uint16_t pm10 = data[14] << 8 | data[15];

  uint16_t bucket0_3 = data[16] << 8 | data[17];
  uint16_t bucket0_5 = data[18] << 8 | data[19];
  uint16_t bucket1_0 = data[20] << 8 | data[21];
  uint16_t bucket2_5 = data[22] << 8 | data[23];
  uint16_t bucket5_0 = data[23] << 8 | data[25];
  uint16_t bucket10 = data[26] << 8 | data[27];

  debug_print(stdout, "PM1.0 standard concentration: %dμg/m3\n", pm1_0_s);
  debug_print(stdout, "PM2.5 standard concentration: %dμg/m3\n", pm2_5_s);
  debug_print(stdout, "PM10 standard concentration: %dμg/m3\n", pm10_s);
  debug_print(stdout, "PM1.0 concentration: %dμg/m3\n", pm1_0);
  debug_print(stdout, "PM2.5 concentration: %dμg/m3\n", pm2_5);
  debug_print(stdout, "PM10 concentration: %dμg/m3\n", pm10);
  debug_print(stdout, "Particles > 0.3µm in 0.1L air: %d\n", bucket0_3);
  debug_print(stdout, "Particles > 0.5µm in 0.1L air: %d\n", bucket0_5);
  debug_print(stdout, "Particles > 1.0µm in 0.1L air: %d\n", bucket1_0);
  debug_print(stdout, "Particles > 2.5µm in 0.1L air: %d\n", bucket2_5);
  debug_print(stdout, "Particles > 5.0µm in 0.1L air: %d\n", bucket5_0);
  debug_print(stdout, "Particles > 10µm in 0.1L air: %d\n", bucket10);

  out->pm1_0_s = pm1_0_s;
  out->pm2_5_s = pm2_5_s;
  out->pm10_s = pm10_s;
  out->pm1_0 = pm1_0;
  out->pm2_5 = pm2_5;
  out->pm10 = pm10;

  out->bucket0_3 = bucket0_3;
  out->bucket0_5 = bucket0_5;
  out->bucket1_0 = bucket1_0;
  out->bucket2_5 = bucket2_5;
  out->bucket5_0 = bucket5_0;
  out->bucket10 = bucket10;

  return NO_ERROR;
}

int PMS7003_init() {
  // O_NOCTTY to make the RPi not the controlling terminal for the process
  uartfd = open("/dev/serial0", O_RDWR | O_NOCTTY);
  if (uartfd == -1) {
    debug_print(stderr, "%s\n", "Couldn't open /dev/serial0");
    return ERROR_DRIVER;
  }

  // Configure file descriptor for the UART port
  struct termios opts;
  tcgetattr(uartfd, &opts); // Get attributes for the serial terminal port
  opts.c_iflag = IGNPAR;  // Ignore framing/parity errors
  opts.c_oflag = 0;    // Output modes, don't need any of these
  opts.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // 9600bd, 8-bit char, ignore modem status lines, enable receiving
  opts.c_lflag = 0;    // Local modes, don't need any of these
  tcflush(uartfd, TCIFLUSH);  // Flush the terminal
  tcsetattr(uartfd, TCSANOW, &opts); // Set the options immediately

  if (uartfd == -1) {
    debug_print(stderr, "%s\n", "Failed after configuring file descriptor");
    return ERROR_DRIVER;
  }

  // Create epoll file descriptor
  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    debug_print(stderr, "%s\n", "Couldn't create epoll file descriptor");
    return ERROR_DRIVER;
  }

  // Add uartfd to epoll to watch for events on uartfd
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = uartfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, uartfd, &event)) {
    debug_print(stderr, "%s\n", "Couldn't add uartfd to epoll");
    return ERROR_DRIVER;
  }

  return NO_ERROR;
}

int PMS7003_deinit() {
  close(uartfd);
  close(epollfd);

  return NO_ERROR;
}

int PMS7003_read(int timeout_ms, pms7003_data *out) {
  if (timeout_ms < 0) {
    debug_print(stderr, "%s\n", "Invalid timeout specified");
    return ERROR_INVAL;
  }

  // Wait until timeout_ms have passed for a message on UART
  struct epoll_event events[MAX_EVENTS];
  int num_events = epoll_wait(epollfd, events, MAX_EVENTS, timeout_ms);
  if (!num_events) {
    debug_print(stderr, "%s\n", "Nothing on UART port in timeout specified");
    return ERROR_TIMEOUT;
  }

  tcflush(uartfd, TCIFLUSH);  // Flush the terminal

  // Read and process data from UART file descriptor
  uint8_t rx_buf[32];
  memset(rx_buf, 0, 32);
  int rv = read_data(uartfd, (void *)rx_buf);
  if (rv) {
    return rv;
  }
  process_data((void *)rx_buf, out);

  return NO_ERROR;
}

