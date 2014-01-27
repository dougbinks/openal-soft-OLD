/*
 * OpenAL Source Device Clock Example
 *
 * Copyright (c) 2014 by Doug Binks & Chris Robinson <chris.kcat@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* This file contains an example for synchronising two sounds. */
/* It should play a square wave followed by silence, */
/* as a second square wave is played inverted on top. */

#include <stdio.h>
#include <assert.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"



static LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;
static LPALGETSOURCEI64VSOFT alSourcei64SOFT;

#define WAVEHALFPERIOD 256
#define BUFFSIZE       102400

static ALCdevice*          pALDevice;
static ALCcontext*         pALContext;


void CloseAL(void)
{
    alcMakeContextCurrent( 0 );
    alcDestroyContext( pALContext );
    alcCloseDevice( pALDevice );
}

int OpenAL(void)
{
    pALDevice = alcOpenDevice(NULL);
    if( !pALDevice )
    {
		fprintf(stderr, "Cannot create OpenAL device\n" );
        return 1;
    }

    pALContext = alcCreateContext(pALDevice, NULL);
    if( !pALContext )
    {
		fprintf(stderr, "Cannot create OpenAL context\n" );
        CloseAL();
        return 1;
    }
    alcMakeContextCurrent(pALContext);

    return 0;
}


int main(int argc, char **argv)
{
    ALint64SOFT clockInfo[4];
    ALint64SOFT clockInfo2nd[4];
    ALint64SOFT clockToPlay;
    ALenum state;
    ALenum state2nd;
    ALenum error;
    ALCint freq;
    ALuint Buffer;
    ALuint Sources[2];
    float  BuffSource[BUFFSIZE];
    int PlayingSecond;



    /* Initialize OpenAL with the default device. */
    if( OpenAL() != 0 )
    {
        return 1;
    }

    alGenBuffers(1, &Buffer);
    alGenSources(2, Sources );
    if(alGetError() != AL_NO_ERROR)
    {
		fprintf(stderr, "Error creating openal source/buffers\n" );
        CloseAL();
        return 1;
    }
    // we want 32bit float buffers
    if( !alIsExtensionPresent( "AL_EXT_FLOAT32" ) )
    {
 		fprintf(stderr, "Error AL_EXT_FLOAT32 not supported\n" );
        CloseAL();
        return 1;
    }

    if(!alIsExtensionPresent("AL_SOFT_device_clock"))
    {
        fprintf(stderr, "Error: AL_SOFT_device_clock not supported\n");
        CloseAL();
        return 1;
    }

    /* Define a macro to help load the function pointers. */
#define LOAD_PROC(x)  ((x) = alGetProcAddress(#x))
    LOAD_PROC(alGetSourcei64vSOFT);
    LOAD_PROC(alSourcei64SOFT);
#undef LOAD_PROC

    /* Load the sound into a buffer. */
    for(int i = 0; i < BUFFSIZE; ++i )
    {
        if( ( i / WAVEHALFPERIOD ) % 2 == 0 )
        {
            BuffSource[i] = 1.0f;
        }
        else
        {
            BuffSource[i] = -1.0f;
        }
    }
    
    alcGetIntegerv( pALDevice, ALC_FREQUENCY, 1, &freq );
    alBufferData( Buffer, AL_FORMAT_MONO_FLOAT32, BuffSource, sizeof( BuffSource ), freq );
    for( int b = 0; b < 2; ++b )
    {
        alSourcei(Sources[b], AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcei(Sources[b], AL_LOOPING, AL_FALSE);
        alSourcei(Sources[b], AL_BUFFER, Buffer);
    }


    /* Play the sound until it finishes. */
    alSourcePlay(Sources[0]);
    PlayingSecond = 0;
    printf("\rSource, Offset, OutputSampleCount, Frequency, UpdateSize\n" );
    do {
        Sleep(10);
        alGetSourcei(Sources[0], AL_SOURCE_STATE, &state);

        alGetSourcei64vSOFT(Sources[0], AL_SAMPLE_OFFSET_DEVICE_CLOCK_SOFT, clockInfo);
        alGetSourcei64vSOFT(Sources[1], AL_SAMPLE_OFFSET_DEVICE_CLOCK_SOFT, clockInfo2nd);
        if( clockInfo[0] + 2*WAVEHALFPERIOD > BUFFSIZE / 4 && !PlayingSecond )
        {
            // want to play at a point where the second wave cancels first
            clockToPlay = clockInfo[1] - ( clockInfo[0] % (2 * WAVEHALFPERIOD) ) + ( 5 * WAVEHALFPERIOD );
            alSourcei64SOFT( Sources[1], AL_PLAY_ON_DEVICE_CLOCK_SOFT, clockToPlay );
            alSourcePlay(Sources[1]);
            PlayingSecond = 1;
        }
        printf("0, %ld, %ld, %ld, %ld\n", clockInfo[0], clockInfo[1], clockInfo[2], clockInfo[3] );
        printf("1, %ld, %ld, %ld, %ld\n", clockInfo2nd[0], clockInfo2nd[1], clockInfo2nd[2], clockInfo2nd[3] );
        fflush(stdout);
        error = alGetError();

    } while(error == AL_NO_ERROR && state == AL_PLAYING);
    printf("\n");

    /* All done. Delete resources, and close OpenAL. */
    alDeleteSources(2, &Sources);
    alDeleteBuffers(1, &Buffer);

    CloseAL();

    return 0;
}
