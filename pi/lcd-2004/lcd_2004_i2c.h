// https://github.com/YuShuanHsieh/lcd-2004a-i2c/blob/main/lcd_2004_i2c.h

#ifndef LCD2004_I2C_H
#define LCD2004_I2C_H

#include <stdlib.h>

// commands
#define LCD_CLEARDISPLAY         0x01
#define LCD_RETURNHOME           0x02
#define LCD_ENTRYMODESET         0x04
#define LCD_DISPLAYCONTROL       0x08
#define LCD_CURSORSHIFT          0x10
#define LCD_FUNCTIONSET          0x20
#define LCD_SETCGRAMADDR         0x40
#define LCD_SETDDRAMADDR         0x80

// flags for display entry mode
#define  LCD_ENTRYSHIFTINCREMENT 0x01
#define  LCD_ENTRYLEFT           0x02

// flags for display and cursor control
#define  LCD_BLINKON             0x01
#define  LCD_CURSORON            0x02
#define  LCD_DISPLAYON           0x04

// flags for display and cursor shift
#define  LCD_MOVERIGHT           0x04
#define  LCD_DISPLAYMOVE         0x08

// flags for function set
#define  LCD_5x10DOTS            0x04
#define  LCD_2LINE               0x08
#define  LCD_8BITMODE            0x10

// flag for backlight control
#define  LCD_BACKLIGHT           0x08

#define  LCD_ENABLE_BIT          0x04

// Modes for lcd_send_byte
#define LCD_CHARACTER   1
#define LCD_COMMAND     0

#define MAX_LINES       4
#define MAX_CHARS       20

#define LCD_ROW0        0x80
#define LCD_ROW1        0xC0
#define LCD_ROW2        0x94
#define LCD_ROW3        0xD4
#define ENABLE_DELAY_US 666

struct lcd_device
{
  uint8_t addr;
  i2c_inst_t *i2c;
};

bool lcd_init(struct lcd_device *lcd);
void lcd_string(struct lcd_device *lcd, const char *s);
void lcd_set_cursor(struct lcd_device *lcd, int line, int position);
void lcd_clear(struct lcd_device *lcd);
void lcd_char(struct lcd_device *lcd, char val);
#endif
