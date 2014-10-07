all: avr-adc-host
avr-adc-host: main.c
	gcc -lrt -o avr-adc-host main.c
	strip avr-adc-host
clean:
	rm -f avr-adc-host