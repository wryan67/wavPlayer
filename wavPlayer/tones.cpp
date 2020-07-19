/*
*
*  $ sudo apt install libasound2-dev
* 
*  $ aplay -L | grep ^default     << to find your sound card
*  export AUDIODEV="default:CARD=Device"   << your sound card
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>

#include "Sound.h"


wavFormatType wavConfig;
long sampleRate = 48000;


bool debug = false;
int volume = 100;

int yankeeDoodle[99] = {
  5,5,6,7,5,7,6,0,5,5,6,7,5,0,4,0,5,5,6,7,8,7,6,5,4,2,3,4,5,0,5,0,-1
};
int yankeeDoodleBase[99] = {
  5,5,6,7,5,7,6,0,5,5,6,7,5,0,4,0,5,5,6,7,8,7,6,5,4,2,3,4,5,0,5,0,-1
};


bool demo1isRunning=false;
bool demo2isRunning=false;

float noteHz[11] = {
  0,        // rest
  195.9977, // 1  G3
  261.6256, // 3  C4
  293.6648, // 4  D4
  329.6276, // 5  E4
  349.2282, // 6  F4
  391.9954, // 7  G4
  440.0000, // 8  A4
  493.8833, // 9  B4
  523.2511, // 10 C5
  587.3295, // 11 D5
};


unsigned long long currentTimeMillis() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    return (unsigned long long)(currentTime.tv_sec) * 1000 +
        (unsigned long long)(currentTime.tv_usec) / 1000;
}

pthread_t threadCreate(void *(*method)(void *), const char *description) {
        pthread_t threadId;
        int status= pthread_create(&threadId, NULL, method, NULL);
        if (status != 0) {
                printf("%s::thread create failed %d--%s\n", description, status, strerror(errno));
                exit(9);
        }
        pthread_detach(threadId);
        return threadId;
}



bool setup() {
   if (debug) {
     printf("intialization commencing...\n"); 
     fflush(stderr);
   }
   int seed;
   FILE *fp;
   fp = fopen("/dev/urandom", "r");
   int b=fread(&seed, sizeof(seed), 1, fp);
   fclose(fp);
   srand(seed);



   wavConfig.audioFormat = 1;
   wavConfig.channels = 2;
   wavConfig.sampleRate = sampleRate;
   wavConfig.bitsPerSample = 16;
   wavConfig.blockAlign = wavConfig.bitsPerSample / 8;
   wavConfig.byteRate = wavConfig.sampleRate * wavConfig.channels * wavConfig.blockAlign;

   return true;
}



bool usage() {
	fprintf(stderr, "usage: wavPlayer [-p gpio] [-d] [-v 0-100]\n");
	fprintf(stderr, "d = debug\n");
	fprintf(stderr, "p = gpio pin\n");
	fprintf(stderr, "v = volume\n");
    fprintf(stderr, "Environment variables:\n");
    fprintf(stderr, "    AUDIODEV='your sound card'\n");

	return false;
}

bool commandLineOptions(int argc, char **argv) {
	int c, index;


	while ((c = getopt(argc, argv, "dv:p:")) != -1)
		switch (c) {
		case 'd':
			debug = true;
			break;
		case 'v':
			sscanf(optarg, "%d", &volume);
			break;
		case '?':
			if (optopt == 'm' || optopt == 't')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character \\x%x.\n", optopt);

			return usage();
		default:
			abort();
		}




	return true;
}



void *demo1(void*) {
   demo1isRunning=true;

//   sendHeader(soundCard1Handle);
   for (int i=0; yankeeDoodle[i]>=0; ++i) {
     playTone(noteHz[yankeeDoodle[i]], 0.300, wavConfig);
     playTone(0, 0.050, wavConfig);
   }
//   drainSound(soundCard1Handle);
   demo1isRunning=false;
   pthread_exit(0);
}

void *demo2(void*) {
   demo2isRunning=true;

// sendHeader(soundCard2Handle);
   for (int i=0; yankeeDoodleBase[i]>=0; ++i) {
//   playTone(soundCard2Handle, noteHz[yankeeDoodleBase[i]]/2);
   }
// drainSound(soundCard2Handle);
   demo2isRunning=false;
   pthread_exit(0);
}

int main(int argc, char **argv)
{


  if (!commandLineOptions(argc, argv)) {
    exit(EXIT_FAILURE);
  }

  if (!setup()) {
    printf("setup failed\n");
    exit(EXIT_FAILURE);
  }

   if (debug) {
     printf("intialization compelte\n"); 
     fflush(stderr);
   }

   long long start, end;

   start = currentTimeMillis();

//   soundCard1Handle=openSoundCard(getenv("AUDIODEV"));
//   soundCard2Handle=openSoundCard(getenv("AUDIODEV"));
   end = currentTimeMillis();
   if (debug) {
     printf("open elapsed=%lld\n",end-start);
   }

   threadCreate(demo1, "demo1");
//   threadCreate(demo2, "demo2");

   usleep(100 * 1000);

    while (demo1isRunning || demo2isRunning) {
      fflush(stdout);
      usleep(100 * 1000);
    }
//sleep(4294967295U);   // 3.27 years

//    snd_pcm_close(soundCard1Handle);
//    snd_pcm_close(soundCard2Handle);

	return 0;

}
