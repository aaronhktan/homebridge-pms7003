#include "pms7003.h"

#include <stdlib.h>
#include <signal.h>

static void int_handle(int signal) {
  PMS7003_deinit();
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGINT, int_handle);
  
  PMS7003_init();

  pms7003_data data;
  while (1) {
    int rv = PMS7003_read(3000, &data);
    printf("PM1.0 standard concentration: %dμg\n", data.pm1_0_s);
    printf("PM2.5 standard concentration: %dμg\n", data.pm2_5_s);
    printf("PM10 standard concentration: %dμg\n", data.pm10_s);
    printf("PM1.0 concentration: %dμg\n", data.pm1_0);
    printf("PM2.5 concentration: %dμg\n", data.pm2_5);
    printf("PM10 concentration: %dμg\n", data.pm10);
    printf("Particles > 0.3µm in 0.1L air: %d\n", data.bucket0_3);
    printf("Particles > 0.5µm in 0.1L air: %d\n", data.bucket0_5);
    printf("Particles > 1.0µm in 0.1L air: %d\n", data.bucket1_0);
    printf("Particles > 2.5µm in 0.1L air: %d\n", data.bucket2_5);
    printf("Particles > 5.0µm in 0.1L air: %d\n", data.bucket5_0);
    printf("Particles > 10µm in 0.1L air: %d\n", data.bucket10);
    if (rv) {
      printf("rv: %d\n", rv);
    }
  } 
}

