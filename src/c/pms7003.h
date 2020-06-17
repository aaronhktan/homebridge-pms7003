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
  ERROR_INVAL,          // Invalid argument
  ERROR_DRIVER,         // UART or epoll failed to read data
  ERROR_TIMEOUT,        // Timeout parameter reached
  ERROR_DATA,           // Framing error or other error with data
  ERROR_CHECK           // Check codes do not match
};

// Standard concentration is concentration for air
// at ICAO standard atmosphere (288.15K, 1013.25hPa at sea level)
// https://publiclab.org/questions/samr/04-07-2019/how-to-interpret-pms5003-sensor-values
typedef struct pms7003_data_s {
  uint16_t pm1_0_s;     // Standard PM1.0 concentration
  uint16_t pm2_5_s;     // Standard factory PM2.5 concentration
  uint16_t pm10_s;      // Standard factory PM10 concentration 
  uint16_t pm1_0;       // Current PM1.0 concentration
  uint16_t pm2_5;       // Current PM2.5 concentration
  uint16_t pm10;        // Current PM10 concentration
  uint16_t bucket0_3;   // Particles of size 0.3µm<x<0.5µm
  uint16_t bucket0_5;   // Particles of size 0.5µm≤x<1.0µm
  uint16_t bucket1_0;   // Particles of size 1.0µm≤x<2.5µm
  uint16_t bucket2_5;   // Particles of size 2.5µm≤x<5.0µm
  uint16_t bucket5_0;   // Particles of size 5.0µm≤x<10µm
  uint16_t bucket10;    // Particles of size >10µm
} pms7003_data;

#define PMS7003_START_CHAR 0x42;
#define PMS7003_SECOND_CHAR 0x4D;
#define PMS7003_DATA_LENGTH 28;

int PMS7003_init();
int PMS7003_deinit();

int PMS7003_read(int timeout_ms,
                 pms7003_data *data);

