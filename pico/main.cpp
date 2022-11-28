#include <cstdio>
#include <hardware/adc.h>
#include <hardware/watchdog.h>
#include "pico/stdlib.h"
#include "dht.h"

int count = 0;
[[noreturn]]
int main() {
  stdio_init_all();
  dht_t dht22;
  dht_t dht11;
  int result = dht_init(&dht22, DHT22, pio0, 14, true);
  result = dht_init(&dht11, DHT11, pio1, 15, true);
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(4);
  uart_init(uart0, 9600);

  while (true) {
    result = dht_start_measurement(&dht22);

    if (result != 0) {
      uart_write_blocking(uart0, reinterpret_cast<const uint8_t *>("|a|"), 3);
      watchdog_enable(1, true);
      while (true);
    }
    result = dht_start_measurement(&dht11);
    if (result != 0) {
      uart_write_blocking(uart0, reinterpret_cast<const uint8_t *>("|b|"), 3);
      watchdog_enable(1, true);
      while (true);
    }
    sleep_ms(2'000);
    float humidity;
    float temp;
    result = dht_finish_measurement_blocking(&dht22, &humidity, &temp);
    if (result != 0) {
      uart_write_blocking(uart0, reinterpret_cast<const uint8_t *>("|c|"), 3);
      watchdog_enable(1, true);
      while (true);
    }
    uint8_t buf[11];
    int written = snprintf((char*)buf,10,"H%f",humidity);
    uart_write_blocking(uart0, buf, written);

    written = snprintf((char*)buf,10,"T%f",temp);
    uart_write_blocking(uart0, buf, written);

    result = dht_finish_measurement_blocking(&dht11, &humidity, &temp);
    if (result != 0) {
      uart_write_blocking(uart0, reinterpret_cast<const uint8_t *>("|d|"), 3);
      watchdog_enable(1, true);
      while (true);
    }
    written = snprintf((char*)buf,10,"h%f",humidity);
    uart_write_blocking(uart0, buf, written);

    written = snprintf((char*)buf,10,"t%f",temp);
    uart_write_blocking(uart0, buf, written);

    
    float reading = static_cast<float>(adc_read()) * 3.3f / static_cast<float>(1 << 12);
    float onboard_temp = 27.0f - (reading - 0.706f) / 0.001721f;
    written = snprintf((char*)buf,10,"p%f",onboard_temp);
    uart_write_blocking(uart0, buf, written);

  }
  if(count > 30*60){
      watchdog_enable(1, true);
      while (true);
  }
  count++;
}
