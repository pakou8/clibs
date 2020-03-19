#include <SPI.h>

#define PORTC_RCLK 0x01
#define PORTC_MNOE 0x02
#define PORTC_MNWE 0x04
#define PORTC_NCE0 0x08
#define PORTC_NCE1 0x10

#define PORTB_DATA 0x03
#define PORTD_DATA 0xfc
#define PORTC_CTRL 0x07
#define PORTC_NCES 0x18

//#define ADDR_8
#define ADDR_16
//#define ADDR_24
//#define SRAM_DEBUG

#ifdef SRAM_DEBUG
#define SRL_BAUDS 115200
#else
#define SRL_BAUDS 500000
#endif

#define SPI_FREQ 8000000
#define NOP __asm__("nop\n\t")

#define INPUT_SIZE 64
static byte inputBuffer[INPUT_SIZE];
static unsigned long startAddress = 0;


static inline void sramDisableMode()
{  
  PORTC |= PORTC_NCES;
  DDRC |= PORTC_NCES;
  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_MNOE;  // Disable memory read (not)
  PORTC |= PORTC_MNWE;  // Disable memory write (not)
  DDRC &= ~PORTC_CTRL;

  // Data read
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;  
}


static inline void sramWriteMode(byte _chip = 0)
{
  if(_chip == 0) { PORTC &= ~PORTC_NCE0; PORTC |= PORTC_NCE1; }
  else { PORTC |= PORTC_NCE0; PORTC &= ~PORTC_NCE1; }
  DDRC |= PORTC_NCES;
  
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_MNOE;  // Disable memory read (not)
  PORTC |= PORTC_MNWE;  // Disable memory write (not)
  DDRC |= PORTC_CTRL;
  
  // Data write
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB |= PORTB_DATA;
  DDRD |= PORTD_DATA;
}


static inline void sramWriteAddress(unsigned long _address)
{
#ifdef ADDR_8
  SPI.transfer(_address & 0xff);
#elif defined ADDR_16
  unsigned int address = _address & 0xffff;
  SPI.transfer16(address);
#elif defined ADDR_24
  byte address[3];
  address[0] = _address & 0xff;
  address[1] = (_address >> 8) & 0xff;
  address[2] = (_address >> 16) & 0xff;
  SPI.transfer(address, 3);
#else
  #error "ADDR_X not defined"
#endif

  PORTC |= PORTC_RCLK;
  PORTC &= ~PORTC_RCLK;  
}


static inline void sramWriteData(byte _value)
{
  PORTB = (PORTB & ~PORTB_DATA) | (_value & PORTB_DATA);
  PORTD = (PORTD & ~PORTD_DATA) | (_value & PORTD_DATA);  
  PORTC &= ~PORTC_MNWE;
  NOP;
  PORTC |= PORTC_MNWE;
}


static inline void sramWrite(unsigned long _address, byte _value)
{
  sramWriteAddress(_address);
  sramWriteData(_value);
}


#ifdef SRAM_DEBUG
static inline void sramReadMode(byte _chip = 0)
{
  // Chip selection
  if(_chip == 0) { PORTC &= ~PORTC_NCE0; PORTC |= PORTC_NCE1; }
  else { PORTC |= PORTC_NCE0; PORTC &= ~PORTC_NCE1; }
  DDRC |= PORTC_NCES;
  
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_MNOE;  // Disable memory read (not)
  PORTC |= PORTC_MNWE;  // Disable memory write (not)
  DDRC |= PORTC_CTRL;
  
  // Data read
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
}


static inline byte sramReadData()
{
  byte value = 0;
  PORTC &= ~PORTC_MNOE;
  NOP;
  value |= PINB & PORTB_DATA;
  value |= PIND & PORTD_DATA;
  PORTC |= PORTC_MNOE;
  return value;
}


static inline byte sramRead(unsigned long _address)
{
  sramWriteAddress(_address);
  return sramReadData();
}
#endif


void setup()
{
  sramWriteMode();
  SPI.begin();
  Serial.begin(SRL_BAUDS);
  SPI.beginTransaction(SPISettings(SPI_FREQ, LSBFIRST, SPI_MODE0));

  // Read TCNT1 to do cycle timing
  TCCR1A = 0;
  TCCR1B = 1;
}


void loop()
{
  size_t count = Serial.readBytes(inputBuffer, INPUT_SIZE);
  if(count > 0)
  {
    for(size_t i = 0; i < count; ++i)
    {
      sramWrite(startAddress + i, inputBuffer[i]);
    }

#ifdef SRAM_DEBUG
    sramReadMode();
    for(size_t i = 0; i < count; ++i)
    {
      byte value = sramRead(startAddress + i);
      if(inputBuffer[i] != value)
        Serial.println(startAddress + i, HEX);
    }
    sramWriteMode();
#endif
    
    startAddress += count;
  }
  else
  {
    if(startAddress > 0)
    {
      Serial.println("Size written (Bytes):");
      Serial.println(startAddress);
      startAddress = 0;
    }
  }
}
