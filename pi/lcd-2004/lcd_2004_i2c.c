// Based on https://github.com/YuShuanHsieh/lcd-2004a-i2c/blob/main/lcd_2004_i2c.c
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "lcd_2004_i2c.h"

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
static bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

static void i2c_write_byte(i2c_inst_t *i2c, uint8_t addr, uint8_t val) {
    i2c_write_blocking(i2c, addr, &val, 1, false);
}

static int lcd_check_available(struct lcd_device *lcd)
{
    uint8_t rxdata;
    if (reserved_addr(lcd->addr))
        return false;
    
    return i2c_read_blocking(lcd->i2c, lcd->addr, &rxdata, 1, false) < 0 ? false : true;
}

static void lcd_toggle_enable(struct lcd_device *lcd, uint8_t val) {
    sleep_us(ENABLE_DELAY_US);
    i2c_write_byte(lcd->i2c, lcd->addr, val | LCD_ENABLE_BIT);
    sleep_us(ENABLE_DELAY_US);
    i2c_write_byte(lcd->i2c, lcd->addr, val & ~LCD_ENABLE_BIT);
    sleep_us(ENABLE_DELAY_US);
}

static void lcd_send_byte(struct lcd_device *lcd, uint8_t val, int mode) {
    uint8_t high = mode | (val & 0xF0) | LCD_BACKLIGHT;
    uint8_t low = mode | ((val << 4) & 0xF0) | LCD_BACKLIGHT;

    // The data can be sent before or after sending RS and RW signals.
    // But data should be available before toggling LCD enable pin.
    i2c_write_byte(lcd->i2c, lcd->addr, high);
    lcd_toggle_enable(lcd, high);

    i2c_write_byte(lcd->i2c, lcd->addr, low);
    lcd_toggle_enable(lcd, low);
}

void inline lcd_char(struct lcd_device *lcd, char val) {
    lcd_send_byte(lcd, val, LCD_CHARACTER);
}

static void lcd_home(struct lcd_device *lcd) {
    lcd_send_byte(lcd, LCD_RETURNHOME, LCD_COMMAND);
}

void lcd_clear(struct lcd_device *lcd) {
    lcd_send_byte(lcd, LCD_CLEARDISPLAY, LCD_COMMAND);
}

void lcd_set_cursor(struct lcd_device *lcd, int line, int position) {
    int val;
	switch(line) {
		case 0:
			val = LCD_ROW0;
			break;
		case 1:
			val = LCD_ROW1;
			break;
		case 2:
			val = LCD_ROW2;
			break;
		case 3:
			val = LCD_ROW3;
			break;
	}
    val += position;
    lcd_send_byte(lcd, val, LCD_COMMAND);
}

void lcd_string(struct lcd_device *lcd, const char *s) {
    while (*s) {
        lcd_char(lcd, *s++);
    }
}

bool lcd_init(struct lcd_device *lcd) {
    if(!lcd_check_available(lcd))
        return false;

    // please check the initialization flow from https://cdn-shop.adafruit.com/datasheets/TC2004A-01.pdf    
    lcd_send_byte(lcd, 0x03, LCD_COMMAND);
    lcd_send_byte(lcd, 0x03, LCD_COMMAND);
    lcd_send_byte(lcd, 0x03, LCD_COMMAND);
    lcd_send_byte(lcd, 0x02, LCD_COMMAND);

    lcd_send_byte(lcd, LCD_ENTRYMODESET | LCD_ENTRYLEFT, LCD_COMMAND);
    lcd_send_byte(lcd, LCD_FUNCTIONSET | LCD_2LINE, LCD_COMMAND);
    lcd_send_byte(lcd, LCD_DISPLAYCONTROL | LCD_DISPLAYON, LCD_COMMAND);
    lcd_clear(lcd);
    lcd_home(lcd);

    return true;
}
