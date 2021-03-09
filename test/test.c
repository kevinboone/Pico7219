/*=========================================================================
 
  Pico7219

  test.c

  A simple test driver for the Pico7219 library. This code just illuminates
  each row in the LED matrix, one at a time. You'll need to configure
  the GPIO pins used to connect the display module and, perhaps, 
  the baud rate.

  =========================================================================*/
#include <pico7219/pico7219.h> // The library's header

//
// Pin assignments
//

// MOSI pin, also called "TX" in the Pico pinout. Connects to the
//   "DIN" pin on the MAX7219
#define MOSI 19

// SCK pin. Connects to the "CLOCK" pin on the MAX7219
#define SCK 18

// Chip-select pin. The same name is used on the Pico and the MAX7219
#define CS 17

// Number of 8x8 modules in the display chain
#define CHAIN_LEN 4

// SPI channel can be 0 or 1, and depends on the pins you've wired
//   to MOSI, etc (see pinout diagram)
#define SPI_CHAN 0

typedef struct Pico7219 Pico7219; // Shorter than "struct Pico7219..."

//
// Start here
//
int main()
  {
  // Create the Pico7219 object, specifying the connected pins and
  //  baud rate. The last parameter indicates whether the column order
  //  should be reversed -- this depends on how the LED matrix is wired
  //  to the MAX7219, and isn't easy to determine except by trying.
  Pico7219 *pico7219 = pico7219_create (SPI_CHAN, 1000 * 1000,
    MOSI, SCK, CS, CHAIN_LEN, FALSE);

  while (1) // Forever
    {
    // Row and column indices in the Pico7219 library are zero-based
    for (int row = 0; row < 8; row++)
      {
      for (int col = 0; col < CHAIN_LEN * PICO7219_COLS; col++)
        {
        pico7219_switch_off_all (pico7219, FALSE);
        pico7219_switch_on (pico7219, row, col, TRUE);
        sleep_ms (50);
        }
      }
    }

  // For completeness, but we never get here...
  pico7219_destroy (pico7219, FALSE);
  }


