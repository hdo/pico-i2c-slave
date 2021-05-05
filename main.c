#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"


#define I2C_PORT i2c0
#define I2C_ADDR 0b00000100

#define READ_SIZE 9
#define WRITE_SIZE 15

#define MODEID_NORMAL    1 
#define MODEID_IDENTIFY  3
#define MODEID_CHALLENGE 4
#define MODEID_SERIAL    5

const uint8_t LED_PIN = 25;
const uint8_t IDENT_STRING[] = "Handrad 0001";
const uint8_t SERIAL_STRING[] = "ABCD12345678";
const uint8_t CHALLENGE_VALUE[] = {0x3B, 0x59, 0xE8, 0x2A, 0xE9, 0xB1, 0xBE, 0xD8, 0x00, 0x00, 0x00, 0x00};

uint8_t rxdata[16];
uint8_t txdata[16];
uint8_t frame_num = 0;

uint32_t current_tick()  {
    return time_us_32() / 1000;
}


void send_reply() {
    uint8_t checksum = 0;
    // increase frame number
    txdata[13] = frame_num++;
    for(uint8_t i; i < 13; i++) {
        checksum ^= txdata[i];
        checksum++;
    }
    txdata[14] = checksum;
    
    i2c_write_raw_blocking(I2C_PORT, txdata, WRITE_SIZE);
}


void mode_normal() {
    // reset
    for(uint8_t i; i < 12; i++) {
        txdata[i] = 0;
    }    
    txdata[0] = 0xFF;
    txdata[1] = 0b11111111;
    txdata[12] = MODEID_NORMAL;
    send_reply();
}

void mode_identify() {
    for(uint8_t i; i < 12; i++) {
        txdata[i] = IDENT_STRING[i];
    }
    txdata[12] = MODEID_IDENTIFY;
    send_reply();
}

void mode_challenge() {
    for(uint8_t i; i < 12; i++) {
        txdata[i] = CHALLENGE_VALUE[i];
    }    
    txdata[12] = MODEID_CHALLENGE;
    send_reply();
}

void mode_serial() {
    for(uint8_t i; i < 12; i++) {
        txdata[i] = SERIAL_STRING[i];
    }
    txdata[12] = MODEID_SERIAL;
    send_reply();
}


int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);


    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(I2C_PORT, 400 * 1000);
    i2c_set_slave_mode(I2C_PORT, true, I2C_ADDR);
    
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    for(int i=0; i < 5; i++) {
        gpio_put(LED_PIN, 0);
        sleep_ms(150);
        gpio_put(LED_PIN, 1);
    }

    puts("Started ...\n");

    while (1) {
        if (i2c_get_read_available(I2C_PORT) >= READ_SIZE) {
            i2c_read_raw_blocking (I2C_PORT, rxdata, READ_SIZE);
            
            uint8_t mode = rxdata[0];
            switch(mode) {
                case MODEID_IDENTIFY: mode_identify(); break;
                case MODEID_CHALLENGE: mode_challenge(); break;
                case MODEID_SERIAL: mode_serial(); break;
                default: mode_normal(); break;
            }            
        }            
        sleep_ms(10);                   
    }
}