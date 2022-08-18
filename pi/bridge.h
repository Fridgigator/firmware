#include <stdint.h>
typedef struct i2c_result_read{
    char len;
    uint8_t content;
} i2c_result_read;
const int8_t WRONG_I2C_ASSERT_ERROR = 1;
const int8_t FAILED_TO_INIT_LCD_DEVICE = 2;

const uint8_t NO_FRAME_AVAILABLE = 1;
const int WRONG_PIO_DEVICE = -1;
const int CANNOT_INIT_PIO = -2;

typedef struct Frame {
        uint8_t p_address;
        uint8_t p_data;
        uint8_t error;
      } Frame;

void c_print(const void* const s);

void c_device_sleep(unsigned int ms);
void c_gpio_init(uint8_t pin);
void c_gpio_set_dir(unsigned char pin, uint8_t direction);
void c_gpio_put(uint8_t pin, uint8_t powerLevel);

void* c_lcd_init(uint8_t i2c_channel, int addr, uint8_t sda_pin, uint8_t scl_pin, int8_t* errorCode);
void c_lcd_putch(void* lcd_device, uint8_t line, int pos, unsigned char ch);
void c_lcd_clear(void* lcd_device);


int c_register_ir(uint8_t pioDevice, uint8_t pin);
Frame c_read_ir(uint8_t pioDevice, uint8_t rx_sm);