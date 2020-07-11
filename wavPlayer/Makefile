
CFLAGS=-O2
LIBS=-lm -lpthread -lasound

wavPlayer: main.cpp -lasound
	@chmod 755 listSoundCards.sh
	gcc ${CFLAGS} -o wavPlayer main.cpp ${LIBS}

clean:
	rm -f wavPlayer
