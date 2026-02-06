#include "hardware/spi.h"

#define SPI_PORT spi0

#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  2
#define PIN_MOSI 3

uint8_t rxBuf[10];
uint8_t txBuf[10];

void prepareACK()
{
    txBuf[0] = 'A';
    txBuf[1] = 'T';
    txBuf[2] = 0x09;
    txBuf[3] = 0x00;
    txBuf[4] = 0x00;
    txBuf[5] = 0x00;
    txBuf[6] = 0x00;
    txBuf[7] = 0x00;
    txBuf[8] = 0x00;
    txBuf[9] = 0x01;
}

void setup()
{
    Serial.begin(115200);

    spi_init(SPI_PORT, 1000 * 1000);

    spi_set_slave(SPI_PORT, true);

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);

    prepareACK();
}

void loop()
{
    // wait for CS low (transaction start)
    if (gpio_get(PIN_CS) == 0)
    {
        // -------- Receive command (10 bytes) --------
        spi_read_blocking(SPI_PORT, 0x00, rxBuf, 10);

        // Optional: validate header
        if (rxBuf[0] == 'A' && rxBuf[1] == 'T')
        {
            Serial.print("CMD: ");
            for(int i=0;i<10;i++){
                Serial.print(rxBuf[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            prepareACK();
        }

        // -------- Send ACK during next 10 clocks --------
        uint8_t dummy[10] = {0};

        spi_write_read_blocking(SPI_PORT, txBuf, dummy, 10);

        // wait until CS released
        while (gpio_get(PIN_CS) == 0);
    }
}
