#define PORTB_CCLR  0x04
#define PORTB_CCLK  0x20

#define PORTC_NOE   0x01
#define PORTC_NWE   0x02
#define PORTC_NCE   0x04

#define PORTB_DATA 0x03
#define PORTD_DATA 0xfc
#define PORTC_CTRL 0x07
#define PORTB_CTRL 0x24

#define SRL_BAUDS 57600
#define SRL_TIMEOUT 100

#define INPUT_SIZE 64
#define NOP __asm__("nop\n\t")


static void romDisableMode()
{ 
  PORTB &= ~PORTB_CCLR; // Reset counter
  PORTB &= ~PORTB_CCLK; // Set count clock low
  PORTC |= PORTC_NOE;   // Disable memory read (not)
  PORTC |= PORTC_NWE;   // Disable memory write (not)
  PORTC |= PORTC_NCE;   // Disable memory chip (not)

  DDRB |= PORTB_CTRL;
  DDRC |= PORTC_CTRL;

  // Data read
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB &= ~PORTB_DATA;
  DDRD &= ~PORTD_DATA;
}

static void romWriteMode()
{
  PORTB |= PORTB_CCLR; // Clear reset
  PORTB &= ~PORTB_CCLK; // Set count clock low
  PORTC |= PORTC_NOE;   // Disable memory read (not)
  PORTC |= PORTC_NWE;   // Disable memory write (not)
  PORTC &= ~PORTC_NCE;   // Enable memory chip (not)

  DDRB |= PORTB_CTRL;
  DDRC |= PORTC_CTRL;
  
  // Data write
  PORTB &= ~PORTB_DATA;
  PORTD &= ~PORTD_DATA;
  DDRB |= PORTB_DATA;
  DDRD |= PORTD_DATA;
}


static inline void romClockAddress()
{
  // Increment address counter
  PORTB |= PORTB_CCLK;
  PORTB &= ~PORTB_CCLK;
}

static inline void romWriteData(byte _value)
{
  // Setup data
  PORTB = (PORTB & ~PORTB_DATA) | (_value & PORTB_DATA);
  PORTD = (PORTD & ~PORTD_DATA) | (_value & PORTD_DATA);  
  PORTC &= ~PORTC_NWE;

  // Wait ~150ns to submit data
  NOP;
  NOP;
  NOP;
  PORTC |= PORTC_NWE;  
}


static inline void romWrite(byte _value)
{
  romClockAddress();
  romWriteData(_value);
}


static void romReadMode()
{
  PORTB |= PORTB_CCLR;  // Clear reset
  PORTB &= ~PORTB_CCLK; // Set count clock low
  PORTC |= PORTC_NOE;   // Disable memory read (not)
  PORTC |= PORTC_NWE;   // Disable memory write (not)
  PORTC &= ~PORTC_NCE;  // Enable memory chip (not)

  DDRB |= PORTB_CTRL;
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
  
  // Wait ~150ns to read data
  PORTC &= ~PORTC_NOE;
  NOP;
  NOP;
  NOP;
  value |= PINB & PORTB_DATA;
  value |= PIND & PORTD_DATA;
  PORTC |= PORTC_NOE;
  
  return value;
}

static inline byte romRead()
{
  romClockAddress();
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
  Serial.begin(SRL_BAUDS);
  Serial.setTimeout(SRL_TIMEOUT);
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
      if(strcmp(tokens[0], "write") == 0 && argc == 1)
      {        
        size_t count = 0;        
        byte data[INPUT_SIZE];
        byte last = 0;
        bool poll = false;        
        
        while((count = Serial.readBytes(data, INPUT_SIZE)) > 0)
        {
          if(poll)
          {
            romReadMode();
            while(romReadData() != last);
          }
          
          romWriteMode();
          for(size_t i = 0; i < count; ++i)
            romWrite(data[i]);
            
          last = data[count-1];
          poll = true;
        }
        romDisableMode();
      }
      else if(strcmp(tokens[0], "read") == 0 && argc == 2)
      {
        byte data[INPUT_SIZE];
        size_t count = atoi(tokens[1]);
        
        romReadMode();
        while(count > 0)
        {
          size_t readsize = (count > INPUT_SIZE) ? INPUT_SIZE : count;
          
          for(size_t i = 0; i < readsize; ++i)
            data[i] = romRead();

          count -= readsize;
          Serial.write(data, readsize);
        }
        romDisableMode();
      }
      else if(strcmp(tokens[0], "memset") == 0 && argc == 3)
      {
        size_t count = atoi(tokens[1]);
        byte value = atoi(tokens[2]);
        bool poll = false; 
        
        while(count > 0)
        {
          if(poll)
          {
            romReadMode();
            while(romReadData() != value);
          }
          
          size_t writesize = (count > INPUT_SIZE) ? INPUT_SIZE : count;

          romWriteMode();
          for(size_t i = 0; i < writesize; ++i)
            romWrite(value);

          count -= writesize;
          poll = true;
        }
        romDisableMode();
      }
    }
  }
}
