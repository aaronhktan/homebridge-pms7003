#include <stdint.h>
#include <stdio.h>

// Print function that only prints if DEBUG is defined
#ifdef DEBUG
#define DEBUG_PRINT 1
#else
#define DEBUG_PRINT 0
#endif
#define debug_print(fd, fmt, ...) \
            do { if (DEBUG_PRINT) fprintf(fd, fmt, __VA_ARGS__); } while (0)

enum Error {
  NO_ERROR,
  ERROR_DEVICE,         // Couldn't find device
  ERROR_INVAL,          // Invalid argument
  ERROR_DRIVER,         // UART or epoll failed to read data
  ERROR_TIMEOUT,        // Timeout parameter reached
  ERROR_DATA,           // Framing error or other error with data
  ERROR_CHECK           // Check codes do not match
};

typedef struct pms7003_data_s {
  uint16_t pm1_0_s;
  uint16_t pm2_5_s;
  uint16_t pm10_s; 
  uint16_t pm1_0;
  uint16_t pm2_5;
  uint16_t pm10;
  uint16_t bucket0_3;
  uint16_t bucket0_5;
  uint16_t bucket1_0;
  uint16_t bucket2_5;
  uint16_t bucket5_0;
  uint16_t bucket10;
} pms7003_data;

#define PMS7003_START_CHAR 0x42;
#define PMS7003_SECOND_CHAR 0x4D;
#define PMS7003_DATA_LENGTH 28;

int PMS7003_init();
int PMS7003_deinit();

int PMS7003_read(int timeout_ms,
                 pms7003_data *data);

