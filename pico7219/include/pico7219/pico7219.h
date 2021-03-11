/*=========================================================================
  
  Pico7219

  pico7219.h

  A low-level library for controlling LED matrixes based on chained
  8x8 modules including MAX7219 ICs.

  See the accompanying test.c to see how this library should be used.

  The functions in this library that turn on and turn off LEDs all
  have a "flush" argument. If flush is TRUE, then changes are written
  to the hardware immediately. As it may be necessary to write an 
  entire row to change one LED, it's much more efficient to group
  changes and called flush() at the end.

  Note the row and column numbers in all functions are zero-based. The
  "bottom-left" corner is (0,0) although, of course, the display modules
  can be rotated so this might be the top right in some installations.

  At present, this library doesn't have a way to set the display module to
  stand-by mode, except by calling pico7219_destroy() to tidy it up.
  Since entering stand-by requires reinitializing the hardware, this
  is probably (but not definitely) the right approach.

  The library supports the notion of a "virtual" chain of display
  modules. When LEDs are turned on and off, they are written to this
  virtual chain, which can be much longer than the real display --
  as long as memory allows. Only the part of the virtual chain that
  fits on the physical display chain will be shown when the LEDs are
  first set, but content that won't fit can be scrolled into view by
  calling pico7219_scroll repeatedly. 

  Copyright (c)2021 Kevin Boone, GPL v3.0

  =========================================================================*/
#pragma once

#include <stdint.h>
#if PICO_ON_DEVICE
#include "hardware/spi.h"
#endif 

#ifndef BOOL
typedef uint8_t BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// Maximum devices in chain. This can usefully be increased, at the expense
//  of using a little more memory.
#define PICO7219_MAX_CHAIN 8

// Number of LEDs in each row of column. This is a feature of the
//   MAX7219, and can't usefull be changed
#define PICO7219_ROWS 8
#define PICO7219_COLS 8

// An enum to denote the SPI channel to use. This is to avoid exposing
//   client classes to the low-level API provided by the Pico SDK
enum PicoSpiNum 
  {
  PICO_SPI_0 = 0,
  PICO_SPI_1
  };

struct Pico7219;

#ifdef __cplusplus
extern "C" { 
#endif

/** pico7219_create() -- create the Pico7219 structure, storing references
    to the hardware parameters. Then initialize the hardware, and set the
    MAX2719 to "running" mode.  The Pico SDK provides no way to tell whether
    initialization succeeded or not, so the only way this function can fail
    is to run out of memory. In tha case, it returns NULL. If it doesn't
    return NULL, use pico7219_destroy() to tidy up. */
extern struct Pico7219 *pico7219_create (enum PicoSpiNum spi_num, 
                           int32_t baud, uint8_t mosi, 
			   uint8_t sck, uint8_t cs, uint8_t chain_len,
			   BOOL reverse_bits);

/** Clean up the library. If "deinit" is TRUE, the corresponding SPI
    channel in the Pico is deinitialized. In either case, set the 
    display hardware to the low-power standby mode. */
extern void             pico7219_destroy (struct Pico7219 *self, BOOL deinit);

/** Write a whole row in one operation. The bits[] argument is an array
    of bytes, where each byte represents a set of on/off states in
    specific columns. bits[0] represents the module at the end of
    the chain nearest the input, however long the chain is. The LSB
    of bits[0] is the either the LSB of the 7219 outputs or the
    MSB, dependending on whether the object was created with 
    bit-reverse mode or not. This is a low-level function, intended
    for clients for which the build-in set_output() and clear_output()
    methods are not fast enough. It does not change the internal state
    of the library at all. */
extern void             pico7219_set_row_bits (const struct Pico7219 *self, 
                          uint8_t row, 
			  const uint8_t bits[PICO7219_MAX_CHAIN]); 

/** Turn on the LED at a particular row and column. If flush is TRUE,
    changes are written immediately to the hardware. Otherwise they are
    buffered for a later call to flush(). */
extern void             pico7219_switch_on (struct Pico7219 *self, 
                          uint8_t row, uint8_t col, BOOL flush);

/** Turn off the LED at a particular row and column. If flush is TRUE,
    changes are written immediately to the hardware. Otherwise they are
    buffered for a later call to flush(). */
extern void             pico7219_switch_off (struct Pico7219 *self, 
                          uint8_t row, uint8_t col, BOOL flush);

/** Turn off all the LEDs in a row. If flush is TRUE,
    changes are written immediately to the hardware. Otherwise they are
    buffered for a later call to flush(). */
extern void pico7219_switch_off_row (struct Pico7219 *self, uint8_t row, 
                          BOOL flush);

/** Turn off all the LEDs in the display. If flush is TRUE,
    changes are written immediately to the hardware. Otherwise they are
    buffered for a later call to flush(). */
extern void pico7219_switch_off_all (struct Pico7219 *self, BOOL flush);

/** Turn on all the LEDs in a row. If flush is TRUE,
    changes are written immediately to the hardware. Otherwise they are
    buffered for a later call to flush(). */
extern void pico7219_switch_on_row (struct Pico7219 *self, uint8_t row, 
                          BOOL flush);

/** Turn on all the LEDs in the display. If the module is powered by
    USB, the supply might not be adequate to switch on all LEDs at
    high intensity. */
extern void pico7219_switch_on_all (struct Pico7219 *self, BOOL flush);

/** Write buffered LED state changes to the hardware. */
extern void pico7219_flush (struct Pico7219 *self);

/** Set the LED brightness in the range 0-15. Default is 1. Note that
 * there is no "off" setting -- even 0 has some illumination. */
extern void pico7219_set_intensity (struct Pico7219 *self, uint8_t intensity);

/** Scroll the virtual module chain one pixel (LED) to the left. The part
      of the virtual chain that fits on the display will be shown. If
      wrap is TRUE, pixels that are scrolled off the display are redrawn
      on the end (of the virtual chain) and may eventually be scrolled back
      into view.
    This function is design to be used alone. Writing new data to the module
      while scrolling will have odd results. Calling flush() will restore
      the non-scrolled state.
    Note that the display will scroll even if the text will fit on the module.
      Don't ask for it if you don't want it. */
extern void pico7219_scroll (struct Pico7219 *self, BOOL wrap);

/** Set the number of "virtual modules" in the display chain. This can be
      any length (subject to memory), but it makes little sense to set
      this smaller than the actual display. The purpose of setting the
      virtual length is to be able to write content that will not fit
      onto the physical display, and then call scroll() to bring it 
      into view. By default, the virtual chain length is the same as
      the predefined maximum physical chain length, that is, 8 modules. 
      If you don't plan to use the scrolling function, you can save a
      little memory by setting the virtual chain length to the actual
      chain length. But we're talking bytes here. */
extern void pico7219_set_virtual_chain_length (struct Pico7219 *self, 
   int chain_len);

#ifdef __cplusplus
} 
#endif


