#include <SPI.h>

#define PORTB_NSS  0x04
#define PORTC_RCLK 0x01
#define PORTC_NOE 0x02
#define PORTC_NWE 0x04
#define PORTC_NCE_0 0x08
#define PORTC_NCE_1 0x10

#define PORTB_DATA 0x03
#define PORTD_DATA 0xfc
#define PORTC_CTRL 0x1f

//#define ADDR_8
#define ADDR_16
//#define SRL_BAUDS 115200
#define SRL_BAUDS 500000
#define SPI_FREQ 8000000
#define NOP __asm__("nop\n\t")

#define INPUT_SIZE 64
#define STATE_IDLE 0
#define STATE_READ 1
#define STATE_WRITE 2

// Initialized in the init function
byte state;
byte chip;
word start;
word current;
word readsize;


static void sramDisableMode()
{
  // Chip select disable everything (not)
  PORTB |= PORTB_NSS;
  DDRB |= PORTB_NSS;
  
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
  // Chip select enable everything (not)
  PORTB &= ~PORTB_NSS;
  DDRB |= PORTB_NSS;
  
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
  // Chip select enable everything (not)
  PORTB &= ~PORTB_NSS;
  DDRB |= PORTB_NSS;
  
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


static byte tokenize(char *_string, const char *_delimiter, const char *_tokens[8])
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
  SPI.begin();
  Serial.begin(SRL_BAUDS);
  SPI.beginTransaction(SPISettings(SPI_FREQ, LSBFIRST, SPI_MODE0));

  // Read TCNT1 to do cycle timing
  TCCR1A = 0;
  TCCR1B = 1;
  
  sramDisableMode();
  state = STATE_IDLE;
}


void loop()
{
  switch(state)
  {
    case STATE_IDLE:
    {
      char command[INPUT_SIZE];
      size_t count = Serial.readBytesUntil(';', command, INPUT_SIZE);
      if(count > 0)
      {
        // Parse the command
        command[count] = 0;
        const char *tokens[8];
        byte argc = tokenize(command, " ", tokens);
        if(argc > 0)
        {
          if(strcmp(tokens[0], "write") == 0 && argc == 3)
          {
            chip = atoi(tokens[1]);
            start = atoi(tokens[2]);
            current = start;
            sramWriteMode(chip);
            state = STATE_WRITE;
          }
          else if(strcmp(tokens[0], "read") == 0 && argc == 4)
          {
            chip = atoi(tokens[1]);
            start = atoi(tokens[2]);
            readsize = atoi(tokens[3]);
            current = start;
            sramReadMode(chip);
            state = STATE_READ;
          }
        }
      }
      break;
    }
    case STATE_READ:
    {
      byte data[INPUT_SIZE];
      
      size_t diff = start + readsize - current;
      size_t count = (diff > INPUT_SIZE) ? INPUT_SIZE : diff;      
      for(size_t i = 0; i < count; ++i)
      {
        data[i] = sramRead(current);
        ++current;
      }
      
      Serial.write(data, count);
      if(current - start == readsize)
      {
        sramDisableMode();
        state = STATE_IDLE;
      }
      break;
    }
    case STATE_WRITE:
    {
      byte data[INPUT_SIZE];
      size_t count = Serial.readBytes(data, INPUT_SIZE);
      if(count > 0)
      {
        for(size_t i = 0; i < count; ++i)
        {
          sramWrite(current, data[i]);
          ++current;
        }
      }
      else
      {
        sramDisableMode();
        state = STATE_IDLE;
      }
      break;
    }
  }
}
