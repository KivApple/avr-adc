all: main.hex
main.hex: main.c
	avr-gcc -std=gnu99 -Os -DF_CPU=14745600 -mmcu=atmega8 main.c -o avr-adc.elf
	avr-gcc -S -std=gnu99 -Os -DF_CPU=14745600 -mmcu=atmega8 main.c
	avr-objcopy -j .text -j .data -O ihex avr-adc.elf avr-adc.hex
	avr-size avr-adc.elf
clean:
	rm -f avr-adc.hex avr-adc.elf main.s
flash:
	avrdude -c usbasp -p m8 -U flash:w:avr-adc.hex
fuse:
	avrdude -c usbasp -p m8 -U lfuse:w:0x3e:m -U hfuse:w:0xd9:m 