/*=========================================================================
 
  Pico7219

  pico7219.c

  A low-level driver for chained display modules based on the MAX7219.
  See the corresponding header file for a description of how the 
  functions are used. If this library is built in "host" mode, the 
  actual GPIO operations are replaced by printouts of the pins of the
  operations that would be carried out.

  Copyright (c)2021 Kevin Boone, GPL v3.0

  =========================================================================*/
#include <stdlib.h>
#include <string.h>

#if PICO_ON_DEVICE
#include "hardware/spi.h"
#include "hardware/gpio.h"
#else
#include <stdio.h> // For printf(). Don't need this in the Pico build
#endif

#include "pico7219/pico7219.h"

#define PICO7219_INTENSITY_REG 0x0A
#define PICO7219_SHUTDOWN_REG 0x0C

// An opaque data structure that holds the information relevant to the
//   library. Users of the library do not see this, or need to. 

struct Pico7219
  {
  uint8_t spi_num; // 0 or 1
  uint8_t cs; // Chip select GPIO pin
  uint8_t chain_len; // Number of chained devices
  BOOL reverse_bits; // TRUE is we must reverse output->layout order
#if PICO_ON_DEVICE
  spi_inst_t* spi; // The Pico-specific SPI device
#endif
  // data is an array of bits that represents the states of the 
  //   individual bits. They are packed into 8-bit chunks, which is
  //   how the need to be written to the hardware, as well as saving
  //   space.
  uint8_t data[PICO7219_ROWS][PICO7219_MAX_CHAIN];
  uint8_t row_dirty [PICO7219_ROWS]; // TRUE for each row to be flushed
  uint8_t *vdata;
  // Length of the "virtual chain" of modules
  int vchain_len;
  };

/** Change the state of the chip-select line, allowing a very short
    time for it to settle. */
static void pico7219_cs (const struct Pico7219 *self, uint8_t select)
  {
#if PICO_ON_DEVICE
  asm volatile("nop \n nop \n nop");
  gpio_put (self->cs, select);  
  asm volatile("nop \n nop \n nop");
#else
  printf ("Set GPIO %d = %d\n", self->cs, select);
#endif
  }

/** write_word_to_chain() outputs the same 16-bit word as many times
    as there are modules in the chain. This is mostly used for
    initialization -- each module will be initialized with the same
    values, so we must repeat the data output enough times that each
    module gets a copy. */
static void pico7219_write_word_to_chain (const struct Pico7219 *self, 
        uint8_t hi, uint8_t lo)
  {
  pico7219_cs (self, 0); 
#if PICO_ON_DEVICE
  uint8_t buf[] = {hi, lo};
  for (int i = 0; i < self->chain_len; i++)
    spi_write_blocking (self->spi, buf, 2);
#else
  (void)hi; (void)lo;
#endif
  pico7219_cs (self, 1); 
  }

/* init() sends the same set of initialization values to all modules
   in the chain. We write zero to all the row buffers, and set
   reasonable values for the control registers. */
static void pico7219_init (const struct Pico7219 *self)
  {
  // Rows
  pico7219_write_word_to_chain (self, 0x00, 0x00); 
  pico7219_write_word_to_chain (self, 0x01, 0x00); 
  pico7219_write_word_to_chain (self, 0x02, 0x00); 
  pico7219_write_word_to_chain (self, 0x03, 0x00); 
  pico7219_write_word_to_chain (self, 0x04, 0x00); 
  pico7219_write_word_to_chain (self, 0x05, 0x00); 
  pico7219_write_word_to_chain (self, 0x06, 0x00); 
  pico7219_write_word_to_chain (self, 0x07, 0x00); 
  pico7219_write_word_to_chain (self, 0x08, 0x00); 
  // Control registers
  pico7219_write_word_to_chain (self, 0x09, 0x00); // Decode mode 
  pico7219_write_word_to_chain (self, PICO7219_INTENSITY_REG, 0x01); 
  pico7219_write_word_to_chain (self, 0x0b, 0x07); // scan limit = full 
  pico7219_write_word_to_chain (self, PICO7219_SHUTDOWN_REG, 0x01); // Run 
  pico7219_write_word_to_chain (self, 0x0f, 0x00); // Display test = off 
  }

/** pico7219_create_vdata(). Create enough space for a "virtual"
    chain of 8x8 displays, whose size is */
void pico7219_set_virtual_chain_length (struct Pico7219 *self, int chain_len)
  {
  if (self->vdata) free (self->vdata);
  self->vdata = malloc (PICO7219_ROWS * chain_len);
  memset (self->vdata, 0, PICO7219_ROWS * chain_len);
  self->vchain_len = chain_len;
  }

/** pico7219_create() */
struct Pico7219 *pico7219_create (enum PicoSpiNum spi_num, int32_t baud,
         uint8_t mosi, uint8_t sck, uint8_t cs, uint8_t chain_len, 
	 BOOL reverse_bits)
  {
  struct Pico7219 *self = malloc (sizeof (struct Pico7219));  
  if (self)
    {
    self->chain_len = chain_len;
    self->cs = cs;
    self->spi_num = spi_num;
    self->reverse_bits = reverse_bits;
    self->vdata = NULL;
    self->vchain_len = 0;
    // Start with the virtual chain length the same as the maximum 
    //  physical chain length
    pico7219_set_virtual_chain_length (self, PICO7219_MAX_CHAIN);
    // Set data buffer to all "off", as that's how the LEDs power up
    memset (self->data, 0, sizeof (self->data));
    // Set all data clean
    memset (self->row_dirty, 0, sizeof (self->row_dirty));
#if PICO_ON_DEVICE
    switch (spi_num)
      {
      case PICO_SPI_0:
        self->spi = spi0;
	break;
      case PICO_SPI_1:
        self->spi = spi1;
	break;
      }

    // Initialize the SPI and GPIO 
    
    spi_init (self->spi, baud); 

    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(sck, GPIO_FUNC_SPI);

    gpio_init (self->cs);
    gpio_set_dir (self->cs, GPIO_OUT);
    gpio_put (self->cs, 1);
#else
printf ("Init SPI %d at %d baud, mosi=%d, sck=%d, cs=%d\n", 
     self->spi_num, baud, mosi, sck, self->cs);
#endif

    // Initialize the hardware
    pico7219_init (self);
    }
  return self;
  }

/** pico7219_destroy() */
void pico7219_destroy (struct Pico7219 *self, BOOL deinit)
  {
  if (self)
    {
    if (self->vdata) free (self->vdata);
    pico7219_write_word_to_chain (self, PICO7219_SHUTDOWN_REG, 0x00); // off 
    if (deinit)
      {
#if PICO_ON_DEVICE
      spi_deinit (self->spi);
#endif
      }
    free (self);
    }
  }

/** reverse_bits() reverses the order of bits in a specific byte. */
static uint8_t pico7219_reverse_bits (uint8_t b)
  {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
  }

/** pico7219_set_row_bits(). */
void pico7219_set_row_bits (const struct Pico7219 *self, uint8_t row, 
        const uint8_t bits[PICO7219_MAX_CHAIN]) 
  {
  pico7219_cs (self, 0); 
  int chain_len = self->chain_len;
  for (int i = 0; i < chain_len; i++)
    {
    uint8_t v = bits[chain_len - i - 1];
    if (self->reverse_bits)
      v = pico7219_reverse_bits (v);
    uint8_t buf[] = {row + 1, v};
#if PICO_ON_DEVICE
    spi_write_blocking (self->spi, buf, 2);
#else
    printf ("SPI write %02x %02x\n", buf[0], buf[1]);
#endif
    }
  pico7219_cs (self, 1); 
  }

/** pico7219_switch_off_row() */
void pico7219_switch_off_row (struct Pico7219 *self, uint8_t row, BOOL flush)
  {
  self->row_dirty[row] = TRUE;
  //fprintf (stderr, "row = %d len=%d\n", row, self->vchain_len);
  memset (self->vdata + row * self->vchain_len, 0x0, self->vchain_len);
  if (flush) pico7219_flush (self);
  }

/** pico7219_switch_off_all() */
void pico7219_switch_off_all (struct Pico7219 *self, BOOL flush)
  {
  for (int i = 0; i < PICO7219_ROWS; i++)
    pico7219_switch_off_row (self, i, FALSE);
  if (flush) pico7219_flush (self);
  }

/** pico7219_switch_on_row() */
void pico7219_switch_on_row (struct Pico7219 *self, uint8_t row, BOOL flush)
  {
  self->row_dirty[row] = TRUE;
  memset (self->vdata + row * self->vchain_len, 0xFF, self->vchain_len);
  if (flush) pico7219_flush (self);
  }

/** pico7219_switch_on_all() */
void pico7219_switch_on_all (struct Pico7219 *self, BOOL flush)
  {
  for (int i = 0; i < PICO7219_ROWS; i++)
    pico7219_switch_on_row (self, i, FALSE);
  if (flush) pico7219_flush (self);
  }

/** pico7219_switch_on() */
void pico7219_switch_on (struct Pico7219 *self, uint8_t row, 
       uint8_t col, BOOL flush)
  {
  if (row < PICO7219_ROWS && col < PICO7219_COLS * self->vchain_len)
    {
    int block = col / 8;
    int pos = col - 8 * block;
    uint8_t v = 1 << pos;
    self->vdata[row * self->vchain_len + block] |= v;
    self->row_dirty[row] = TRUE;
    if (flush) pico7219_flush (self);
    } 
  }

/** pico7219_switch_off() */
void pico7219_switch_off (struct Pico7219 *self, uint8_t row, 
       uint8_t col, BOOL flush)
  {
  if (row < PICO7219_ROWS && col < PICO7219_COLS * self->vchain_len)
    {
    int block = col / 8;
    int pos = col - 8 * block;
    uint8_t v = 1 << pos;
    self->vdata[row * self->vchain_len + block] &= ~v;
    self->row_dirty[row] = TRUE;
    if (flush) pico7219_flush (self);
    }
  }

/** Copy from the virtual chain to self->data, preparatory to 
    writing to the device. This function will only write the start
    of the virtual chain, if it is longer than the physical chain. */
static void pico7219_vrow_to_row (struct Pico7219 *self, int row)
  {
  int target_mods = self->chain_len;
  if (target_mods > self->vchain_len) target_mods = self->vchain_len;
  int row_start = row * self->vchain_len;
  for (int i = 0; i < target_mods; i++)
    {
    self->data[row][i] = self->vdata[row_start + i];
    }
  }

/** Scroll one pixel left. */
void pico7219_scroll (struct Pico7219 *self, BOOL wrap)
  {
  int target_mods = self->chain_len;
  if (target_mods > self->vchain_len) target_mods = self->vchain_len;

  // Shift bits in vdata
  // This logic is twisted because the bits are in MSB-LSB order in the 
  //   opposite order from the modules. So when we shift a bit rightwards
  //   off the end of one module, it appears as the MSB in the next, not
  //   the LSB.
  for (int row = 0; row < PICO7219_ROWS; row++)
    {
    int row_start = row * self->vchain_len;
    uint8_t carry = 0;
    int l = self->vchain_len - 1;
    for (int i = l; i >= 0; i--)
      {
      int row_offset = row_start + i;
      BOOL carry_next = FALSE;

      if (self->vdata[row_offset] & 0x01) carry_next = TRUE;
      self->vdata[row_offset] >>= 1; 

      // If we're at position 0, and the shift would carry, we have to
      //   carry to position "-1" which, of course, does not exist. So,
      //   instead, we carry to position l, that is, to the far end of
      //   the chain.
      if (wrap)
        {
        if (i == 0 && carry_next)
          self->vdata[row_start + l] |= 0x80;
        }

      self->vdata[row_offset] |= carry; 
      carry = 0;

      if (carry_next)
	carry = 0x80;
      }

    pico7219_set_row_bits (self, row, self->vdata + row * self->vchain_len);
    }
  }

/** pico7219_flush() */
void pico7219_flush (struct Pico7219 *self)
  {
  for (int i = 0; i < PICO7219_ROWS; i++)
    {
    pico7219_vrow_to_row (self, i);
    if (self->row_dirty[i])
      pico7219_set_row_bits (self, i, self->data[i]);
    self->row_dirty[i] = FALSE;
    }
  }

/** pico7219_set_intensity() */
void pico7219_set_intensity (struct Pico7219 *self, uint8_t intensity)
  {
  pico7219_write_word_to_chain (self, PICO7219_INTENSITY_REG, intensity); 
  }


