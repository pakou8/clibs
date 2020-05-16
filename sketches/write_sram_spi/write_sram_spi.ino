#include <SPI.h>

#define PORTC_RCLK 0x01
#define PORTC_NOE 0x02
#define PORTC_NWE 0x04
#define PORTC_NCE_0 0x08
#define PORTC_NCE_1 0x10

#define PORTB_DATA 0x03
#define PORTD_DATA 0xfc
#define PORTC_CTRL 0x1f

//#define SRL_BAUDS 115200
#define SRL_BAUDS 500000
#define SRL_TIMEOUT 100
#define SPI_FREQ 8000000

#define INPUT_SIZE 64
#define NOP __asm__("nop\n\t")


static void sramDisableMode()
{
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE;   // Disable memory read (not) using pull-up
  PORTC |= PORTC_NWE;   // Disable memory write (not) using pull-up
  PORTC |= PORTC_NCE_0; // Disable memory chip 0 (not) using pull-up
  PORTC |= PORTC_NCE_1; // Disable memory chip 1 (not) using pull-up
  DDRC &= ~PORTC_CTRL;

  // Data read
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
}


static void sramWriteMode(byte _chip = 0)
{
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE;  // Disable memory read (not)
  PORTC |= PORTC_NWE;  // Disable memory write (not)
  // Select one of the memory chip
  if(_chip == 0) { PORTC &= ~PORTC_NCE_0; PORTC |= PORTC_NCE_1; }
  else { PORTC |= PORTC_NCE_0; PORTC &= ~PORTC_NCE_1; }
  DDRC |= PORTC_CTRL;
  
  // Data write
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB |= PORTB_DATA;
  DDRD |= PORTD_DATA;
}


static inline void sramWriteAddress(word _address)
{
  SPI.transfer16(_address);

  PORTC |= PORTC_RCLK;
  PORTC &= ~PORTC_RCLK;
}


static inline void sramWriteData(byte _value)
{
  PORTB = (PORTB & ~PORTB_DATA) | (_value & PORTB_DATA);
  PORTD = (PORTD & ~PORTD_DATA) | (_value & PORTD_DATA);  
  PORTC &= ~PORTC_NWE;
  NOP;
  NOP;
  PORTC |= PORTC_NWE;
}


static inline void sramWrite(word _address, byte _value)
{
  sramWriteAddress(_address);
  sramWriteData(_value);
}


static void sramReadMode(byte _chip = 0)
{
  // Various controls  
  PORTC &= ~PORTC_RCLK; // Set Shift register clock low
  PORTC |= PORTC_NOE;  // Disable memory read (not)
  PORTC |= PORTC_NWE;  // Disable memory write (not)
  // Select one of the memory chip
  if(_chip == 0) { PORTC &= ~PORTC_NCE_0; PORTC |= PORTC_NCE_1; }
  else { PORTC |= PORTC_NCE_0; PORTC &= ~PORTC_NCE_1; }
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
  PORTC &= ~PORTC_NOE;
  NOP;
  NOP;
  value |= PINB & PORTB_DATA;
  value |= PIND & PORTD_DATA;
  PORTC |= PORTC_NOE;
  return value;
}


static inline byte sramRead(word _address)
{
  sramWriteAddress(_address);
  return sramReadData();
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

  sramDisableMode();
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
      if(strcmp(tokens[0], "write") == 0 && argc == 3)
      {
        size_t count = 0;
        byte data[INPUT_SIZE];
        byte chip = atoi(tokens[1]);
        word ptr = atoi(tokens[2]);
        
        sramWriteMode(chip);
        while((count = Serial.readBytes(data, INPUT_SIZE)) > 0)
        {
          for(size_t i = 0; i < count; ++i)
            sramWrite(ptr++, data[i]);
        }
        sramDisableMode();
      }
      else if(strcmp(tokens[0], "read") == 0 && argc == 4)
      {
        byte data[INPUT_SIZE];
        byte chip = atoi(tokens[1]);
        word ptr = atoi(tokens[2]);
        size_t count = atoi(tokens[3]);
        
        sramReadMode(chip);
        while(count > 0)
        {
          size_t readsize = (count > INPUT_SIZE) ? INPUT_SIZE : count;
          
          for(size_t i = 0; i < readsize; ++i)
            data[i] = sramRead(ptr++);

          count -= readsize;
          Serial.write(data, readsize);
        }
        sramDisableMode();
      }
      else if(strcmp(tokens[0], "memset") == 0 && argc == 5)
      {
        byte chip = atoi(tokens[1]);
        word ptr = atoi(tokens[2]);
        size_t count = atoi(tokens[3]);
        byte value = atoi(tokens[4]);
        
        sramWriteMode(chip);
        
        for(size_t i = 0; i < count; ++i)
          sramWrite(ptr++, value);
          
        sramDisableMode();
      }
    }
  }
}
