#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
// Firmware options
#define USART_BAUDRATE 460800
#define LED_PIN 1
#define ADC_COUNT 6
#define STARTUP_DELAY 1000
// Calculated UBRR value
#define UBRR (F_CPU / (16 * (uint32_t)USART_BAUDRATE) - 1)
// Global variables
uint8_t adc[ADC_COUNT]; // Buffer
uint8_t cur_in_adc; // Input byte index
uint8_t cur_out_adc; // Output byte index
// USART interrupt handler
ISR(USART_RXC_vect) {
	// Read data from USART
	uint8_t buffer = UDR;
	if (buffer == 0xFF) {
		if (cur_out_adc < ADC_COUNT) {
			// Return data byte from buffer
			UDR = adc[cur_out_adc];
			cur_out_adc++;
			// Activate led
			PORTB |= _BV(LED_PIN);
		} else {
			// Chain 0xFF
			UDR = 0xFF;
			// Deactivate led
			PORTB &= ~_BV(LED_PIN);
		}
	} else {
		// Chain data byte
		UDR = buffer;
		// Reset byte counter
		cur_out_adc = 0;
		// Deactivate led
		PORTB &= ~_BV(LED_PIN);
	}
}
// Main function
void main() {
	// Setup watchdog timer
	wdt_enable(WDTO_15MS);
	// Setup pin for led
	DDRB |= _BV(LED_PIN);
	// Blink led
	PORTB |= _BV(LED_PIN);
	for (uint8_t i = 0; i < STARTUP_DELAY / 5; i++) {
		_delay_ms(5);
		wdt_reset();
	}
	PORTB &= ~_BV(LED_PIN);
	// Setup ADC
	ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(ADLAR);
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
	// Setup USART
	UBRRL = UBRR & 0xFF;
	UBRRH = UBRR >> 8;
	UCSRA = 0;
	UCSRB = _BV(RXCIE) | _BV(RXEN) | _BV(TXEN);
	UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
	// Enable interrupts
	sei();
	// Main loop
	while (1) {
		// Reset watchdog timer
		wdt_reset();
		// Select ADC channel
		ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(ADLAR) | cur_in_adc;
		// Start conversion and wait until it performed
		ADCSRA |= _BV(ADIF) | _BV(ADSC);
		while (ADCSRA & _BV(ADSC));
		// Put value from ADC to buffer
		uint8_t value = ADCH;
		adc[cur_in_adc] = (value != 0xFF) ? value : 0xFE;
		// Switch to next channel
		cur_in_adc++;
		if (cur_in_adc >= ADC_COUNT) {
			cur_in_adc = 0;
		}
	}
}