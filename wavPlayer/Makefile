
CFLAGS=-O2
LIBS=-lm -lpthread -lasound

all: chmod wavPlayer pcm tones

wavPlayer: main.cpp -lasound
	gcc ${CFLAGS} -o wavPlayer main.cpp ${LIBS}

tones: tones.cpp -lasound
	gcc ${CFLAGS} -o tones tones.cpp ${LIBS}

pcm: pcm.c -lasound
	gcc ${CFLAGS} -o pcm pcm.c ${LIBS}

chmod:
	@chmod 755 listSoundCards.sh
clean:
	rm -f wavPlayer
