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
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>

#include <chrono>
#include <ctime>  
#include "sound.h"


char* soundCardName;
snd_pcm_t* soundCardHandle;

bool debug = false;
char *wavFileNames[32];
int wavFiles = 0;
int volume = 100;




bool setup() {
    int err;

	int seed;
	FILE *fp;
	fp = fopen("/dev/urandom", "r");
	int b=fread(&seed, sizeof(seed), 1, fp);
	fclose(fp);
	srand(seed);



    openSoundCard(getenv("AUDIODEV"));


	return true;
}                                             // end of setup function


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

	if (argc < 2) {
		return usage();
	}

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



	for (index = optind; index < argc; index++) {
		wavFileNames[wavFiles++] =argv[index];
	}

	if (wavFiles < 1) {
		printf("please supply at least 1 wav file to play\n");
		return false;
	}
	return true;
}




}

int main(int argc, char **argv)
{
	if (!commandLineOptions(argc, argv)) {
		return 1;
	}

	if (!setup()) {
		printf("setup failed\n");
		return 1;
	}


	for (int i = 0; i < wavFiles; ++i) {
		playWavFile(wavFileNames[i], 100.0kkk);
	}



	return 0;

}
