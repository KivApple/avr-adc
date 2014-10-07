all:
	make -C firmware
	make -C host
clean:
	make -C firmware clean
	make -C host clean
flash:
	make -C firmware flash
fuse:
	make -C firmware fuse