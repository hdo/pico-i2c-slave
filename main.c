#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"


#define I2C_PORT i2c0
#define I2C_ADDR 0b00000100

#define READ_SIZE 9
#define WRITE_SIZE 15

#define MODEID_NORMAL    1 
#define MODEID_IDENTIFY  3
#define MODEID_CHALLENGE 4
#define MODEID_SERIAL    5


// define the hardware registers used
volatile uint32_t * const I2C0_DATA_CMD       = (volatile uint32_t * const)(I2C0_BASE + 0x10);
volatile uint32_t * const I2C0_INTR_STAT      = (volatile uint32_t * const)(I2C0_BASE + 0x2c);
volatile uint32_t * const I2C0_INTR_MASK      = (volatile uint32_t * const)(I2C0_BASE + 0x30);
volatile uint32_t * const I2C0_CLR_RD_REQ     = (volatile uint32_t * const)(I2C0_BASE + 0x50);
volatile uint32_t * const I2C0_CLR_STOP_DET   = (volatile uint32_t * const)(I2C0_BASE + 0x60);
volatile uint32_t * const I2C0_CLR_START_DET  = (volatile uint32_t * const)(I2C0_BASE + 0x64);

// Declare the bits in the registers we use
#define I2C_DATA_CMD_FIRST_BYTE  0x00000800
#define I2C_DATA_CMD_DATA        0x000000ff
#define I2C_INTR_STAT_READ_REQ   0x00000020
#define I2C_INTR_STAT_RX_FULL    0x00000004
#define I2C_INTR_STAT_START_DET  (1 << 10)   // bit 10
#define I2C_INTR_MASK_READ_REQ   0x00000020  // bit 5 (1 << 5)
#define I2C_INTR_MASK_RX_FULL    0x00000004
#define I2C_INTR_MASK_START_DET  (1 << 10)   // bit 10

#define I2C_INTR_STAT_STOP_DET  (1 << 9)   
#define I2C_INTR_MAKS_STOP_DET  (1 << 9)   


const uint8_t LED_PIN = 25;
const uint8_t IDENT_STRING[] = "Handrad 0001";
const uint8_t SERIAL_STRING[] = "ABCD12345678";
const uint8_t CHALLENGE_VALUE[] = {0x3B, 0x59, 0xE8, 0x2A, 0xE9, 0xB1, 0xBE, 0xD8, 0x00, 0x00, 0x00, 0x00};

uint8_t rx_head = 0;
uint8_t tx_head = 0;
uint8_t rxdata[16];
uint8_t txdata[16];
uint8_t frame_num = 0;

uint32_t current_tick()  {
    return time_us_32() / 1000;
}


void printhex(uint8_t *buffer, uint8_t length) {
    for(uint8_t i = 0; i < length; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\r\n");
}

void i2c0_irq_handler() {
    // Get interrupt status
    uint32_t status = *I2C0_INTR_STAT;

    // check for start condition
    if (status & I2C_INTR_STAT_START_DET) {
        // reset heads
        tx_head = 0;
        rx_head = 0;
        *I2C0_CLR_START_DET;
    }

    // check for stop condition
    if (status & I2C_INTR_STAT_STOP_DET) {
        // reset heads
        tx_head = 0;
        rx_head = 0;
        *I2C0_CLR_STOP_DET;
    }

    // Check to see if we have received data from the I2C master
    if (status & I2C_INTR_STAT_RX_FULL) {
        // Read the data (this will clear the interrupt)
        uint32_t value = *I2C0_DATA_CMD;
        // Check if this is the 1st byte we have received
        if (value & I2C_DATA_CMD_FIRST_BYTE) {
            // 
            rx_head = 0;
            // If so treat it as the address to use
          
        } 
        rxdata[rx_head++] = (uint8_t)(value & I2C_DATA_CMD_DATA);

        if (rx_head == READ_SIZE) {
            printhex(rxdata, READ_SIZE);
        }

        if (rx_head > READ_SIZE) {
            rx_head = 0;
            puts("rx out of bounds");
        }
    }

    // Check to see if the I2C master is requesting data from us
    if (status & I2C_INTR_STAT_READ_REQ) {
        // Write the data from the current address in RAM
        *I2C0_DATA_CMD = (uint32_t) txdata[tx_head++];
        // Clear the interrupt
        *I2C0_CLR_RD_REQ;
        if (tx_head > WRITE_SIZE) {
            tx_head = 0;
            puts("tx out of bounds");
        }
    }
}


void prepare_reply() {
    uint8_t checksum = 0;
    // increase frame number
    txdata[13] = frame_num++;
    for(uint8_t i; i < 13; i++) {
        checksum ^= txdata[i];
        checksum++;
    }
    txdata[14] = checksum;    
}


void mode_normal() {
    // reset
    for(uint8_t i; i < 12; i++) {
        txdata[i] = 0;
    }    
    txdata[0] = 0xFF;
    txdata[1] = 0b11111111;
    txdata[12] = MODEID_NORMAL;
    prepare_reply();
}

void mode_identify() {
    for(uint8_t i; i < 12; i++) {
        txdata[i] = IDENT_STRING[i];
    }
    txdata[12] = MODEID_IDENTIFY;
    prepare_reply();
}

void mode_challenge() {
    for(uint8_t i; i < 12; i++) {
        txdata[i] = CHALLENGE_VALUE[i];
    }    
    txdata[12] = MODEID_CHALLENGE;
    prepare_reply();
}

void mode_serial() {
    for(uint8_t i; i < 12; i++) {
        txdata[i] = SERIAL_STRING[i];
    }
    txdata[12] = MODEID_SERIAL;
    prepare_reply();
}


void setup_mock() {
    for(uint8_t i=0; i < sizeof(txdata); i++) {
        txdata[i] = i;
    }
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

    setup_mock();


    // blinky
    for(int i=0; i < 3; i++) {
        gpio_put(LED_PIN, 0);
        sleep_ms(150);
        gpio_put(LED_PIN, 1);
    }

    puts("Started ...\n");
    printhex(txdata, 15);


    // Enable the interrupts we want
    *I2C0_INTR_MASK = (I2C_INTR_MASK_READ_REQ | I2C_INTR_MASK_RX_FULL | I2C_INTR_MASK_START_DET);
    //*I2C0_INTR_MASK = (I2C_INTR_MASK_READ_REQ | I2C_INTR_MASK_RX_FULL);

    // Set up the interrupt handlers
    irq_set_exclusive_handler(I2C0_IRQ, i2c0_irq_handler);
    // Enable I2C interrupts
    irq_set_enabled(I2C0_IRQ, true);


    while (1) {
        puts("ping");
        sleep_ms(500);                   
    }
}