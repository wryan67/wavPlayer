
CFLAGS=-O2 -lm -lwiringPi -l pthread -lasound

wavPlayer: -lasound
	@chmod 755 listSoundCards.sh
	gcc ${CFLAGS} -o wavPlayer main.cpp

clean:
	rm -f wavPlayer
