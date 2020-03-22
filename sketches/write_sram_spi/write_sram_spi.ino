#include <SPI.h>

#define PORTB_NSS  0x04
#define PORTC_RCLK 0x01
#define PORTC_NOE_0 0x02
#define PORTC_NWE_0 0x04

#define PORTB_DATA 0x03
#define PORTD_DATA 0xfc
#define PORTC_CTRL 0x07

//#define ADDR_8
#define ADDR_16
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
static word startAddress = 0;


static void sramDisableMode()
{
  // Chip select disable everything (not)
  PORTB |= PORTB_NSS;
  DDRB |= PORTB_NSS;
  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE_0;  // Disable memory read (not)
  PORTC |= PORTC_NWE_0;  // Disable memory write (not)
  DDRC &= ~PORTC_CTRL;

  // Data read
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
}


static void sramWriteMode()
{
  // Chip select enable everything (not)
  PORTB &= ~PORTB_NSS;
  DDRB |= PORTB_NSS;
  
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE_0;  // Disable memory read (not)
  PORTC |= PORTC_NWE_0;  // Disable memory write (not)
  DDRC |= PORTC_CTRL;
  
  // Data write
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB |= PORTB_DATA;
  DDRD |= PORTD_DATA;
}


static inline void sramWriteAddress(word _address)
{
#ifdef ADDR_8
  SPI.transfer(_address & 0xff);
#elif defined ADDR_16
  SPI.transfer16(_address);
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
  PORTC &= ~PORTC_NWE_0;
  NOP;
  PORTC |= PORTC_NWE_0;
}


static inline void sramWrite(word _address, byte _value)
{
  sramWriteAddress(_address);
  sramWriteData(_value);
}


#ifdef SRAM_DEBUG
static void sramReadMode()
{
  // Chip select enable everything (not)
  PORTB &= ~PORTB_NSS;
  DDRB |= PORTB_NSS;
  
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE_0;  // Disable memory read (not)
  PORTC |= PORTC_NWE_0;  // Disable memory write (not)
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
  PORTC &= ~PORTC_NOE_0;
  NOP;
  value |= PINB & PORTB_DATA;
  value |= PIND & PORTD_DATA;
  PORTC |= PORTC_NOE_0;
  return value;
}


static inline byte sramRead(word _address)
{
  sramWriteAddress(_address);
  return sramReadData();
}
#endif


void setup()
{
  sramDisableMode();
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
