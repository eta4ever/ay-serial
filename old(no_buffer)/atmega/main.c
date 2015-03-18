#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

	//DA0	PB2
	//DA1	PB3
	//DA2	PB4
	//DA3	PB5
	//DA4	PC0
	//DA5	PC1
	//DA6	PC2
	//DA7	PC3
	//BC1	PC4
	//BC2	+Vcc
	//BDIR	PC5

unsigned char buffer[16]; // буфер принятых данных
unsigned char bytesReceived; // Счетчик принятых байт

// Инициализация
void setup(void)
{
	// режим портов
	DDRB = 0b11111111;
	DDRC = 0b00111111;

	
	// настройка последовательного интерфейса

	// устанавливаем стандартный режим 8N1
	UCSRC=(1<<URSEL)|(3<<UCSZ0);

	// устанавливаем скорость 38400.
	// тут курим AVR Baudrate Calculator, почему
	// не все скорости одинаково полезны
	//UBRRL=25;
	UBRRL = 12;
	UBRRH=0;

	// разрешить прием и передачу по USART
	// RXENable, TXENable в регистре UCSRB
	UCSRB=(1<<RXEN)|(1<<TXEN);

}

// запись значения в наш "порт", состоящий из частей PB и PC
void valToPort(unsigned int val){

	 // для порта B взять младший полубайт
	 // и сдвинуть на 2 влево , т.к. надо выводить начиная с PB2	
	 unsigned int PBPart = val & 0b00001111;
	 PBPart <<= 2; 
	 
	 // для порта C взять старший полубайт
	 // и сдвинуть вправо, т.к. выводить начиная с PC0
	 unsigned int PCPart = val & 0b11110000;
	 PCPart >>= 4;

	 // записать полученное в порты
	 PORTB = PBPart;
	 PORTC = PCPart;
}

// отправка в регистр AY значения
void sendToAY(unsigned int addr, unsigned int data)
{
	//неактивный режим: BC1=0, BDIR=0
	PORTC &= 0b11001111;

	//установка адреса
	valToPort(addr);

	//фиксация адреса: BC1=1, BDIR=1
	PORTC |= 0b00110000; 
	_delay_us(1);
	PORTC &= 0b11001111;

	//установка значения
	valToPort(data);

	//фиксация значения: BC1=0, BDIR=1
	PORTC |= 0b00100000; 
	_delay_us(1);
	PORTC &= 0b11011111;
}


int main(void)
{
	setup(); // Инициализация

	while(1)
	{
	// принимаем 16 байт
		for (bytesReceived=0; bytesReceived<16; bytesReceived++)
		{
			// ждем приемник
			while(!(UCSRA & (1<<RXC))) {}

			// забираем байт
			buffer[bytesReceived] = UDR;
		}

		sendToAY(0x00, buffer[0]); // Period voice A
		sendToAY(0x01, buffer[1]&0x0f); // Fine period voice A

		sendToAY(0x02, buffer[2]); // Period voice B
		sendToAY(0x03, buffer[3]&0x0f); // Fine period voice B

		sendToAY(0x04, buffer[4]); // Period voice C
		sendToAY(0x05, buffer[5]&0x0f); // Fine Period voice C

		sendToAY(0x06, buffer[6]&0x1f); // Noise period

		sendToAY(0x07, buffer[7]); // Mixer control 

		sendToAY(0x08, buffer[8]&0x1f); // Volume control A
		sendToAY(0x09, buffer[9]&0x1f); // Volume control B
		sendToAY(0x0A, buffer[10]&0x1f); // Volume control C

		sendToAY(0x0B, buffer[11]); // Envelope high period
		sendToAY(0x0C, buffer[12]); // Envelope low period
		sendToAY(0x0D, buffer[13]&0x0f); // Envelope shape 
    }
}