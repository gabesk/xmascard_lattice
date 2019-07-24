
//#define F_CPU 8000000
//#define BAUD 38400
#define F_CPU 7370000
#define BAUD 115200
#define NULL 0

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/setbaud.h>

#define DDR_SPI DDRB
#define PORT_SPI PORTB
#define PIN_SS PORTB2
#define PIN_MISO PORTB4
#define DD_MISO DDB4
#define DD_MOSI DDB3
#define DD_SCK DDB5
#define DD_SS DDB2

// See SetupFpgaResetAndDone for more notes
#define DDR_CRESET DDRC
#define DDR_CDONE DDRC
#define DDR_VBATT DDRC
#define DDR_PWR_CTRL DDRC
#define DDR_RXD_CTRL DDRD

 
#define PORT_CRESET PORTC
#define PORT_CDONE PORTC
#define PORT_VBATT PORTC
#define PORT_PWR_CTRL PORTC
#define PORT_RXD_CTRL PORTD


#define PIN_CRESET PINC
#define PIN_CDONE PINC
#define PIN_VBATT PINC
#define PIN_PWR_CTRL PINC
#define PIN_RXD_CTRL PIND


#define BIT_CRESET PINC0
#define BIT_CDONE PINC1
#define BIT_VBATT PINC3
#define BIT_PWR_CTRL PORTC4
#define BIT_RXD_CTRL PORTD4


void USART_Init( void );
void USART_Transmit( unsigned char data );
unsigned char USART_Receive_With_Error( unsigned char* frame_error );
unsigned char USART_Receive( void );
void USART_Flush( void );

void SPI_MasterInit(void);
void SPI_MasterTransmit(char cData);
void SPI_MasterTransmit_WaitBefore(char cData);
char SPI_SlaveReceive(void);

void ReleaseSpiFlashFromPowerDown(void);

void SetupFpgaResetAndDone();
void PutFpgaIntoReset();
void PutFpgaOutOfReset();
unsigned char ReadFpgaDoneBit();

void SetupAutoTurnOffTimer();

int ReadBatteryVoltage();
int ReadUsbVoltage();

char pageodata[260];
int pageaddress;

void setup() {
  USART_Init();
  SPI_MasterInit();
  SetupFpgaResetAndDone();
  SetupAutoTurnOffTimer();
  
  PRR0 &= ~(1<<PRADC);
}

void loop() {

  unsigned char err;
  unsigned char cmd;
  unsigned char initial_cmd;
  
  // See if we are running on battery power. If we are, power off the AVR to save battery power.
  // (this isn't actually quite what we want --- the battery can always sit there plugged in without wanting the thing to power
  //  off ... what we'd like is to figure out if we're running on USB and only exit power save mode in that case)
  //  but I don't know if there's that option.)
  // Actually ... probably what we want is to know if is the USB is plugged in. I wonder what the best way to do that is ...
  // maybe we can sense the TX line and see if it's negative when powered on battery?
  // I suppose one way would be to look at if the battery is draining, but that kind of sucks.
  // Maybe we just always put it into low power until an interrupt come sin.
  // Even that's kind of weird tho. So let's just deal with the power saving mode, by turning it off after 60 seconds if on battery.
  // We can do that by just having a timer interrupt fire.

  // On power up, put the serial line into passthrough mode to the FPGA.
  //cmd = 'z';
  //initial_cmd = 1;
  cmd = '?';
  initial_cmd = 0;

  do {
    //USART_Transmit(b);
    /* Configure SS as low (enabled) */
    if (cmd == 'l') {
      PORT_SPI &= ~(1<<PIN_SS);
        
      //USART_Transmit('l');
        
    }
    
    /* Configure SS as high (disabled) */
    else if (cmd == 'r') {
      PORT_SPI |= (1<<PIN_SS);
        
      //USART_Transmit('r');
        
    }
    
    /* Send/receive a byte (transaction) over the SPI connection */
    else if (cmd == 'b') {
      //USART_Transmit('b');
        
      unsigned char to_slave = USART_Receive();
      SPI_MasterTransmit(to_slave);
        
      unsigned char from_slave = SPI_SlaveReceive();
      USART_Transmit(from_slave);
        
    }
    
    /* Program a page of data */
    else if (cmd == 'p') {
      // Receive 256 + 3 address + 1 command byte
      for (int i=0;i<260;i++) {
        pageodata[i] = USART_Receive();
      }
        
      USART_Transmit('a');
        
      PORT_SPI &= ~(1<<PIN_SS);
      for (int i=0;i<260;i++) {
        SPI_MasterTransmit(pageodata[i]);
      }
      PORT_SPI |= (1<<PIN_SS);
        
      USART_Transmit('d');
        
    } 
    
    /* Verify a page of data */
    else if (cmd == 'v') {
      // Read 1 command byte and 3 address bytes
      // (just store it in the page array for now since it won't be needed once transmitted to the spi flash)
      for (int i=0;i<4;i++) {
        pageodata[i] = USART_Receive();
      }
        
      PORT_SPI &= ~(1<<PIN_SS);
        
      // Transmit cmd + addr
      for (int i=0;i<4;i++) {
        SPI_MasterTransmit(pageodata[i]);
      }

      // Read back 256 bytes of page memory
      for (int i=0;i<256;i++) {
        SPI_MasterTransmit(0);
        pageodata[i] = SPI_SlaveReceive();
      }
        
      PORT_SPI |= (1<<PIN_SS);
        
      // Transmit it back over serial
      for (int i=0;i<256;i++) {
        USART_Transmit(pageodata[i]);
      }
        
      /* Unknown command */
    }

    else if (cmd == 'f') {
      // Fast program. Overlap sending the spi bytes with the receive bytes.
      // Receive a byte, send it to spi, the immediately receive another byte from serial knowing that the SPI will
      // finish by the time the next one is done.
      PORT_SPI &= ~(1<<PIN_SS);
      SPDR0 = USART_Receive();
      for (int i=0;i<259;i++) {
        SPI_MasterTransmit_WaitBefore(USART_Receive());
      }
      while(!(SPSR0 & (1<<SPIF)));
      PORT_SPI |= (1<<PIN_SS);
      USART_Transmit('a');
      USART_Transmit('d');
    }
      
    else if (cmd == 'g') {
      // Fast verify.
      PORT_SPI &= ~(1<<PIN_SS);
      SPDR0 = USART_Receive();  // receive a byte, start is sending, immediately go back to receiving a new byte
      for (int i=0;i<3;i++) {
        SPI_MasterTransmit_WaitBefore(USART_Receive());
      }

      // since using WaitBefore, old byte still sending, so wait for it to finish
      while(!(SPSR0 & (1<<SPIF)));
        
      SPI_MasterTransmit(0);    // get byte 0 of page data into SPDR0
      for (int i=0;i<255;i++) {
        SPDR0 = 0;        // unless it's the last byte, start a new transmission
        USART_Transmit(SPDR0);  // transmit old received byte (starting with byte 0 and ending with byte 254)
      }
      USART_Transmit(SPDR0);    // transmit byte 255 but don't start a new transmission
      while(!(SPSR0 & (1<<SPIF)));
      PORT_SPI |= (1<<PIN_SS);
    }
      
    else if (cmd == 'q') {
      PutFpgaIntoReset();
      USART_Transmit('q');
    }
      
    else if (cmd == 'w') {
      PutFpgaOutOfReset();
      USART_Transmit('w');
    }
      
    else if (cmd == 'e') {
      USART_Transmit(ReadFpgaDoneBit());
    }
      
    else if (cmd == 't') {
      ReleaseSpiFlashFromPowerDown();
      USART_Transmit('t');
    }
      
    else if (cmd == 'z') {
      if (!initial_cmd) {
        USART_Transmit('z');
        USART_Flush();
        // Delay a bit to give this character a chance to end properly before switching over to whatever junk the FPGA is sending.
        _delay_ms(100);
      }
      PORT_RXD_CTRL &= ~(1 << BIT_RXD_CTRL);
      while (1) {
          
        // Provide a way to regain control from FPGA passthrough.
        // If a break symbol is transmitted (which is just the serial
        // line going low for much longer than a single byte,
        // introducing a framing error), break out of the passthrough
        // loop and regain control.
        unsigned char dummy = USART_Receive_With_Error(&err);
        if (err) {
          PORT_RXD_CTRL |= (1 << BIT_RXD_CTRL);
          break;
        }
      }
    }
    
    else if (cmd == 'c') {
      USART_Transmit('c');
      int value = ReadBatteryVoltage();
      for (int idx=0;idx<4;idx++) {
        int nibble = ((value & 0xF000) >> 12) + 48;
        if (nibble > 57) nibble += 7;
        value = value << 4;
        USART_Transmit(nibble);
      }
    }
    
    else if (cmd == 'u') {
      USART_Transmit('u');
      int value = ReadUsbVoltage();
      for (int idx=0;idx<4;idx++) {
        int nibble = ((value & 0xF000) >> 12) + 48;
        if (nibble > 57) nibble += 7;
        value = value << 4;
        USART_Transmit(nibble);
      }
    }
    
    else if (cmd == 'y') {
      USART_Transmit('y');
      unsigned int value = 0xabcd;
      for (int idx=0;idx<4;idx++) {
        int nibble = ((value & 0xF000) >> 12) + 48;
        if (nibble > 57) nibble += 7;
        value = value << 4;
        USART_Transmit(nibble);
      }     
      USART_Transmit('d');
    }
    
    else {
      USART_Transmit('?');
    }
    
    cmd = USART_Receive();
    initial_cmd = 0;
  } while (1);
}

void USART_Init( void )
{
  /*Set baud rate */
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;

  /* Enable receiver and transmitter */
  UCSR0B = (1<<RXEN0)|(1<<TXEN0);

  /* Set frame format: 8data, 1stop bit */
  UCSR0C = (3<<UCSZ00);
}
void USART_Transmit( unsigned char data )
{
  /* Wait for empty transmit buffer */
  while ( !( UCSR0A & (1<<UDRE0)))
    ;

  /* Put data into buffer, sends the data */
  UDR0 = data;
}

unsigned char USART_Receive()
{
  return USART_Receive_With_Error(NULL);
}

unsigned char USART_Receive_With_Error( unsigned char* frame_error )
{
  unsigned char status, resh, resl;
  
  /* Wait for data to be received */
  while ( !(UCSR0A & (1<<RXC0)) )
    ;
  
  /* Get status, then data from buffer */
  status = UCSR0A;
  resh = UCSR0B;
  resl = UDR0;
  
  /* Indicate frame errors */
  if (frame_error) {
    *frame_error = 0;
    if ( status & (1<<FE0) ) {
      *frame_error = 1; 
    }
  }
  
  return resl;
}

void USART_Flush( void )
{
  unsigned char dummy;
  while ( UCSR0A & (1<<RXC0) ) dummy = UDR0;
}

void SPI_MasterInit(void)
{
  /* Set MOSI, SS, and SCK output, all others input */
  DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_SS);
  
  /* Configure MISO as high-z */
  PORT_SPI &= ~(1 << PIN_MISO);
  
  /* Configure SS as high (disabled) */
  PORT_SPI |= (1 << PIN_SS);
  
  /* Enable SPI, Master, set clock rate fck/16 */
  SPCR0 = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
}

void SPI_MasterTransmit(char cData)
{
  /* Start transmission */
  SPDR0 = cData;
  
  /* Wait for transmission complete */
  while(!(SPSR0 & (1<<SPIF)))
  ;
}
void SPI_MasterTransmit_WaitBefore(char cData)
{
  /* Wait for transmission complete */
  while(!(SPSR0 & (1<<SPIF)))
  ;

  /* Start transmission */
  SPDR0 = cData;
}
char SPI_SlaveReceive(void)
{
  /* Wait for reception complete */
  while(!(SPSR0 & (1<<SPIF)))
  ;
  
  /* Return Data Register */
  return SPDR0;
}

void ReleaseSpiFlashFromPowerDown(void) {
  PORT_SPI &= ~(1<<PIN_SS);
  SPI_MasterTransmit(0xAB);
  PORT_SPI |= (1<<PIN_SS);
  _delay_us(10);
}




void SetupFpgaResetAndDone() {
  // CRESET#  -> pin 23 -> PC0 (ADC0)
  // CDONE_B  -> pin 24 -> PC1 -> input (ADC1)
  // VBATT    -> pin 26 -> PC3 -> analog input (ADC3)
  // PWR_CTRL -> pin 27 -> PC4 -> output (ADC4)
  // RXD_CTRL -> pin 2  -> PD4 -> output
  //
  // Normally CRESET is open-circuit so it can float high
  // When we want to reset the FPGA, turn it to an input, low, so it pulls the FPGA low
  
  DDR_CRESET   &= ~(1 << BIT_CRESET);
  DDR_CDONE    &= ~(1 << BIT_CDONE);
  DDR_VBATT    &= ~(1 << BIT_VBATT);
  DDR_PWR_CTRL |=  (1 << BIT_PWR_CTRL);
  DDR_RXD_CTRL |=  (1 << BIT_RXD_CTRL);

  PORT_CRESET   &= ~(1 << BIT_CRESET);
  PORT_CDONE    &= ~(1 << BIT_CDONE);
  PORT_VBATT    &= ~(1 << BIT_VBATT);
  PORT_PWR_CTRL |=  (1 << BIT_PWR_CTRL);
  //PORT_PWR_CTRL &= ~(1 << BIT_PWR_CTRL);
  PORT_RXD_CTRL |= (1 << BIT_RXD_CTRL); // i have control
  //PORT_RXD_CTRL &= ~(1 << BIT_RXD_CTRL); // fpga has control
  
  ADCSRA = (1 << ADEN); // Enable ADC, let the rest stay at defaults
}

void PutFpgaIntoReset() {
  // Not needed because already set low for Tri-state in initialization
  // PORT_CRESET   &= ~(1 << BIT_CRESET); // Set output low
  DDR_CRESET |= (1 << BIT_CRESET); // Turn pin into output
}

void PutFpgaOutOfReset() {
  DDR_CRESET &= ~(1 << BIT_CRESET); // Turn pin into input
  // Not needed because already set low for Tri-state in initialization
  // PORT_CRESET |= (1 << BIT_CRESET);  // Set output low
}

unsigned char ReadFpgaDoneBit() {
  if (PIN_CDONE & (1 << BIT_CDONE)) return 1;
  else return 0;
}

int ReadBatteryVoltage() {
  
  // Set up the ADC to read the VBATT signal
  ADMUX = 3; // MUX = ADC3

  ADCSRA |= (1 << ADSC); // start conversion
  
  while ( (ADCSRA & (1 << ADSC) ) );
  //while ( (ADCSRA & (1 << ADIF)) == 0 ); // wait for it to finish (interrupt flag set)
  ADCSRA |= (1 << ADIF); // and clear it
  
  int result = ADCL; // read result - low register must be read first
  result |= (ADCH << 8);
  
  //PRR0 |= 1<<PRADC;
  return result;
}

int ReadUsbVoltage() {
  
  // Set up the ADC to read the VUSB_HALF signal
  ADMUX = 2; // MUX = ADC2
  
  ADCSRA |= (1 << ADSC); // start conversion
  
  //while ( (ADCSRA & (1 << ADSC) ) );
  while ( (ADCSRA & (1 << ADIF)) == 0 ); // wait for it to finish (interrupt flag set)
  ADCSRA |= (1 << ADIF); // and clear it
  
  int result = ADCL; // read result - low register must be read first
  result |= (ADCH << 8);
  
  //PRR0 |= 1<<PRADC;
  return result;
}

volatile unsigned char timer_overflow_cnt;

ISR(TIMER1_OVF_vect) {
  timer_overflow_cnt++;
  if (timer_overflow_cnt == 8) {
    // Turn off the circuit.
    //PORT_PWR_CTRL &=  ~(1 << BIT_PWR_CTRL);
    // Immediately after writing ^ everything should lose power and nothing else will execute because there will be no more power.
    // Unless the circuit is on USB, in which case it will remain powered by the USB circuity, but at least the battery will stop
    // draining.
  }
}

void SetupAutoTurnOffTimer() {
  TIMSK1 = (1 << TOIE1);          // Enable timer1 overflow interrupt(TOIE1)
  sei();                  // Enable global interrupts by setting global interrupt enable bit in SREG
  
  TCCR1B |= (1 << CS10) | (1 << CS12);  // with an 8mhz input clock, if set to interrupt on overflow with the max 1024 prescaler, this will interrupt every 8 seconds.
  PRR0 &= ~(1<<PRTIM1);         // turn on the timer
}
