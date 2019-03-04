#define F_CPU 8000000 // Clock Speed

#include <avr/io.h>
#include <util/delay.h>

#define DDR_SPI DDRB
#define PORT_SPI PORTB
#define PIN_SS PORTB2
#define PIN_MISO PORTB4
#define DD_MISO DDB4
#define DD_MOSI DDB3
#define DD_SCK DDB5
#define DD_SS DDB2

void USART_Init( unsigned int ubrr);
void USART_Transmit( unsigned int data );
unsigned int USART_Receive( void );
void USART_Flush( void );

void SPI_MasterInit(void);
void SPI_MasterTransmit(char cData);
char SPI_SlaveReceive(void);

#define BAUD 38400
#define MYUBRR F_CPU/16/BAUD-1

#include <util/setbaud.h>

char pageodata[260];
int pageaddress;

//int main( void )
//{
	//USART_Init(MYUBRR);
	//_delay_us(10);
	////SPI_MasterInit();
	//
	//while (1) {
		//unsigned int cmd = USART_Receive();
		//USART_Transmit(cmd);
		//
		/////* Configure SS as low (enabled) */
		////if (cmd == 'l') {
			////PORT_SPI &= ~PIN_SS;
			////
			//////USART_Transmit('l');	
		////
		/////* Configure SS as high (disabled) */
		////} else if (cmd == 'r') {
			////PORT_SPI |= PIN_SS;
////
			//////USART_Transmit('r');			
		////
		/////* Send/receive a byte (transaction) over the SPI connection */
		////} else if (cmd == 'b') {
			//////USART_Transmit('b');
			////
			////unsigned int to_slave = USART_Receive();
			////SPI_MasterTransmit(to_slave);
		////
			////char from_slave = SPI_SlaveReceive();
			////USART_Transmit(from_slave);
			////
		/////* Unknown command */
		////} else {
			////USART_Transmit('t');
		////}
	//}
	//
	//return 0;
//}

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

void USART_Init( unsigned int ubrr)
{
	/*Set baud rate */
	//UBRR0H = (unsigned char)(ubrr>>8);
	//UBRR0L = (unsigned char)ubrr;
	UBRR0H = 0;
	UBRR0L = 12;
	/* Enable receiver and transmitter */
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	/* Set frame format: 8data, 2stop bit */
	//UCSR0C = (1<<USBS0)|(3<<UCSZ00);
	UCSR0C = (3<<UCSZ00);
}

void USART_Transmit( unsigned int data )
{
	/* Wait for empty transmit buffer */
	while ( !( UCSR0A & (1<<UDRE0)))
		;
	/* Copy 9th bit to TXB8 */
	//UCSR0B &= ~(1<<TXB80);
	//if ( data & 0x0100 )
	//UCSR0B |= (1<<TXB80);
	/* Put data into buffer, sends the data */
	UDR0 = data;
}

unsigned int USART_Receive( void )
{
	unsigned char status, resh, resl;
	/* Wait for data to be received */
	while ( !(UCSR0A & (1<<RXC0)) )
		;
	/* Get status and 9th bit, then data */
	/* from buffer */
	status = UCSR0A;
	resh = UCSR0B;
	resl = UDR0;
	/* If error, return -1 */
	//if ( status & (1<<FE0)|(1<<DOR0)|(1<<UPE0) )
	//	return -1;
	/* Filter the 9th bit, then return */
	//resh = (resh >> 1) & 0x01;
	//return ((resh << 8) | resl);
	return resl;
}

void USART_Flush( void )
{
	unsigned char dummy;
	while ( UCSR0A & (1<<RXC0) ) dummy = UDR0;
}

void SetupSoftwareUartIoPorts() {
	// PD1 (normally UART TXD) is our input from USB-serial
	// PD0 (normally UART RXD) is our output to USB-serial

	// Set receive pin to input, tri-state
	DDRD &= ~((1 << DDRD1) | (1 << DDRD3));
	PORTD &= ~((1 << PORTD1) | (1 << DDRD3));
	
	// Set transmit pin to regular output
	DDRD |= (1 << DDRD0) | (1 << DDRD4);
	PORTD |= (1 << PORTD0) | (1 << DDRD4);
}

void SetupFpgaResetAndDone() {
	// PE3 is CRESET#
	// PC0 is CDONE_B and should be input
	// Normally CRESET is open-circuit so it can float high
	// When we want to reset the FPGA, turn it to an input, low, so it pulls the FPGA low
	
	// Since we're not using PORTC for anything else, just make it all input
	DDRC = 0;
	PORTC = 0;
	
	// Initially set PORTE as input as well
	DDRE = 0;
	PORTE = 0;
}

void PutFpgaIntoReset() {
	// Not needed because already set low for Tri-state in initialization
	// PORTE |= ~(1 << PORTE3);	// Set output low
	DDRE |= (1 << DDRE3);		// Turn pin into output
}

void PutFpgaOutOfReset() {
	DDRE &= ~(1 << DDRE3);		// Turn pin into input
	// Not needed because already set low for Tri-state in initialization
	// PORTE |= ~(1 << PORTE3);	// Set output low
}

unsigned char ReadFpgaDoneBit() {
	if (PINC & (1 << PINC0)) return 1;
	else return 0;
}

unsigned char ReadBit() {
	if (PIND & (1 << PIND1)) return 1;
	else			  return 0;
}

void WriteBit(unsigned char value) {
	if (value) PORTD |= (1 << PORTD0);
	else       PORTD &= ~(1 << PORTD0);
}

unsigned char ReadBitFpga() {
	if (PIND & (1 << PIND3)) return 1;
	else			  return 0;
}

void WriteBitFpga(unsigned char value) {
	if (value) PORTD |= (1 << PORTD4);
	else       PORTD &= ~(1 << PORTD4);
}


unsigned char ReceiveByte() {
	unsigned char incoming = 0;
	// Wait for rx to go low
	while (ReadBit());
	// Wait half a bit period for the first bit
	_delay_us(13);
	
	// Wait for the next bit, read it, then add it to our accumulated byte
	// (LSB is sent first so shift in from the left
	for (unsigned char i=0;i<8;i++) {
		_delay_us(24);
		incoming = (ReadBit() << 7) | (incoming >> 1);
	}
	
	// Delay another bit so we're in the middle of the stop bit when we return
	_delay_us(26);
	
	return incoming;
}

void TransmitByte(unsigned char outgoing) {
	// Set tx low, and wait 1 bit period
	WriteBit(0);
	_delay_us(26);
	
	// Shift out bits one by one
	for (unsigned char i=0;i<8;i++) {
		WriteBit(outgoing & 1);
		outgoing = outgoing >> 1;
		_delay_us(24);
	}

	// Set tx high, ending the transmission, and wait 1 bit period for the stop bit
	WriteBit(1);
	_delay_us(26);
}

int main(void) {
	unsigned char cmd;
	SetupSoftwareUartIoPorts();
	SPI_MasterInit();
	SetupFpgaResetAndDone();
	while (1) {
		cmd = ReceiveByte();
		//TransmitByte(b);
		/* Configure SS as low (enabled) */
		if (cmd == 'l') {
			PORT_SPI &= ~(1<<PIN_SS);
		
			//USART_Transmit('l');
		
			/* Configure SS as high (disabled) */
		} else if (cmd == 'r') {
			PORT_SPI |= (1<<PIN_SS);
		
			//USART_Transmit('r');
		
			/* Send/receive a byte (transaction) over the SPI connection */
		} else if (cmd == 'b') {
			//USART_Transmit('b');
		
			unsigned char to_slave = ReceiveByte();
			SPI_MasterTransmit(to_slave);
		
			unsigned char from_slave = SPI_SlaveReceive();
			TransmitByte(from_slave);
		
			/* Program a page of data */
		} else if (cmd == 'p') {
			// Receive 256 + 3 address + 1 command byte
			for (int i=0;i<260;i++) {
				pageodata[i] = ReceiveByte();
			}
			
			TransmitByte('a');
			
			PORT_SPI &= ~(1<<PIN_SS);
			for (int i=0;i<260;i++) {
				SPI_MasterTransmit(pageodata[i]);				
			}
			PORT_SPI |= (1<<PIN_SS);
			
			TransmitByte('d');
			
			/* Verify a page of data */
		} else if (cmd == 'v') {
			// Read 1 command byte and 3 address bytes
			// (just store it in the page array for now since it won't be needed once transmitted to the spi flash)
			for (int i=0;i<4;i++) {
				pageodata[i] = ReceiveByte();
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
				TransmitByte(pageodata[i]);
			}
				
			/* Unknown command */
		} 
		else if (cmd == 'f') {
			// Fast program. Overlap sending the spi bytes with the receive bytes.	
			// Receive a byte, send it to spi, the immediately receive another byte from serial knowing that the SPI will
			// finish by the time the next one is done.
			PORT_SPI &= ~(1<<PIN_SS);
			SPDR0 = ReceiveByte();
			for (int i=0;i<259;i++) {
				SPI_MasterTransmit_WaitBefore(ReceiveByte());
			}
			while(!(SPSR0 & (1<<SPIF)));
			PORT_SPI |= (1<<PIN_SS);
			TransmitByte('a');
			TransmitByte('d');
		}
		
		else if (cmd == 'g') {
			// Fast verify.
			PORT_SPI &= ~(1<<PIN_SS);
			SPDR0 = ReceiveByte();	// receive a byte, start is sending, immediately go back to receiving a new byte
			for (int i=0;i<3;i++) {
				SPI_MasterTransmit_WaitBefore(ReceiveByte());
			}

			// since using WaitBefore, old byte still sending, so wait for it to finish
			while(!(SPSR0 & (1<<SPIF)));
			
			SPI_MasterTransmit(0);		// get byte 0 of page data into SPDR0
			for (int i=0;i<255;i++) {
				SPDR0 = 0;				// unless it's the last byte, start a new transmission
				TransmitByte(SPDR0);	// transmit old received byte (starting with byte 0 and ending with byte 254)
			}			
			TransmitByte(SPDR0);		// transmit byte 255 but don't start a new transmission
			while(!(SPSR0 & (1<<SPIF)));
			PORT_SPI |= (1<<PIN_SS);
		}
		
		else if (cmd == 'q') {
			PutFpgaIntoReset();
			TransmitByte('q');
		}
		
		else if (cmd == 'w') {
			PutFpgaOutOfReset();
			TransmitByte('w');
		}
		
		else if (cmd == 'e') {
			TransmitByte(ReadFpgaDoneBit());
		}
		
		else if (cmd == 't') {
			ReleaseSpiFlashFromPowerDown();
			TransmitByte('t');
		}
		
		else if (cmd == 'z') {
			TransmitByte('z');
			unsigned char breakcounter = 0; 
			while (1) {
			
				// Provide a way to regain control from FPGA passthrough.
				// If a break symbol is transmitted (which is just the serial
				// line going low for much longer than a single byte), break
				// out of the passthrough loop and regain control.	
				unsigned char from_computer = ReadBit();
				if (from_computer) breakcounter = 0;
				else breakcounter++;
				if (breakcounter == 255) break;
				
				WriteBitFpga(from_computer);
				WriteBit(ReadBitFpga());
			}
		}
		
		else {
			TransmitByte('?');
		}

	}
}