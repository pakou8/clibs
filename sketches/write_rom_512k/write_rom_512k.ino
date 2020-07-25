#include <SPI.h>

#define PORTC_RCLK  0x01
#define PORTC_NOE   0x02
#define PORTC_NWE   0x04
#define PORTC_NCE   0x08

#define PORTB_DATA 0x03
#define PORTD_DATA 0xfc
#define PORTC_CTRL 0x0f

#define SRL_BAUDS 115200
#define SRL_TIMEOUT 100
#define SPI_FREQ 8000000

#define INPUT_SIZE 32
#define NOP __asm__("nop\n\t")


static void romDisableMode()
{ 
  // Various control
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE;   // Disable memory read (not) using pull-up
  PORTC |= PORTC_NWE;   // Disable memory write (not) using pull-up
  PORTC |= PORTC_NCE;   // Disable memory chip (not) using pull-up
  DDRC &= ~PORTC_CTRL;

  // Data read
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
}


static void romWriteMode()
{
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE;  // Disable memory read (not)
  PORTC |= PORTC_NWE;  // Disable memory write (not)
  PORTC &= ~PORTC_NCE;
  DDRC |= PORTC_CTRL;
  
  // Data write
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB |= PORTB_DATA;
  DDRD |= PORTD_DATA;
}


static inline void romWriteAddress(long _address)
{
  byte tmp[3];
  tmp[0] = (byte) (_address & 0xff);
  tmp[1] = (byte) ((_address >> 8) & 0xff);
  tmp[2] = (byte) ((_address >> 16) & 0xff);
  
  SPI.transfer(tmp, 3);

  PORTC |= PORTC_RCLK;
  PORTC &= ~PORTC_RCLK;
}


static inline void romWriteData(byte _value)
{
  PORTB = (PORTB & ~PORTB_DATA) | (_value & PORTB_DATA);
  PORTD = (PORTD & ~PORTD_DATA) | (_value & PORTD_DATA);  
  PORTC &= ~PORTC_NWE;
  NOP;
  NOP;
  PORTC |= PORTC_NWE;
}


static inline void romWrite(long _address, byte _value)
{
  romWriteAddress(_address);
  romWriteData(_value);
}


static void romReadMode()
{
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE;  // Disable memory read (not)
  PORTC |= PORTC_NWE;  // Disable memory write (not)
  PORTC &= ~PORTC_NCE;
  DDRC |= PORTC_CTRL;
  
  // Data read
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
}


static inline byte romReadData()
{
  byte value = 0;
  PORTC &= ~PORTC_NOE;
  NOP;
  NOP;
  value |= PINB & PORTB_DATA;
  value |= PIND & PORTD_DATA;
  PORTC |= PORTC_NOE;
  return value;
}


static inline byte romRead(long _address)
{
  romWriteAddress(_address);
  return romReadData();
}


static byte split(char *_string, const char *_delimiter, const char *_tokens[8])
{
  byte count = 0;
  char *token = strtok(_string, _delimiter);
  while(token != NULL && count < 8)
  {
    _tokens[count++] = token;
    token = strtok(NULL, _delimiter);    
  }
  return count;
}


void setup()
{
  // Read TCNT1 to do cycle timing
  TCCR1A = 0;
  TCCR1B = 1;

  romDisableMode();
  SPI.begin();
  Serial.begin(SRL_BAUDS);
  Serial.setTimeout(SRL_TIMEOUT);
  SPI.beginTransaction(SPISettings(SPI_FREQ, LSBFIRST, SPI_MODE0));
}


void loop()
{
  char command[INPUT_SIZE];
  size_t count = Serial.readBytesUntil('\n', command, INPUT_SIZE);
  if(count > 0)
  {
    // Parse the command
    command[count] = 0;
    const char *tokens[8];
    byte argc = split(command, " ", tokens);
    if(argc > 0)
    {
      if(strcmp(tokens[0], "write") == 0 && argc == 2)
      {
        size_t count = 0;
        byte data[INPUT_SIZE];
        long ptr = atol(tokens[1]);
        
        while((count = Serial.readBytes(data, INPUT_SIZE)) > 0)
        {
          for(size_t i = 0; i < count; ++i)
          {
            romWriteMode();
            byte last = data[i];
            romWrite(0x0a955L, 0xaa); //romWrite(0x5555L, 0xaa);
            romWrite(0x016aaL, 0x55); //romWrite(0x2aaaL, 0x55);
            romWrite(0x0a955L, 0xa0); //romWrite(0x5555L, 0xa0);
            romWrite(ptr++, last);

            romReadMode();
            while((romReadData() & 0x80) != (last & 0x80));
          }
        }
        romDisableMode();
      }
      else if(strcmp(tokens[0], "read") == 0 && argc == 3)
      {
        byte data[INPUT_SIZE];
        long ptr = atol(tokens[1]);
        long count = atol(tokens[2]);
        
        romReadMode();
        while(count > 0)
        {
          long readsize = (count > INPUT_SIZE) ? INPUT_SIZE : count;
          
          for(long i = 0; i < readsize; ++i)
            data[i] = romRead(ptr++);

          count -= readsize;
          Serial.write(data, readsize);
        }
        romDisableMode();
      }
      else if(strcmp(tokens[0], "erase") == 0 && argc == 1)
      {
        romWriteMode();        
        romWrite(0x0a955L, 0xaa); //romWrite(0x5555L, 0xaa);
        romWrite(0x016aaL, 0x55); //romWrite(0x2aaaL, 0x55);
        romWrite(0x0a955L, 0x80);
        romWrite(0x0a955L, 0xaa);
        romWrite(0x016aaL, 0x55);
        romWrite(0x0a955L, 0x10);

        romReadMode();
        while((romReadData() & 0x80) != (0x10 & 0x80));
        Serial.println("erased!");
          
        romDisableMode();
      }
    }
  }
}
