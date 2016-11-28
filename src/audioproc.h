/*
 * audioproc.h
 *
 *  Created on: Apr 2, 2013
 *      Author: luo
 */

#ifndef AUDIOPROC_H_
#define AUDIOPROC_H_
#include <errno.h>
#include <fcntl.h>
#include <gulliver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/slogcodes.h>
#include <ctype.h>
#include <limits.h>
#include <screen/screen.h>
#include <sys/asoundlib.h>

#include <bps/bps.h>
#include <bps/audiomixer.h>
#include <bps/audiodevice.h>
#include <bps/dialog.h>
#include <bps/navigator.h>

#include <time.h>
#include <math.h>
#include <sched.h>
#include <errno.h>
#include <pthread.h>

#include "AudioBufferPipe.h"

class BpsHandler;

typedef struct {
    snd_pcm_t *pcm_handle;
    snd_pcm_info_t info;
    snd_pcm_channel_params_t pp;
    snd_pcm_channel_setup_t setup;
    snd_pcm_channel_info_t pi;
    snd_mixer_t *mixer_handle;
    snd_mixer_group_t group;

    char *name;
    int card;
    int dev_hdl;

    int   block_size;
    char *sample_buffer;
} pcm_device_t;

class AudioProc
{
public:
	AudioProc();
	~AudioProc();

	static const char OUT_DEVICE[];
	static const char IN_DEVICE[];


	int init();
	int setCompression( double compression );
	int setScale( double scale );
	int setDistortion( double dist );

	int fi();

	void captureMain();
	void playbackMain();
	void mixerMain();
	void threadMain();

//	static void* entry( void (*target)() ) {
	static void* entry( void *ap) {
		((AudioProc*)(ap))->threadMain();
		return 0;
	}

	static void* capture_entry( void *ap) {
		((AudioProc*)(ap))->captureMain();
		return 0;
	}
	static void* playback_entry( void *ap) {
		((AudioProc*)(ap))->playbackMain();
		return 0;
	}
	static void* mixer_entry( void *ap) {
		((AudioProc*)(ap))->mixerMain();
		return 0;
	}

	// device control
	int setup_ouput_device();
	int setup_input_device();
	int setup_device( pcm_device_t *dev, int mode, int32_t channel );
	int restart_devices();
	char* pcm_device_to_string( pcm_device_t* dev, char* str, int len  );


	// processing
	void generate_tone( char* block, int size, double freqHZ, double amplitude );
	void flatten( char* block, int size, double threshold, double scale );
	void compress( char* block, int size, double threshold, double scale );

    // device control
	AudioBufferPipe *m_pipe;

    char *outdev_name;
    char *indev_name;
    pcm_device_t m_outdev;
    pcm_device_t m_indev;

    // Settings
	int sample_rate;
	int sample_channels;
	int sample_bits;
	int32_t format;

	int frameInterval;
	int blockSize;
	int transactionSize;

	int do_shutdown;

	double phaseCarry;
	int consumeFlag;
	// stats tracking
	int frameCounter;
	unsigned int latency;

	// Controls
	double compression_threshold;
	double compression_scale;
	double squelch_threshold;
	unsigned int distortion_threshold;
	int hold;

	pthread_t m_thread;
	pthread_t m_captureThread;
	pthread_t m_playbackThread;
	pthread_t m_mixerThread;

	BpsHandler *m_bpsHandler;
};

#include <bb/AbstractBpsEventHandler>
#include <QObject>
class BpsHandler : public QObject, public bb::AbstractBpsEventHandler {
    Q_OBJECT
    AudioProc *parent_ap;
public:
    BpsHandler();
    ~BpsHandler();
    void init( AudioProc *ap );
    virtual void event(bps_event_t *event);
};

extern AudioProc *ap;

#endif /* AUDIOPROC_H_ */
