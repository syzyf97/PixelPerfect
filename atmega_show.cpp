#if defined(AVR) || defined(__AVR__)
#include "LED.h"

static uint32_t endTime = 0u;

namespace pix
{
  // Byte-oriented seam: byteLen = pixels * bytes-per-pixel (3 for RGB, 4 for RGBW).
  void _extern_timer_show(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer, uint8_t bright)
  {
    if (nullptr == bytes || byteLen == 0) return;
    volatile uint16_t count = byteLen;
    volatile uint8_t *port = portOutputRegister(digitalPinToPort(pin));
    if (nullptr == port) return;
    uint8_t const pinMask = digitalPinToBitMask(pin);
    volatile uint8_t *ptr = (volatile uint8_t*)bytes;
    volatile uint8_t hi = *port | pinMask, lo = *port & ~pinMask;
    volatile uint8_t color = *ptr++;
    color = (color * bright + color) >> 8; // fastScale
    volatile uint8_t color_2sc = *ptr++;
    --count;
    volatile const uint8_t scale = 0;
    // the code below looks strange, but unfortunately this procedure had to be used 
    // because the variable had to be in register > 15, and the compiler doesn't know about it
    register uint8_t bit_count asm("r18");
    while (micros() - timer < 50) {}
    noInterrupts();
    
    asm volatile(
      "init_transmit: \n\t"
      "lsl %[color] \n\t"
      "brcc first_bit_0 \n\t"
      "rjmp first_bit_1 \n\t"

      "first_bit_0: \n\t"                 // [0] -> 6H / 14L
      "nop \n\t"                          // 1
      "st %a[port], %[hi] \n\t"           // 3 add to prev state -> set to hi
      "mul %[color_2sc], %[bright] \n\t"  // 2
      "add r0, %[color_2sc] \n\t"         // 3
      "adc r1, %[scale] \n\t"             // 4
      "st %a[port], %[lo] \n\t"           // 6 -> set to lo
      "mov %[color_2sc], r1 \n\t"         // 1
      "clr r1 \n\t"                       // 2
      "ldi %[bit_count], 0x07 \n\t"       // 3
      "nop \n\t"                          // 4
      "nop \n\t"                          // 5
      "nop \n\t"                          // 6
      "nop \n\t"                          // 7
      "nop \n\t"                          // 8
      "lsl %[color] \n\t"                 // 9
      "brcc next_bit_0 \n\t"              // 10 | 11 if jump
      "rjmp next_bit_1 \n\t"              // 12

      "first_bit_1: \n\t"                 // [1] -> 13H / 7L
      "st %a[port], %[hi] \n\t"           // 2 add to prev state -> set to hi
      "mul %[color_2sc], %[bright] \n\t"  // 2
      "add r0, %[color_2sc] \n\t"         // 3
      "adc r1, %[scale] \n\t"             // 4
      "mov %[color_2sc], r1 \n\t"         // 5
      "clr r1 \n\t"                       // 6
      "ldi %[bit_count], 0x07 \n\t"       // 7
      "nop \n\t"                          // 8
      "nop \n\t"                          // 9
      "nop \n\t"                          // 10
      "nop \n\t"                          // 11
      "st %a[port], %[lo] \n\t"           // 13 -> set to lo
      "nop \n\t"                          // 1
      "lsl %[color] \n\t"                 // 2
      "brcc next_bit_0 \n\t"              // 3 | 4 if jump
      "rjmp next_bit_1 \n\t"              // 5

      "next_bit_0: \n\t"                  // [0] -> 6H / 14L
      "nop \n\t"                          // 1
      "st %a[port], %[hi] \n\t"           // 3 add to prev state -> set to hi
      "nop \n\t"                          // 1
      "nop \n\t"                          // 2
      "nop \n\t"                          // 3
      "nop \n\t"                          // 4
      "st %a[port], %[lo] \n\t"           // 6 -> set to lo
      "nop \n\t"                          // 1
      "nop \n\t"                          // 2
      "nop \n\t"                          // 3
      "nop \n\t"                          // 4
      "dec %[bit_count] \n\t"             // 5
      "cpi %[bit_count], 0x01 \n\t"       // 6
      "breq bridge_0_to_last_bit \n\t"    // 7 | 8 if jump
      "nop \n\t"                          // 8
      "lsl %[color] \n\t"                 // 9
      "brcc next_bit_0 \n\t"              // 10 | 11 if jump
      "rjmp next_bit_1 \n\t"              // 12

      "bridge_0_to_last_bit: \n\t"        // end with 8 clk
      "lsl %[color] \n\t"                 // 9
      "brcc last_bit_0 \n\t"              // 10 | 11 if jump
      "rjmp last_bit_1 \n\t"              // 12

      "next_bit_1: \n\t"                  // [1] -> 13H / 7L
      "st %a[port], %[hi] \n\t"           // 2 add to prev state -> set to hi
      "nop \n\t"                          // 1
      "nop \n\t"                          // 2
      "nop \n\t"                          // 3
      "nop \n\t"                          // 4
      "nop \n\t"                          // 5
      "nop \n\t"                          // 6
      "nop \n\t"                          // 7
      "dec %[bit_count] \n\t"             // 8
      "cpi %[bit_count], 0x01 \n\t"       // 9
      "breq bridge_1_to_last_bit \n\t"    // 10 | 11 if jump
      "nop \n\t"                          // 11
      "st %a[port], %[lo] \n\t"           // 13 -> set to lo
      "nop \n\t"                          // 1
      "lsl %[color] \n\t"                 // 2
      "brcc next_bit_0 \n\t"              // 3 | 4 if jump
      "rjmp next_bit_1 \n\t"              // 5 

      "bridge_1_to_last_bit: \n\t"        // end with 11 clk
      "st %a[port], %[lo] \n\t"           // 13 -> set to lo
      "nop \n\t"                          // 1
      "lsl %[color] \n\t"                 // 2
      "brcc last_bit_0 \n\t"              // 3 | 4 if jump
      "rjmp last_bit_1 \n\t"              // 5

      "last_bit_0: \n\t"                  // [0] -> 6H / 14L
      "nop \n\t"                          // 1
      "st %a[port], %[hi] \n\t"           // 3 add to prev state -> set to hi
      "mov %[color], %[color_2sc] \n\t"   // 1
      "nop \n\t"                          // 2
      "nop \n\t"                          // 3
      "nop \n\t"                          // 4
      "st %a[port], %[lo] \n\t"           // 6 -> set to lo
      "sbiw %[count], 0x00 \n\t"          // 2
      "breq end_with_0 \n\t"              // 3 | 4 if jump
      "ld %[color_2sc], %a[ptr]+ \n\t"    // 5
      "sbiw %[count], 0x01 \n\t"          // 7
      "lsl %[color] \n\t"                 // 8
      "brcs bridge_to_first_bit_1 \n\t"   // 9 | 10 if jump
      "rjmp first_bit_0 \n\t"             // 11

      "end_with_0: \n\t"                  // end with 4 clk
      "nop \n\t"                          // 5
      "nop \n\t"                          // 6
      "nop \n\t"                          // 7
      "nop \n\t"                          // 8
      "nop \n\t"                          // 9
      "nop \n\t"                          // 10
      "nop \n\t"                          // 11
      "rjmp end_transmit \n\t"            // 13

      "bridge_to_first_bit_1: \n\t"       
      "rjmp first_bit_1 \n\t"             // 2 add to prev state

      "last_bit_1: \n\t"                  // [1] -> 13H / 7L
      "st %a[port], %[hi] \n\t"           // 2 add to prev state -> set to hi
      "mov %[color], %[color_2sc] \n\t"   // 1
      "nop \n\t"                          // 2 
      "nop \n\t"                          // 3 
      "nop \n\t"                          // 4
      "sbiw %[count], 0x00 \n\t"          // 6
      "breq end_with_1 \n\t"              // 7 | 8 if jump
      "ld %[color_2sc], %a[ptr]+ \n\t"    // 9
      "sbiw %[count], 0x01 \n\t"          // 11
      "st %a[port], %[lo] \n\t"           // 13 -> set to lo
      "lsl %[color] \n\t"                 // 1
      "brcs bridge_to_first_bit_1 \n\t"   // 2 | 3 if jump
      "rjmp first_bit_0 \n\t"             // 4
      
      "end_with_1: \n\t"                  // end with 8 clk
      "nop \n\t"                          // 9
      "nop \n\t"                          // 10
      "nop \n\t"                          // 11
      "st %a[port], %[lo] \n\t"           // 13 -> set to lo
      "nop \n\t"                          // 1
      "nop \n\t"                          // 2
      "nop \n\t"                          // 3
      "nop \n\t"                          // 4
      "rjmp end_transmit \n\t"            // 6

      "end_transmit: \n\t"
      "nop \n\t"

      : [port] "+e" (port), [color] "+r" (color), [color_2sc] "+r" (color_2sc), [bit_count] "+r" (bit_count), [count] "+w" (count)
      : [ptr] "e" (ptr), [hi] "r" (hi), [lo] "r" (lo), [bright] "r" (bright), [scale] "r" (scale)
      : "r0", "r1"
    );
    interrupts();
    timer = micros();
  }

  void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright, bool scale)
  {
    _extern_timer_show(reinterpret_cast<const uint8_t*>(leds), (uint16_t)(len * 3), pin, endTime, scale ? bright : 255);
  }
}

#endif // AVR
