wavPlayer
---------
Read standard .wav file and write PCM data to ALSA driver.  If you're not familiar with ALSA, it's a very good and mature framework that provides an universal API to access and use sound card devices.  I'm currently using it to play music over an USB sound card on a Raspberry Pi zero.  Here are some links which describe ALSA in more detail. 

* https://en.wikipedia.org/wiki/Advanced_Linux_Sound_Architecture
* https://www.alsa-project.org/wiki/Main_Page
* https://www.alsa-project.org/alsa-doc/alsa-lib/

## Prerquisite

To compile, you will need to install the ALSA dev library:

    $ sudo apt install libasound2-dev
    
## Download & switching branches
After cloning the repository, you'll need to switch to the alsa branch.  Note that "projects" in the change dirctory command (cd ~/projects) is simply refering to the folder from where you cloned wavPlayer.

    $ git clone https://github.com/wryan67/wavPlayer.git
    $ cd ~/projects/wavPlayer
    $ git checkout alsa
    
## Compiling
I use Visual Studio, so if you're comfortable with VS, just open the .sln file.   If you're not so confortable with VisualStudio, or you just want to work directly on the RPi, then change folders again to the subdirectory "wavPlayer", and use make to compile the program.  The rest of the instructions are geared towards working directly on the RPi. 

    $ cd ~/projects/wavPlayer/wavPlayer
    make clean && make exe


## Device discovery
Use aplay to find your default devices.  I typically use the grep ^default because aplay -L by itself reads like a dictionary. 

    $ aplay -L | grep ^default
    default:CARD=ALSA
    default:CARD=Device

Or use my handy shell script which gives a little more info than the above command, and also provides the audio environment variable needed in the next step:

    $ ./listSoundCards.sh
    bcm2835 ALSA, bcm2835 ALSA
      export AUDIODEV='default:CARD=ALSA'

    Generic USB Audio Device, USB Audio
      export AUDIODEV='default:CARD=Device'


## Environment variable

Setup your audio device like this:

    $ export AUDIODEV='default:CARD=Device'

I recommend putting the export command in your .bashrc script so that it is available after each reboot.

## Demos
There are three demos:

* wavPlayer - plays standard .wav files
* tones - plays sinusoidal wave tones, with threading and sound mixing 
* pcm - the original example from ALSA Lib.

## Executing
After you find your sound card and setup the AUDIODEV environment variable, you should be ready to test.

    $ ./bin/tones
    { playes a tune }
    
    $$ bin/wavPlayer trolleyBell-east.wav
    Playing trolleyBell-west.wav ...

    $$ bin/wavPlayer trolleyBell-west.wav
    Playing trolleyBell-west.wav ...


    $ ./bin/wavPlayer -d rr-crossing-bells.wav
    audio format      1
    num channels      1
    sample rate       48000
    byte rate         96000
    block align       2
    bits per sample   16
    segment size      2
    checking overflow
    verifying write status
    flushing soundCardHandle



