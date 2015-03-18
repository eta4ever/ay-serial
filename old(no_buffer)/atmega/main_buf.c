#define F_CPU 8000000UL // 8 МГц от встроенного генератора
#include <avr/io.h> // ввод-вывод
#include <util/delay.h> // задержки
#include <avr/interrupt.h> // прерывания

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

unsigned char const bigBufferPackets = 25; // пакетов в большом буфере
unsigned int const bigBufferSize = bigBufferPackets * 16; // размер большого буфера

unsigned char buffer[16]; // буфер принятых данных
unsigned char bytesReceived; // Счетчик принятых байт

unsigned char bigBuffer[bigBufferSize]; // большой буфер
unsigned int bbRead = 0; // указатель на элемент буфера для чтения
unsigned int bbWrite = 0; // указатель на элемент буфера для записи

// принять 16 байт по UART, записать в buffer
void receive16(void)
{
	for (bytesReceived=0; bytesReceived<16; bytesReceived++)
	{
		// ждем приемник
		while(!(UCSRA & (1<<RXC))) {}

		// забираем байт
		buffer[bytesReceived] = UDR;
	}
}

// увеличить на заданное значение указатель чтения, учесть кольцо
void bbReadInc(unsigned char increment)
{
	bbRead += increment;
	if (bbRead >= bigBufferSize)
	{
		bbRead -= bigBufferSize
	}
}

// увеличить на заданное значение указатель записи, учесть кольцо
void bbWriteInc(unsigned char increment)
{
	bbWrite += increment;
	if (bbWrite >= bigBufferSize)
	{
		bbWrite -= bigBufferSize
	}
}

// начальное заполнение буфера, заполняем большой буфер не полностью
void fillBigBuffer(void)
{
	for (unsigned char packetCount = 0; packetCount < (bigBufferPackets - 5); packetCount++)
	{
		receive16();
		for (unsigned char byteCount = 0; byteCount < 16; byteCount++)
		{
			bigBuffer[packetCount*16 + byteCount] = buffer[byteCount];
		}
	}

	// установить указатели чтения и записи
	bbRead = 0;
	bbWrite = 320;
}

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

	// первоначальное заполнение буфера
	fillBigBuffer();
}

// Принимаем 16 байт по прерыванию UART, дописываем в буфер
ISR(USART0_RX_vect)
{
	receive16();

	for (unsigned char byteCount=0; byteCount<16; byteCount++)
	{
		bigBuffer[bbWrite] = buffer[byteCount];
		bbWriteInc(1);
	}
}

// запись значения в наш "порт", состоящий из частей PB и PC
void valToPort(unsigned int val)
{

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

		// коррекция некоторых байт, т.к. не всем регистрам надо 8 бит
		int bbReadTmp = bbRead; // фиксировать bbRead
		
		bbReadInc(1); //1
		bigBuffer[bbRead] &= 0x0f;
		bbReadInc(2); //3
		bigBuffer[bbRead] &= 0x0f;
		bbReadInc(2); //5
		bigBuffer[bbRead] &= 0x0f;
		bbReadInc(1); //6
		bigBuffer[bbRead] &= 0x1f;
		bbReadInc(2); //8
		bigBuffer[bbRead] &= 0x1f;
		bbReadInc(1); //9
		bigBuffer[bbRead] &= 0x1f;
		bbReadInc(1); //10
		bigBuffer[bbRead] &= 0x1f;
		bbReadInc(3); //13
		bigBuffer[bbRead] &= 0x0f;

		bbRead = bbReadTmp; // вернуть bbRead		

		// записать в регистры 0-13. 15-16 не надо
		for (unsigned char byteCount=0; byteCount<14; byteCount++)
		{
			sendToAY(byteCount, bbRead);
			bbReadInc(1);
		}

		// пропустить регистры 15-16
		bbReadInc(2);

		// пауза 20 мс
		_delay_ms(20);

		// sendToAY(0x00, buffer[0]); // Period voice A
		// sendToAY(0x01, buffer[1]&0x0f); // Fine period voice A
		// sendToAY(0x02, buffer[2]); // Period voice B
		// sendToAY(0x03, buffer[3]&0x0f); // Fine period voice B
		// sendToAY(0x04, buffer[4]); // Period voice C
		// sendToAY(0x05, buffer[5]&0x0f); // Fine Period voice C
		// sendToAY(0x06, buffer[6]&0x1f); // Noise period
		// sendToAY(0x07, buffer[7]); // Mixer control 
		// sendToAY(0x08, buffer[8]&0x1f); // Volume control A
		// sendToAY(0x09, buffer[9]&0x1f); // Volume control B
		// sendToAY(0x0A, buffer[10]&0x1f); // Volume control C
		// sendToAY(0x0B, buffer[11]); // Envelope high period
		// sendToAY(0x0C, buffer[12]); // Envelope low period
		// sendToAY(0x0D, buffer[13]&0x0f); // Envelope shape 
    }
}