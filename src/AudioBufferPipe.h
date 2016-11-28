/*
 * AudioBufferPipe.h
 *
 *  Created on: 2013-04-12
 *      Author: luo
 */

#ifndef AUDIOBUFFERPIPE_H_
#define AUDIOBUFFERPIPE_H_

class AudioBufferPipeElement;


typedef struct {
	int length;
	char buffer[0];
} AudioBufferElement;

class AudioBufferPipe
{
public:
    AudioBufferPipe();
    ~AudioBufferPipe();

    int init( int block_size, int buffer_length, int pipes );

    pthread_mutex_t m_mutex;


    int    block_size;
    int    buffer_length;
    char  *sample_buffer_pool;
    AudioBufferElement **sample_buffer;

    int   buffer_write_position;
    int   buffer_read_position;
    int   warp;

    int m_numPipes;
    AudioBufferPipeElement *m_pipes;

    AudioBufferElement* get_write_buffer();
    void commit_write_buffer();

    AudioBufferElement* get_read_buffer();
    void free_read_buffer();

    void printTimeDelta();
    unsigned int getTimeDelta();
    void printTimestamp();
};

class AudioBufferPipeElement {
public:
	AudioBufferPipeElement();
	~AudioBufferPipeElement();

	int m_pipeNumber;
	AudioBufferPipe 	*m_parent_pipe;
    int 				m_buffer_length;
    AudioBufferElement **m_sample_buffer;
	AudioBufferPipeElement* next;

	// position is last completed buffer
    int m_position;
    int m_wrap;
    int m_isHead;


    /* Gets next available element for processing
     */
    int init( AudioBufferPipe *parent_pipe, int pipe_num );
    AudioBufferElement* get();
    void put();
};


#endif /* AUDIOBUFFERPIPE_H_ */
