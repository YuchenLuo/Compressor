/*
 * AudioBufferPipe.cpp
 *
 *  Created on: Apr 7, 2013
 *      Author: luo
 */

#include "audioproc.h"

AudioBufferPipe::AudioBufferPipe() :
block_size(0),
buffer_length(0),
buffer_write_position(0),
buffer_read_position(0),
warp(0)
{

}

AudioBufferPipe::~AudioBufferPipe()
{
	free( sample_buffer_pool );
}

int AudioBufferPipe::init( int blockSize, int bufferLength, int pipes )
{
	block_size = blockSize;
	buffer_length = bufferLength;

	// init sample buffer
	sample_buffer = (AudioBufferElement**) malloc( buffer_length * sizeof(AudioBufferElement*) );
	if(!sample_buffer)
	{
	    fprintf( stderr, "%s:%d could not create buffer \n", __func__, __LINE__ );
	    return ENOMEM;
	}

	sample_buffer_pool = (char*) malloc( (block_size+1) * buffer_length );
	if(!sample_buffer_pool)
	{
	    fprintf( stderr, "%s:%d could not create buffer \n", __func__, __LINE__ );
	    goto fail;
	}
	for(int i = 0; i<buffer_length; i++ ) {
		sample_buffer[i] = (AudioBufferElement*)((int)sample_buffer_pool + i*(block_size+1));
	}
    fprintf( stderr, "AudioBufferPipe %s:%d  block_size %d, buffer_length %d \n", __func__, __LINE__, block_size, buffer_length );

    m_mutex = PTHREAD_MUTEX_INITIALIZER;

    // init pipes
	if( pipes < 2 )
		return EINVAL;

	m_numPipes = pipes;
	m_pipes = new AudioBufferPipeElement[m_numPipes];
	for( int i = 0; i < m_numPipes; i++ )
	{
		m_pipes[i].init( this, i );
		m_pipes[i].next = &m_pipes[(i+1)%m_numPipes];
	}
	m_pipes[m_numPipes-1].m_isHead = 1;
	return EOK;

	fail:
	free(sample_buffer);
	return ENOMEM;
}

/* Writes data to next available block, puts data on main Queue
 * buffer_write_position points last written block
 */
AudioBufferElement* AudioBufferPipe::get_write_buffer()
{
	//printTimestamp();
    //fprintf( stderr, "%s:%d w %d r %d wrap %d \n", __func__, __LINE__, buffer_write_position, buffer_read_position, warp );

	// Check whether next block is available
	if( warp && buffer_write_position+1 > buffer_read_position )
	{
	    fprintf( stderr, "%s:%d Buffer overflow \n", __func__, __LINE__ );
		return 0;
	}
	else
	{
		return sample_buffer[ (buffer_write_position + 1) % buffer_length ];
	}
	return 0;
}

void AudioBufferPipe::commit_write_buffer()
{
	if( buffer_write_position + 1 >= buffer_length )
	{
		buffer_write_position = 0;
		warp = 1;
	} else
	{
		buffer_write_position++;
	}
	//printTimestamp();
//    fprintf( stderr, "%s:%d w %d r %d wrap %d block length = %d\n", __func__, __LINE__,
//    		buffer_write_position, buffer_read_position, warp, sample_buffer[buffer_write_position]->length );
}

/*
 * buffer_read_position represents last read
 */
AudioBufferElement* AudioBufferPipe::get_read_buffer()
{
    //fprintf( stderr, "%s:%d w %d r %d wrap %d \n", __func__, __LINE__, buffer_write_position, buffer_read_position, warp );
	if( !warp && buffer_read_position+1 > buffer_write_position )
	{
	    fprintf( stderr, "%s:%d Buffer underflow \n", __func__, __LINE__ );
	    return 0;
	} else {
		return sample_buffer[ (buffer_read_position + 1) % buffer_length ];
	}

}

void AudioBufferPipe::free_read_buffer()
{
    //fprintf( stderr, "%s:%d w %d r %d wrap %d \n", __func__, __LINE__, buffer_write_position, buffer_read_position, warp );
	if( buffer_read_position + 1 >= buffer_length )
	{
		buffer_read_position = 0;
		warp = 0;
	} else
	{
		buffer_read_position++;
	}
}
unsigned int AudioBufferPipe::getTimeDelta(){
	struct timespec tp;
	static struct timespec prev_tp;

	clock_gettime( CLOCK_REALTIME, &tp );

	int sec = tp.tv_sec - prev_tp.tv_sec;
	int msec;
	if(sec == 0)
		msec = int((double)(tp.tv_nsec - prev_tp.tv_nsec) /1000000.0);
	else
		msec = int((double)(tp.tv_nsec + (1000000000*sec - prev_tp.tv_nsec) ) /1000000.0);

	prev_tp = tp;
	return msec;
}
void AudioBufferPipe::printTimeDelta()
{
	struct timespec tp;
	static struct timespec prev_tp;

	clock_gettime( CLOCK_REALTIME, &tp );

	int sec = tp.tv_sec - prev_tp.tv_sec;
	int msec;
	if(sec == 0)
		msec = int((double)(tp.tv_nsec - prev_tp.tv_nsec) /1000000.0);
	else
		msec = int((double)(tp.tv_nsec + (1000000000*sec - prev_tp.tv_nsec) ) /1000000.0);

	fprintf( stderr, "[%d]", msec );

	prev_tp = tp;
}

void AudioBufferPipe::printTimestamp()
{
	struct timespec tp;
	clock_gettime( CLOCK_REALTIME, &tp );
	fprintf( stderr, "[%d:%d]",tp.tv_sec, (int)(tp.tv_nsec/1000000.0) );
}



AudioBufferPipeElement::AudioBufferPipeElement(){
}
AudioBufferPipeElement::~AudioBufferPipeElement(){
}
int AudioBufferPipeElement::init( AudioBufferPipe *parent_pipe, int pipe_num ) {
	m_parent_pipe = parent_pipe;
	m_sample_buffer = m_parent_pipe->sample_buffer;
	m_buffer_length = m_parent_pipe->buffer_length;
	m_wrap = 0;
	m_pipeNumber = pipe_num;
	m_position = 0;
	m_isHead = 0;
	return EOK;
}

AudioBufferElement* AudioBufferPipeElement::get() {
	pthread_mutex_lock(&m_parent_pipe->m_mutex);

    //fprintf( stderr, "%s:%d pos %d wrap %d pipeNumber %d\n", __func__, __LINE__, m_position, m_wrap, m_pipeNumber );

    int wrap = m_isHead ? m_wrap != next->m_wrap: m_wrap == next->m_wrap;
	if( wrap && ( m_position+1 > next->m_position ) )
	{
	    //fprintf( stderr, "%s:%d (%d) overflow/underflow \n", __func__, __LINE__, m_pipeNumber );
	    pthread_mutex_unlock(&m_parent_pipe->m_mutex);
		return 0;
	} else
	{
		pthread_mutex_unlock(&m_parent_pipe->m_mutex);
		return m_sample_buffer[(m_position + 1)%m_buffer_length];
	}
}
void AudioBufferPipeElement::put(){
	pthread_mutex_lock(&m_parent_pipe->m_mutex);

    //fprintf( stderr, "%s:%d pos %d wrap %d pipeNumber %d\n", __func__, __LINE__, m_position, m_wrap, m_pipeNumber );

	if( m_position + 1 >= m_buffer_length )
	{
		m_wrap = !m_wrap;
		m_position = 0;
	} else
	{
		m_position++;
	}
    pthread_mutex_unlock(&m_parent_pipe->m_mutex);
}


