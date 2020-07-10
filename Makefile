
CFLAGS=-O2 -lm -lwiringPi -l pthread -lasound

wavPlayer: -lasound
	gcc ${CFLAGS} -o wavPlayer main.cpp

clean:
	rm -f wavPlayer
