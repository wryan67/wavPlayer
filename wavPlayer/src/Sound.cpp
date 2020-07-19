#include "Sound.h"
#include <vector>
#include <mutex>
#include <thread>

using namespace std;


vector<snd_pcm_t*> soundCardHandles;
mutex soundPool;
mutex soundPoolInit;
bool  soundPoolRunning = false;

#define minSoundPoolSize 10



snd_pcm_t* openSoundCard(const char* soundCardName) {
    int err;

    if (strlen(soundCardName) == 0) {
        soundCardName = "default";
    }

    snd_pcm_t* soundCardHandle;

    if ((err = snd_pcm_open(&soundCardHandle, soundCardName, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    return soundCardHandle;
}


void drainSound(snd_pcm_t* soundCardHandle) {
    int err = snd_pcm_drain(soundCardHandle);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_drain failed: %s\n", snd_strerror(err));
    }
}

void closeSoundCard(snd_pcm_t* soundCardHandle) {
    drainSound(soundCardHandle);
    int err = snd_pcm_close(soundCardHandle);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_close failed: %s\n", snd_strerror(err));
    }
}



void soundCardPool() {

    while (true) {
        while (soundCardHandles.size() < minSoundPoolSize) {
            soundPool.lock();
            snd_pcm_t* handle = openSoundCard(getenv("AUDIODEV"));
            soundCardHandles.push_back(handle);
            soundPool.unlock();
        }
        usleep(2000);
    }
}

snd_pcm_t* getSoundCardHandle() {
    if (!soundPoolRunning) {
        soundPoolInit.lock();
        if (!soundPoolRunning) {
            thread wav = thread(soundCardPool);
            wav.detach();
            while (soundCardHandles.size() < 2) {
                usleep(1000);
            }
            soundPoolRunning = true;
        }
        soundPoolInit.unlock();
    }

    soundPool.lock();
    if (soundCardHandles.size() < 1) {
        fprintf(stderr, "unable to obtain sound card handle");
        soundPool.unlock();
        return NULL;
    }
    snd_pcm_t* handle = soundCardHandles.back();
    soundCardHandles.pop_back();
    soundPool.unlock();
    return handle;
}
