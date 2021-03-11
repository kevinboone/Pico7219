/*=========================================================================
 
  Pico7219

  test.c

  A simple test driver for the Pico7219 library. This code writes
  some text, and scrolls it across the screen.

  You'll need to configure the GPIO pins used to connect the display module 
  and, perhaps, the baud rate.

  =========================================================================*/
#include <string.h>
#include <pico7219/pico7219.h> // The library's header

extern uint8_t font8_table[];
extern uint8_t font8_first;
extern uint8_t font8_last;

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

// Draw a character to the library. Note that the width of the "virtual
// display" can be much longer than the physical module chain, and
// off-display elements can later be scrolled into view. However, it's
// the job of the application, not the library, to size the virtual
// display sufficiently to fit all the text in.
//
// chr is an offset in the font table, which starts with character 32 (space).
// It isn't an ASCII character.
void draw_character (Pico7219 *pico7219, uint8_t chr, int x_offset, 
       BOOL flush)
  {
  for (int i = 0; i < 8; i++) // row
    {
    // The font elements are one byte wide even though, as its an 8x5 font,
    //   only the top five bits of each byte are used.
    uint8_t v = font8_table[8 * chr + i];
    for (int j = 0; j < 8; j++) // column
      {
      int sel = 1 << j;
      if (sel & v)
        pico7219_switch_on (pico7219, 7 - i, 7- j + x_offset, FALSE);
      }
    }
  if (flush)
    pico7219_flush (pico7219);
  }

// Draw a string of text on the (virtual) display. This function assumes
// that the library has already been configured to provide a virtual
// chain of LED modules that is long enough to fit all the text onto.
void draw_string (Pico7219 *pico7219, const char *s, BOOL flush)
  {
  int x = 0;
  while (*s)
    {
    draw_character (pico7219, *s - ' ', x, FALSE); 
    s++;
    x += 6;
    }
  if (flush)
    pico7219_flush (pico7219);
  }

// Get the number of horizontal pixels that a string will take. Since each
// font element is five pixels wide, and there is one pixel between each
// character, we just multiply the string length by 6.
int get_string_length_pixels (const char *s)
  {
  return strlen (s) * 6;
  }

// Get the number of 8x8 LED modules that would be needed to accomodate the
// string of text. That's the number of pixels divided by 8 (the module
// width), and then one added to round up. 
int get_string_length_modules (const char *s)
  {
  return get_string_length_pixels(s) / 8 + 1;
  }

// Show a string of characters, and then scroll it across the display.
// This function uses pico7219_set_virtual_chain_length() to ensure that
// there are enough "virtual" modules in the display chain to fit
// the whole string. It then scrolls it enough times to scroll the 
// whole string right off the end.
void show_text_and_scroll (Pico7219 *pico7219, const char *string)
  {
  pico7219_set_virtual_chain_length (pico7219, 
    get_string_length_modules(string));
  draw_string (pico7219, string, FALSE);
  pico7219_flush (pico7219);

  int l = get_string_length_pixels (string);

  for (int i = 0; i < l; i++)
    {
    sleep_ms (50);
    pico7219_scroll (pico7219, FALSE);
    }

  pico7219_switch_off_all (pico7219, TRUE);
  sleep_ms (500);
  }

//
// Start here
//
int main()
  {
  // Create the Pico7219 object, specifying the connected pins and
  //  baud rate. The last parameter indicates whether the column order
  //  should be reversed -- this depends on how the LED matrix is wired
  //  to the MAX7219, and isn't easy to determine except by trying.
  Pico7219 *pico7219 = pico7219_create (SPI_CHAN, 1500 * 1000,
    MOSI, SCK, CS, CHAIN_LEN, FALSE);

  pico7219_switch_off_all (pico7219, FALSE);

    
  while (1) // Forever
    {
    // Each string starts with some spaces, so that it scrolls into 
    // view, rather than just appearing at the left of the display.
    show_text_and_scroll (pico7219, "    The boy stood on the burning deck"); 
    show_text_and_scroll (pico7219, "    The heat did make him quiver");
    show_text_and_scroll (pico7219, "    He gave a cough, his leg fell off");
    show_text_and_scroll (pico7219, "    And floated down the river");
    }

  // For completeness, but we never get here...
  pico7219_destroy (pico7219, FALSE);
  }


