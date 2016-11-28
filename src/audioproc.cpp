/*
 * audioproc.cpp
 *
 *  Created on: Apr 2, 2013
 *      Author: luo
 */
#include "audioproc.h"
#include <bps/bps.h>

#ifndef max
    #define max(a,b) ((a) < (b) ? (a) : (b))
#endif

const char AudioProc::OUT_DEVICE[] = "pcmPreferred";
const char AudioProc::IN_DEVICE[] = "pcmPreferred";
AudioProc *ap;


AudioProc::AudioProc() :
				outdev_name((char *)OUT_DEVICE),
				indev_name((char *)IN_DEVICE),
				sample_rate(11025),
				sample_channels(1),
				sample_bits(16),
				format(SND_PCM_SFMT_S32_LE),
				transactionSize(1),
				frameInterval(20),
				do_shutdown(0)
{
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );
	pthread_create( &m_thread, NULL, &entry, (void*)this );
}

AudioProc::~AudioProc()
{
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );
    do_shutdown = 1;

    sched_yield();

	pthread_join( m_thread, NULL );
}

int AudioProc::init()
{
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );
    fprintf( stderr, "SampleRate = %d, channels = %d, SampleBits = %d\n", sample_rate, sample_channels, sample_bits );

	setup_input_device();
	setup_ouput_device();

	//int samplesPerFrame = sample_rate*frameInterval/1000;
	//int samplesPerBlock = m_indev.block_size / (sample_bits/8);
	//int blocksPerFrame = samplesPerFrame / samplesPerBlock;
	blockSize = m_indev.block_size;
	//transactionSize = 1;

	m_pipe = new AudioBufferPipe();
	if( m_pipe->init( blockSize * transactionSize, 4, 3 ) != EOK )
		return -1;
    fprintf( stderr, "frameInterval = %d\n", frameInterval );

    union {
          uint32_t i;
          char c[4];
    } endint = {0x01020304};
    if(endint.c[0] == 1)
    {
        fprintf( stderr, "system is BE\n" );
    } else {
        fprintf( stderr, "system is LE\n" );
    }

    frameCounter = 0;

    compression_threshold = 0x03000000;
    compression_scale = 0.7;
    squelch_threshold = 0x00000100;
    distortion_threshold = 0xeFFFFFFF;
    hold = 0;
	phaseCarry = 0;

    fprintf( stderr, "Init BPS\n" );
	m_bpsHandler = new BpsHandler();
	m_bpsHandler->init(this);

    fprintf( stderr, "Init start worker threads\n" );
	pthread_create( &m_captureThread, NULL, &capture_entry, (void*)this );
	pthread_create( &m_playbackThread, NULL, &playback_entry, (void*)this );
	pthread_create( &m_mixerThread, NULL, &mixer_entry, (void*)this );

	fprintf( stderr, "Init done\n" );
	return 0;
}

int AudioProc::setCompression( double compression ) {
	compression_threshold = compression;
	fprintf( stderr, "%s:  %f \n", __func__, compression_threshold );
	return 0;
}
int AudioProc::setScale( double scale ){
	compression_scale = scale;
	fprintf( stderr, "%s:  %f \n", __func__, compression_scale );
	return 0;
}
int AudioProc::setDistortion( double dist ){
	if(dist == 10.0)
		distortion_threshold = (unsigned int)compression_threshold;
	else if (dist == 0.0)
		distortion_threshold = 0xEFFFFFFF;
	else
		distortion_threshold = (unsigned int)((double)0xEFFFFFFF)*(pow(2.0,-dist/3.0));
	fprintf( stderr, "%s:  0x%08X \n", __func__, distortion_threshold );
	return 0;
}


int AudioProc::fi()
{
    snd_mixer_close(m_outdev.mixer_handle);
    snd_pcm_close(m_outdev.pcm_handle);
    snd_mixer_close(m_indev.mixer_handle);
    snd_pcm_close(m_indev.pcm_handle);

    delete m_pipe;

    free(m_outdev.sample_buffer);
    free(m_indev.sample_buffer);

    m_outdev.sample_buffer = NULL;
    m_indev.sample_buffer = NULL;

	return 0;
}

void AudioProc::captureMain()
{
	int ret;
	fd_set rfds;
	fd_set wfds;

    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	snd_pcm_plugin_flush( m_indev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK );
//	snd_pcm_plugin_set_disable ( m_indev.pcm_handle, PLUGIN_DISABLE_BUFFER_PARTIAL_BLOCKS );

	while(!do_shutdown)
	{
	//	snd_pcm_plugin_flush( m_indev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK );
		while(hold)
			usleep(1000);
		FD_SET(snd_pcm_file_descriptor(m_indev.pcm_handle, SND_PCM_CHANNEL_CAPTURE), &rfds);
		ret = snd_pcm_file_descriptor(m_indev.pcm_handle, SND_PCM_CHANNEL_CAPTURE);

		if( select(ret + 1, &rfds, &wfds, NULL, NULL) == -1 ) {
			fprintf( stderr, "%s:%d select \n", __func__, __LINE__ );
			goto fail;
		}

		if( FD_ISSET( snd_pcm_file_descriptor( m_indev.pcm_handle, SND_PCM_CHANNEL_CAPTURE ), &rfds ) )
		{
		    //fprintf( stderr, "%s:%d  capture device ready \n", __func__, __LINE__ );
			AudioBufferElement* writeBuffer;
			//writeBuffer = m_pipe->get_write_buffer();
			writeBuffer = m_pipe->m_pipes[2].get();
			if( writeBuffer == 0 )
			{
	    		usleep( 1000 );
				continue;
			}

            snd_pcm_channel_status_t status;
            int     read = 0;

            //m_pipe->printTimeDelta();
            //fprintf( stderr, "%s:%d snd_pcm_read 0x%08X \n", __func__, __LINE__, (int)writeBuffer->buffer );
            read = snd_pcm_read ( m_indev.pcm_handle, writeBuffer->buffer, blockSize*transactionSize );
        	writeBuffer->length = read;

            if( read < 0 )
            {
            	writeBuffer->length = 0;

            	fprintf( stderr, "%s:%d  snd_pcm_plugin_read returned %d \n", __func__, __LINE__, read );
            	memset (&status, 0, sizeof (status));

            	status.channel = SND_PCM_CHANNEL_CAPTURE;
            	if( snd_pcm_plugin_status ( m_indev.pcm_handle, &status) < 0)
            	{
            	    fprintf( stderr, "%s:%d overrun: capture channel status error \n", __func__, __LINE__ );
                    goto fail;

            	}
        	    fprintf( stderr, "%s:%d status = %d\n", __func__, __LINE__, status.status );

            	if( status.status == SND_PCM_STATUS_READY || status.status == SND_PCM_STATUS_OVERRUN )
            	{
            	    if (snd_pcm_plugin_prepare ( m_indev.pcm_handle, SND_PCM_CHANNEL_CAPTURE) < 0)
            		{
                		fprintf( stderr, "%s:%d overrun: capture channel prepare error \n", __func__, __LINE__ );
            	        goto fail;
            		}
            	}
            } else if( read != blockSize*transactionSize ){
        		fprintf( stderr, "%s:%d abnormal read size = %d \n", __func__, __LINE__, read );
            	writeBuffer->length = blockSize*transactionSize;
            }
            m_pipe->m_pipes[2].put();
    		//m_pipe->commit_write_buffer();
		}
	}
	fail:
	exit(0);
}

void AudioProc::playbackMain()
{
	int ret;
	fd_set rfds;
	fd_set wfds;

    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	snd_pcm_plugin_flush(m_outdev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK);

	while(!do_shutdown)
	{
		while(hold)
			usleep(1000);

		// Add stdin to fdset if this process is in foreground
        if (tcgetpgrp(0) == getpid())
            FD_SET(STDIN_FILENO, &rfds);

        FD_SET(snd_pcm_file_descriptor(m_outdev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK), &wfds);
        ret = snd_pcm_file_descriptor(m_outdev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK);

        /* Wait for FD to become ready*/
        if (select(ret + 1, &rfds, &wfds, NULL, NULL) == -1) {
            fprintf( stderr, "%s:%d select \n", __func__, __LINE__ );
            goto fail;
        }

        /* Output device is ready  */
        if( FD_ISSET( snd_pcm_file_descriptor( m_outdev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK ), &wfds ) ) {
//            fprintf( stderr, "%s:%d Output device ready \n", __func__, __LINE__ );
        	char* buffer;
        	AudioBufferElement *readBuffer = 0;
    		//readBuffer = m_pipe->get_read_buffer();
        	readBuffer = m_pipe->m_pipes[0].get();
    		if( readBuffer == 0 )
    		{
    			usleep(1000);
                continue;
    		}else
    		{
        		buffer = readBuffer->buffer;
    		}

            snd_pcm_channel_status_t status;
            int written = 0;

            //generate_tone( m_outdev.sample_buffer, m_outdev.block_size, ((grandcounter++))%4000 + 100, 120 );
            //fprintf( stderr, "f = %f Hz \n", 500.0*((grandcounter>>0x6)%20));
            //written = snd_pcm_plugin_write( m_outdev.pcm_handle, m_outdev.sample_buffer, m_outdev.block_size );

            //m_pipe->printTimeDelta();
            //fprintf( stderr, "%s:%d snd_pcm_plugin_write 0x%08X \n", __func__, __LINE__, (int)readBuffer->buffer );
            written = snd_pcm_plugin_write( m_outdev.pcm_handle, readBuffer->buffer, blockSize*transactionSize );
            if( written < readBuffer->length ) {
            	memset(&status, 0, sizeof(status));
            	status.channel = SND_PCM_CHANNEL_PLAYBACK;
            	if( snd_pcm_plugin_status(m_outdev.pcm_handle, &status ) < 0 ) {
                    fprintf( stderr, "%s:%d playback channel status error\n", __func__, __LINE__ );
                    goto fail;
            	}

            	if( status.status == SND_PCM_STATUS_READY || status.status == SND_PCM_STATUS_UNDERRUN ) {
            		fprintf( stderr, "%s:%d playback channel error %d\n", __func__, __LINE__, status.status );
            		if (snd_pcm_plugin_prepare(m_outdev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK) < 0) {
                        fprintf( stderr, "%s:%d playback channel snd_pcm_plugin_prepare error\n", __func__, __LINE__ );
                        goto fail;
            		}
            	}

            	if (written < 0)
            		written = 0;
                //fprintf( stderr, "%s:%d Incomplete write.\n    rewrite snd_pcm_plugin_write 0x%08X 0x%08X \n", __func__, __LINE__, (int)readBuffer->buffer, written );
            	written += snd_pcm_plugin_write( m_outdev.pcm_handle, readBuffer->buffer + written, blockSize*transactionSize - written );
            }
            m_pipe->m_pipes[0].put();
    		//usleep( 1000 );
        }
	}

fail:
	snd_pcm_plugin_flush(m_outdev.pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
	exit(0);
}
void AudioProc::mixerMain()
{
	int ret;
	fd_set rfds;
	fd_set wfds;

    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	while(!do_shutdown)
	{
		FD_SET(snd_mixer_file_descriptor(m_indev.mixer_handle), &rfds);
		FD_SET(snd_mixer_file_descriptor(m_outdev.mixer_handle), &rfds);

        ret = max( snd_mixer_file_descriptor(m_outdev.mixer_handle),
        		snd_mixer_file_descriptor(m_indev.mixer_handle) );

		 if (select(ret + 1, &rfds, &wfds, NULL, NULL) == -1) {
		            fprintf( stderr, "%s:%d select \n", __func__, __LINE__ );
		            return;
		 }

	     if (FD_ISSET (snd_mixer_file_descriptor (m_outdev.mixer_handle), &rfds))
	        {
				fprintf( stderr, "%s:%d playback mixer event\n", __func__, __LINE__ );
	            snd_mixer_callbacks_t callbacks = {
	                0, 0, 0, 0
	            };

	            snd_mixer_read (m_outdev.mixer_handle, &callbacks);
	        }

	        if (FD_ISSET (snd_mixer_file_descriptor (m_indev.mixer_handle), &rfds))
	        {
				fprintf( stderr, "%s:%d capture mixer event\n", __func__, __LINE__ );
	            snd_mixer_callbacks_t callbacks = {
	                0, 0, 0, 0
	            };

	            snd_mixer_read (m_indev.mixer_handle, &callbacks);
	        }

	}
	return;
}
void AudioProc::threadMain()
{
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

	init();

	while(!do_shutdown)
	{
    	AudioBufferElement *buffer = 0;
    	buffer = m_pipe->m_pipes[1].get();
    	if( buffer )
    	{
    		//generate_tone( buffer->buffer, buffer->length, 2100, 120 );
			compress( buffer->buffer, blockSize*transactionSize, compression_threshold, compression_scale );
			m_pipe->m_pipes[1].put();
    	}else
    	{
    		usleep(1000);
    	}
	}
	pthread_join( m_captureThread, NULL );
	pthread_join( m_playbackThread, NULL );
	pthread_join( m_mixerThread, NULL );

	fi();
	exit(0);
}


int AudioProc::setup_ouput_device( )
{
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

    m_outdev.name = outdev_name;
    setup_device( &m_outdev, SND_PCM_OPEN_PLAYBACK, SND_PCM_CHANNEL_PLAYBACK );
    return 0;
}
int AudioProc::setup_input_device()
{
	int ret;
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

    m_indev.name = indev_name;
    //setup_device( &m_indev, SND_PCM_OPEN_CAPTURE, SND_PCM_CHANNEL_CAPTURE );

    pcm_device_t *dev = &m_indev;

    if ( ( ret = snd_pcm_open_preferred (&dev->pcm_handle, &dev->card, &dev->dev_hdl, SND_PCM_OPEN_CAPTURE ) ) < 0 )
    {
        fprintf( stderr, "%s:%d snd_pcm_open_preferred failed with code = %d (%s)\n", __func__, __LINE__, ret, strerror(-ret) );
        return -1;
    }

    if( ( ret = snd_pcm_info(dev->pcm_handle, &dev->info ) ) < 0) {
    	fprintf( stderr, "snd_pcm_info failed: %s\n", snd_strerror(ret));
        goto fail;
     }

    dev->card = dev->info.card;

    /* channel info */
    memset(&dev->pi, 0, sizeof(dev->pi) );
    dev->pi.channel = SND_PCM_CHANNEL_CAPTURE;
    if( ( ret = snd_pcm_plugin_info(dev->pcm_handle, &dev->pi ) ) < 0) {
    	fprintf( stderr, "snd_pcm_plugin_info failed: %s\n", snd_strerror(ret));
        goto fail;
    }

    /* channel params */
    memset(&dev->pp, 0, sizeof(dev->pp));
    dev->pp.mode = SND_PCM_MODE_BLOCK;
    dev->pp.channel = SND_PCM_CHANNEL_CAPTURE;
    dev->pp.start_mode = SND_PCM_START_FULL;
    dev->pp.stop_mode = SND_PCM_STOP_ROLLOVER;  //SND_PCM_STOP_STOP
    dev->pp.buf.block.frag_size = dev->pi.max_fragment_size;
    dev->pp.buf.block.frags_max = 1;
    dev->pp.buf.block.frags_min = 1;
    dev->pp.format.interleave = 1;
    dev->pp.format.rate = sample_rate;
    dev->pp.format.voices = sample_channels;
    dev->pp.format.format = format;

    strcpy(dev->pp.sw_mixer_subchn_name, "Wave playback channel");
    if( ( ret = snd_pcm_plugin_params( dev->pcm_handle, &dev->pp ) ) < 0) {
    	fprintf( stderr, "snd_pcm_plugin_params failed: %s\n", snd_strerror(ret));
         goto fail;
     }

    if ( ( ret = snd_pcm_plugin_prepare( dev->pcm_handle, SND_PCM_CHANNEL_CAPTURE ) ) < 0 ) {
    	fprintf( stderr, "snd_pcm_plugin_prepare failed: %s\n", snd_strerror(ret));
        goto fail;
    }

    memset(&dev->setup, 0, sizeof(dev->setup));
    memset(&dev->group, 0, sizeof(dev->group));
    dev->setup.channel = SND_PCM_CHANNEL_CAPTURE;
    dev->setup.mixer_gid = &dev->group.gid;

    if( ( ret = snd_pcm_plugin_setup( dev->pcm_handle, &dev->setup) ) < 0) {
    	fprintf( stderr, "snd_pcm_plugin_setup failed: %s\n", snd_strerror(ret) );
        goto fail;
    }

    if( dev->group.gid.name[0] == 0) {
    	fprintf( stderr, "Mixer Pcm Group [%s] Not Set \n", dev->group.gid.name);
        goto fail;
    }

    if( ( ret = snd_mixer_open(&dev->mixer_handle, dev->card, dev->setup.mixer_device) ) < 0) {
    	fprintf( stderr, "snd_mixer_open failed: %s\n", snd_strerror(ret));
        goto fail;
    }

    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );
    char strbuf[4096];
    fprintf( stderr, pcm_device_to_string( dev, strbuf, 4096 ) );

    fprintf( stderr, "setup.buf.block.frag_size = %d\n", dev->setup.buf.block.frag_size );

	/* Setup buffer */
	dev->block_size = dev->setup.buf.block.frag_size;
/*
	dev->sample_buffer = (char*) malloc(dev->block_size);
    if ( !dev->sample_buffer ) {
        fprintf( stderr, "malloc failed\n" );
        goto fail;
    }
*/

    return 0;

    fail:
    fprintf( stderr, "%s:%d Failed \n", __func__, __LINE__ );
    snd_pcm_close(dev->pcm_handle);

    return 0;
}

int AudioProc::setup_device( pcm_device_t *dev, int mode, int32_t channel )
{
    int ret;
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

    if ( ( ret = snd_pcm_open_preferred (&dev->pcm_handle, &dev->card, &dev->dev_hdl, mode ) ) < 0 )
    {
        fprintf( stderr, "%s:%d snd_pcm_open_preferred failed with code = %d (%s)\n", __func__, __LINE__, ret, strerror(-ret) );
        return -1;
    }

    if( ( ret = snd_pcm_info(dev->pcm_handle, &dev->info ) ) < 0) {
    	fprintf( stderr, "snd_pcm_info failed: %s\n", snd_strerror(ret));
        goto fail;
     }

    dev->card = dev->info.card;

    /* channel info */
    memset(&dev->pi, 0, sizeof(dev->pi) );
    dev->pi.channel = channel;
    if( ( ret = snd_pcm_plugin_info(dev->pcm_handle, &dev->pi ) ) < 0) {
    	fprintf( stderr, "snd_pcm_plugin_info failed: %s\n", snd_strerror(ret));
        goto fail;
    }

    /* channel params */
    memset(&dev->pp, 0, sizeof(dev->pp));
    dev->pp.mode = SND_PCM_MODE_BLOCK;
    dev->pp.channel = channel;
    dev->pp.start_mode = SND_PCM_START_FULL;
    dev->pp.stop_mode = SND_PCM_STOP_ROLLOVER; //SND_PCM_STOP_STOP
    dev->pp.buf.block.frag_size = dev->pi.max_fragment_size;

    dev->pp.buf.block.frags_max = 1;
    dev->pp.buf.block.frags_min = 1;

    dev->pp.format.interleave = 1;
    dev->pp.format.rate = sample_rate;
    dev->pp.format.voices = sample_channels;

    dev->pp.format.format = format;

    strcpy(dev->pp.sw_mixer_subchn_name, "Wave playback channel");
    if( ( ret = snd_pcm_plugin_params( dev->pcm_handle, &dev->pp ) ) < 0) {
    	fprintf( stderr, "snd_pcm_plugin_params failed: %s\n", snd_strerror(ret));
         goto fail;
     }

    if ( ( ret = snd_pcm_plugin_prepare( dev->pcm_handle, channel ) ) < 0 ) {
    	fprintf( stderr, "snd_pcm_plugin_prepare failed: %s\n", snd_strerror(ret));
        goto fail;
    }

    memset(&dev->setup, 0, sizeof(dev->setup));
    memset(&dev->group, 0, sizeof(dev->group));
    dev->setup.channel = channel;
    dev->setup.mixer_gid = &dev->group.gid;

    if( ( ret = snd_pcm_plugin_setup( dev->pcm_handle, &dev->setup) ) < 0) {
    	fprintf( stderr, "snd_pcm_plugin_setup failed: %s\n", snd_strerror(ret) );
        goto fail;
    }

    if( dev->group.gid.name[0] == 0) {
    	fprintf( stderr, "Mixer Pcm Group [%s] Not Set \n", dev->group.gid.name);
        goto fail;
    }

    if( ( ret = snd_mixer_open(&dev->mixer_handle, dev->card, dev->setup.mixer_device) ) < 0) {
    	fprintf( stderr, "snd_mixer_open failed: %s\n", snd_strerror(ret));
        goto fail;
    }

    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );
    char strbuf[4096];
    fprintf( stderr, pcm_device_to_string( dev, strbuf, 4096 ) );

    fprintf( stderr, "setup.buf.block.frag_size = %d\n", dev->setup.buf.block.frag_size );

	/* Setup buffer */
	dev->block_size = dev->setup.buf.block.frag_size;
	/*
    dev->sample_buffer = (char*) malloc(dev->block_size);

    if ( !dev->sample_buffer ) {
        fprintf( stderr, "malloc failed\n" );
        goto fail;
    }
    */
    return 0;

    fail:
    fprintf( stderr, "%s:%d Failed \n", __func__, __LINE__ );
    snd_pcm_close(dev->pcm_handle);
    return -1;
}

int AudioProc::restart_devices()
{
    fprintf( stderr, "%s:%d \n", __func__, __LINE__ );

	hold = 1;
	// sleep for a frame interval to allow playback and capture threads to suspend
	usleep(30000);

	snd_mixer_close(m_outdev.mixer_handle);
	snd_pcm_close(m_outdev.pcm_handle);
	snd_mixer_close(m_indev.mixer_handle);
	snd_pcm_close(m_indev.pcm_handle);
	setup_input_device();
	setup_ouput_device();

	usleep(30000);
	hold = 0;
}

char* AudioProc::pcm_device_to_string( pcm_device_t* dev, char* str, int len  )
{
	const int SIZE=1024;
    char tmp[SIZE];

    snprintf(str, len, "Format %s \n", snd_pcm_get_format_name(dev->setup.format.format) );

    snprintf(tmp, SIZE, "Frag Size %d \n", dev->setup.buf.block.frag_size);
    strlcat(str, tmp, len);

    snprintf(tmp, SIZE, "Total Frags %d \n", dev->setup.buf.block.frags);
    strlcat(str, tmp, len);

    snprintf(tmp, SIZE, "Rate %d \n", dev->setup.format.rate);
    strlcat(str, tmp, len);

    snprintf(tmp, SIZE, "Voices %d \n", dev->setup.format.voices);
    strlcat(str, tmp, len);

    snprintf(tmp, SIZE, "Mixer Pcm Group [%s]\n", dev->group.gid.name);
    strlcat(str, tmp, len);

    return str;
}

void AudioProc::generate_tone( char* block, int size, double freqHZ, double amplitude )
{
	int16_t *block16 = (int16_t*)block;
	double freqRad = freqHZ*2*3.14;
	double freqRadPerSample = freqRad/sample_rate ;

	for( int i = 0; i < size/2; i++ )
	{
		block16[i] = static_cast<int> ( amplitude * sin( phaseCarry + (double)i*freqRadPerSample) );
	}
	phaseCarry = phaseCarry + (double)size/2*freqRadPerSample;
	while(phaseCarry>6.28)
		phaseCarry -= 6.28;
}
void AudioProc::flatten( char* block, int size, double threshold, double scale )
{
	int32_t *block32 = (int32_t*)block;

	for( int i = 1; i < size/4; i++ )
	{
		int blockVal = static_cast<double>(block32[i]);
		if( blockVal > 0 )
		{
			block32[i] = static_cast<int32_t>( blockVal + (threshold-blockVal)*scale );
		} else {
			block32[i] = static_cast<int32_t>( blockVal + (-threshold-blockVal)*scale );
		}
	}
}

void AudioProc::compress( char* block, int size, double threshold, double scale )
{
	int32_t *block32 = (int32_t*)block;
	double avg = 0;
	double adjustment = 1;

	for( int i = 0; i < size/4; i++ )
	{
	    //fprintf( stderr, " %d ", abs(block32[i]) );
		avg += (((double) abs(block32[i]) ) /(double)(size/4));
	}
	if( avg > squelch_threshold ) {
		adjustment = threshold/avg;
		adjustment = (adjustment-1)*scale+1;
	} else {
		adjustment = 0;
	}

	for( int i = 0; i < size/4; i++ )
	{
		block32[i] = static_cast<int32_t>(block32[i]*adjustment);

		if( abs(block32[i]) > distortion_threshold ) {
			block32[i] = (block32[i]>0)?distortion_threshold:-distortion_threshold;
		}
	}
    //fprintf( stderr, "\n(%f %f %f) %d \n", adjustment, avg, threshold, size/4 );
}


BpsHandler::BpsHandler() {

}

BpsHandler::~BpsHandler() {
    // Close the BPS library
    bps_shutdown();
}
void BpsHandler::init( AudioProc *ap ) {
	parent_ap = ap;
	bps_initialize();
    subscribe(audiodevice_get_domain());
    audiodevice_request_events(0);
}
// Implement the event() function in your handler class
void BpsHandler::event(bps_event_t *event)
{
    // Handle events
    if( bps_event_get_domain(event) == audiodevice_get_domain() ) {
    	fprintf( stderr, "Audio BPS event\n");
    	parent_ap->restart_devices();
    }
}
