
# PixelPerfect

Library to control WS2812B led strips without dynamic memory allocations, but in simplest way than popular FastLED. Just check out.


## Authors

- [@syzyf97](https://www.github.com/syzyf97)


## Usage/Examples

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2} // 39 leds and pin 2

void setup() {
    strip.begin();
    strip.brighntess = 120; // brightness is public variable
    strip.fill(0, 255, 255); // this fill all 39 leds in magenta
    strip.show(); // update strip, with dynamic scaled with brightness;

    delay(2000);
    strip.clear();
    strip[10] = {255, 255, 0}; // u can get acces to any led by indeks
    strip[10].b = 255; // u can olo can get acces to part of color
    strip[10].g -= 255; // that let u relate with before value of colors
    // you can set brightness for only this update, with not need change original
    // brightness variable.
    strip.show(255); // <- 255 mean brightness 255 to this update
}

void loop() {}

```


# API Reference

## LED struct

### Contructors

```cpp
pix::LED(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `r` | Public red part of color |
| `g` | Public green part of color |
| `b` | Public blue part of color |

```cpp
pix::LED(uint32_t color);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `color` | Color in one 32-bit type like 0xRRGGBB |

### Operators
```cpp
pix::LED::operator uint32_t();
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `return` | Color in one 32-bit type like 0xRRGGBB |

##

```cpp
uint8_t& pix::LED::operator[](uint8_t index); 
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `index` | Color part index in order RGB (R0 G1 B2) |
| `return` | Reference to color part |

**Warning** if *index* > 2 then function return safety static void colorPart, but this variable is not connected to any LED, and change him have no effect.

##

```cpp
pix::LED& pix::LED::operator=(uint32_t color);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `color` | Color in one 32-bit type like 0xRRGGBB |
| `return` | Self-reference |

##

```cpp
pix::LED& pix::LED::operator=(const pix::LED& other);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `other` | Other led reference to copy |
| `return` | Self-reference |

##

```cpp
bool pix::LED::operator==(const pix::LED& other);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `other` | Other led reference to comparison |
| `return` | The LED has the same color as specified |

##

```cpp
bool pix::LED::operator==(uint32_t color);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `color` | Color in one 32-bit type like 0xRRGGBB to comparison |
| `return` | The LED has the same color as specified |

##

```cpp
bool pix::LED::operator!=(const pix::LED& other);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `other` | Other led reference to comparison |
| `return` | The LED has different color as specified |

##
```cpp
bool pix::LED::operator!=(uint32_t color);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `color` | Color in one 32-bit type like 0xRRGGBB to comparison |
| `return` | The LED has different color as specified |


## Strips interface

### Contructors

```cpp
pix::StripTemplate<uint16_t PIX_LEN>(uint8_t pin = 255, bool reverse = false);
// you can be use instead of the above
pix::Strip<uint16_t PIX_LEN>(uint8_t pin = 255, bool reverse = false);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `PIX_LEN` | Pixel count in strip |
| `pin` | Pin to control strip |
| `reverse` | Reverse direction of strip |

##

```cpp
pix::StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `pixelCount` | Pixel count in strip |
| `pin` | Pin to control strip |
| `reverse` | Reverse direction of strip |

### Begin

```cpp
void pix::Strip::begin(void);
```
| Description |
| :--- | 
|  Initialize pin as output |

### Get reference to led

```cpp
pix::LED& pix::Strip::led(uint16_t i);
const pix::LED& pix::Strip::led(uint16_t i) const;
```
```cpp
pix::LED& pix::Strip::operator[](uint16_t i);
const pix::LED& pix::Strip::operator[](uint16_t i) const;
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `i` | Index of led |
| `return` | Reference to *i* index led |

**Warning** if *i* > PIX_LEN then function return safety static void led, but this led is not connected to any strip, and change him have no effect.

### Change pixel color (in range)
```cpp
void pix::Strip::fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `r` | Red part of color |
| `g` | Green part of color |
| `b` | Blue part of color |
| `from` | First led index to fill |
| `to` | Last led index to fill |

**Warning** if *i* > PIX_LEN then function return safety static void led, but this led is not connected to any strip, and change him have no effect.

##

```cpp
void pix::Strip::fillFromTo(uint32_t color, uint16_t from, uint16_t to);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `color` | Color in one 32-bit type like 0xRRGGBB |
| `from` | First led index to fill |
| `to` | Last led index to fill |

**Warning** if *i* > PIX_LEN then function return safety static void led, but this led is not connected to any strip, and change him have no effect.

### Change pixel color (all)

```cpp
void pix::Strip::fill(uint8_t r, uint8_t g, uint8_t b);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `r` | Red part of color |
| `g` | Green part of color |
| `b` | Blue part of color |

##

```cpp
void pix::Strip::fill(uint32_t color);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `color` | Color in one 32-bit type like 0xRRGGBB |

### Update strip


Update strip according to leds and public brightness variable.

```cpp
void pix::Strip::show(void);
```
##

Update strip according to leds and specified brightness.

```cpp
void pix::Strip::show(uint8_t bright);
```
| Parameter | Description                |
| :-------- | :------------------------- |
| `bright` |  Brightness for scale to this update |






