# Pico7219

A low-level library for the Raspberry Pi Pico, for controlling chained 
LED matrices based on the MAX7219 IC.

This library for the Raspberry Pi Pico allows SPI control of a chained set of 8x8 LED matrices, each of which is managed by a MAX7219. Pre-built displays of
the type are widely available and inexpensive. 

The library constists of two files: `pico7219.c` and `pico7219.h`. 
The `.h` file contains descriptions of the functions exposed to clients.
See `test.c` for an example of how the library can be used.

This is a low-level library: it allows individual LEDs in the matrix to
be turned on and turned off. It also exposes a whole-row write function
that might be useful in some applications. Anything clever, like displaying
text or graphics, or scrolling the display, will need to be done by 
additional code.

For a description how this library works, and how to connect a Pico
to a compatible display module, see my website:

http://kevinboone.me/pico7219.html


