/*
1. Дать запрос на хост
2. Принять от хоста 256 байт, записать их в кольцевой буфер
3. Дать запрос на хост
4. Принять от хоста 256 байт, записать их в кольцевой буфер

5. Начать играть.

6. По достижению позиции 256 - запросить у хоста еще.
7. Прерыванием прининять 256 байт.
8. По достижению 512 - запросить у хоста еще.
9. GOTO 6
*/

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

const unsigned int bufferSize = 512; // размер буфера
unsigned char buffer[512]; // буфер 512 байт
unsigned int readPos = 0; // позиция чтения из буфера
unsigned int writePos = 0; // позиция записи в буфер
const unsigned char message = 8; // сообщение для хоста

// увеличить на заданное значение позицию чтения, учесть кольцо
void readInc(unsigned char increment)
{
	readPos += increment;
	if (readPos >= bufferSize)
	{
		readPos -= bufferSize;
	}
}

// увеличить на заданное значение позицию записи, учесть кольцо
void writeInc(unsigned char increment)
{
	writePos += increment;
	if (writePos >= bufferSize)
	{
		writePos -= bufferSize;
	}
}

// отправить на хост запрос на выдачу 256 байт
void askForBytes()
{
	while ((UCSRA & (1 << UDRE)) == 0) {}; // Ждать свободного буфера
    UDR = message; // Отправить на хост запрос
}

// обработчик прерывания. Пишет байт в текущую позицию записи в буфер.
ISR(USART_RXC_vect)
{
	buffer[writePos] = UDR;
	writeInc(1);
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

// Инициализация
void setup(void)
{
	// режим портов
	DDRB = 0b11111111;
	DDRC = 0b00111111;

	sei(); // включение прерываний

	// настройка последовательного интерфейса

	// устанавливаем стандартный режим 8N1
	UCSRC=(1<<URSEL)|(3<<UCSZ0);

	// устанавливаем скорость 38400.
	// тут курим AVR Baudrate Calculator, почему
	// не все скорости одинаково полезны
	//UBRRL=25;
	UBRRL = 12;
	UBRRH = 0;

	// разрешить прием и передачу по USART
	// RXENable, TXENable в регистре UCSRB
	// и разрешить прерывание по приему (RXCIE)
	UCSRB=(1<<RXEN)|(1<<TXEN)|(1<<RXCIE);

	// пусть все устаканится
	_delay_ms(100);

	// запросить первые 256 байт
	askForBytes();

	// подождать
	_delay_ms(100);

	// запросить еще 256 байт
	askForBytes();

	// подождать
	_delay_ms(100);
}

int main(void)
{
	setup(); // Инициализация

	while(1)
	{

		// коррекция некоторых байт, т.к. не всем регистрам надо 8 бит
		int readPosTmp = readPos; // фиксировать позицию чтения
		
		readInc(1); //1
		buffer[readPos] &= 0x0f;
		readInc(2); //3
		buffer[readPos] &= 0x0f;
		readInc(2); //5
		buffer[readPos] &= 0x0f;
		readInc(1); //6
		buffer[readPos] &= 0x1f;
		readInc(2); //8
		buffer[readPos] &= 0x1f;
		readInc(1); //9
		buffer[readPos] &= 0x1f;
		readInc(1); //10
		buffer[readPos] &= 0x1f;
		readInc(3); //13
		buffer[readPos] &= 0x0f;

		readPos = readPosTmp; // вернуть позицию чтения		

		// записать в регистры 0-13. 15-16 не надо
		for (unsigned char byteCount=0; byteCount<14; byteCount++)
		{
			sendToAY(byteCount, buffer[readPos]);
			readInc(1);
		}

		// пропустить регистры 15-16
		readInc(2);

		// проверить позицию чтения, если 256 или 512 (0) - запросить данные
		if ((readPos == 256)|(readPos == 0))
		{
			askForBytes();
		}

		// пауза 20 мс
		_delay_ms(20);
    }
}