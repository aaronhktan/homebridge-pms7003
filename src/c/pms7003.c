#include <stdio.h>
#include <unistd.h>   // POSIX APIs
#include <fcntl.h>    // open(), read(), write()
#include <termios.h>  // For terminal devices, aka serial port
#include <stdint.h>   // For uint8_t

int main(int argc, char **argv) {
  // O_NOCTTY to make the serial port not the controlling terminal for the process
  int uartfd = open("/dev/serial0", O_RDONLY | O_NOCTTY);
  if (uartfd == -1) {
    fprintf(stderr, "Couldn't open /dev/serial0\n");
    return -1;
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
 
  // Check for messages
  if (uartfd == -1) {
    fprintf(stderr, "Failed after configuring file descriptor");
    return -1;
  }

  uint8_t rx_buf[32];
  while (1) {
    // Read characters until we see 0x42, which is the first byte in the
    // data frame of UART
    do {
      read(uartfd, (void *)&rx_buf, 1);
    } while (rx_buf[0] != 0x42);

    // Once we have the start character, we can read the other 31 bytes
    // of the frame. 
    int rx_length = read(uartfd, (void *)&rx_buf[1], 31);

    if (rx_length < 0) {
      continue;
    } else if (rx_length == 0) {
      // No data ??
      continue;
    }

    // First two bytes are start fixed start characters
    if (rx_buf[0] != 0x42 || rx_buf[1] != 0x4d) {
      fprintf(stderr, "Framing error, sadness :(\n");
      continue;
    }

    // Next two bytes tell us length of data, should be 2 * 13 + 2
    if (rx_buf[2] << 8 | rx_buf[3] != 28) {
      fprintf(stderr, "Incorrect length of data\n");
      continue;
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
    
    printf("PM1.0 standard concentration: %dμg\n", pm1_0_s);
    printf("PM2.5 standard concentration: %dμg\n", pm2_5_s);
    printf("PM10 standard concentration: %dμg\n", pm10_s);
    printf("PM1.0 concentration: %dμg\n", pm1_0);
    printf("PM2.5 concentration: %dμg\n", pm2_5);
    printf("PM10 concentration: %dμg\n", pm10);
    printf("Particles > 0.3µm in 0.1L air: %d\n", bucket0_3);
    printf("Particles > 0.5µm in 0.1L air: %d\n", bucket0_5);
    printf("Particles > 1.0µm in 0.1L air: %d\n", bucket1_0);
    printf("Particles > 2.5µm in 0.1L air: %d\n", bucket2_5);
    printf("Particles > 5.0µm in 0.1L air: %d\n", bucket5_0);
    printf("Particles > 10µm in 0.1L air: %d\n", bucket10);

    // Bytes 29 and 30 are not used except for in the check code
    printf("Calculated check code: 0x%x; check code: 0x%x\n", rx_buf[0] 
      + rx_buf[1] + rx_buf[2] + rx_buf[3] + pm1_0_s + pm2_5_s + pm10_s + pm1_0
      + pm2_5 + pm10 + bucket0_3 + bucket0_5 + bucket1_0 + bucket2_5
      + bucket5_0 + bucket10 + rx_buf[28] + rx_buf[29], rx_buf[30] << 8 | rx_buf[31]); 
  } 
}

