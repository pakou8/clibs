#include <SPI.h>

#define PORTB_RCLK 0x02
#define PORTB_DRW 0x01
#define PORTD_INT 0x01

#define SRL_BAUDS 500000
#define SPI_FREQ 8000000
#define NOP __asm__("nop\n\t")


void onClock()
{
  word dummy16 = 0;
  byte dummy8 = 0;

  // Latch address and data
  PORTB |= PORTB_RCLK;
  
  word address = SPI.transfer16(dummy16);
  byte data = SPI.transfer(dummy8);
  byte rW = PINB & PORTB_DRW;  

  // Release latch
  PORTB &= ~PORTB_RCLK;

  char tmp[64];
  char mode = (rW > 0) ? 'r' : 'W';
  byte ascii = (data > 0x21 && data < 0x7e) ? data : '.';
  sprintf(tmp, "%04x\t%c\t%02x %c", address, mode, data, ascii);
  Serial.println(tmp);
}


void setup()
{
  // Read TCNT1 to do cycle timing
  TCCR1A = 0;
  TCCR1B = 1;

  SPI.begin();
  Serial.begin(SRL_BAUDS);
  SPI.beginTransaction(SPISettings(SPI_FREQ, LSBFIRST, SPI_MODE0));

  DDRB |= PORTB_RCLK;
  DDRB &= ~PORTB_DRW;
  PORTB &= ~PORTB_RCLK; // Set shift register clock low
  DDRD &= ~PORTD_INT; // Set interrupt pin as input

  attachInterrupt(0, onClock, RISING);
}


void loop()
{
 
}
