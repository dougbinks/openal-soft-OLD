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
#include "config.h" // for alMain.h, remove when move extension to alext.h
#include "alMain.h" // move to alext.h when fully approved


static LPALGETSOURCEI64VSOFT   alGetSourcei64vSOFT;
static LPALGETSOURCEI64VSOFT   alSourcei64SOFT;
static LPALSOURCEPLAYTIMESOFTX alSourcePlayTimeSOFTX;
static LPALCGENINTEGER64VSOFTX alcGetInteger64vSOFTX;
static LPALCGENDOUBLEVSOFTX    alcGetDoublevSOFTX;

#define WAVEHALFPERIOD 129
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
    ALint64SOFT clockInfo[3];
    ALint64SOFT clockInfo2nd[3];
    ALint64SOFT clockToPlay;
    ALenum state;
    ALenum error;
    ALCint deviceFreq;
    ALuint Buffer;
    ALuint Sources[2];
    float  BuffSource[BUFFSIZE];
    int PlayingSecond;
    ALint64SOFT alignError;
    const ALint64SOFT halfPeriod =  (ALint64SOFT)WAVEHALFPERIOD << 32;
    double frequency;


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

    if(!alcIsExtensionPresent(pALDevice, "ALC_SOFTX_device_clock"))
    {
        fprintf(stderr, "Error: AL_SOFTX_device_clock not supported\n");
        CloseAL();
        return 1;
    }

    /* Define a macro to help load the function pointers. */
#define LOAD_PROC(x)  ((x) = alGetProcAddress(#x))
    LOAD_PROC(alGetSourcei64vSOFT);
    LOAD_PROC(alSourcei64SOFT);
    LOAD_PROC(alSourcePlayTimeSOFTX);
#undef LOAD_PROC
    alcGetInteger64vSOFTX = alcGetProcAddress(pALDevice, "alcGetInteger64vSOFTX");
    alcGetDoublevSOFTX    = alcGetProcAddress(pALDevice, "alcGetDoublevSOFTX");

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
    
    alcGetIntegerv( pALDevice, ALC_FREQUENCY, 1, &deviceFreq );
    frequency = 44100;
    alBufferData( Buffer, AL_FORMAT_MONO_FLOAT32, BuffSource, sizeof( BuffSource ), (ALsizei)frequency );
    alcGetDoublevSOFTX( pALDevice, ALC_DEVICE_CONVERT_ACTUALFREQUENCY, 1, &frequency );
    for( int b = 0; b < 2; ++b )
    {
        alSourcei(Sources[b], AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcei(Sources[b], AL_LOOPING, AL_FALSE);
        alSourcei(Sources[b], AL_BUFFER, Buffer);
    }


    /* Play the sound until it finishes. */
    alSourcePlay(Sources[0]);
    PlayingSecond = 0;
    printf("\rOutputSampleCount 1, Sample Offset 1, OutputSampleCount 2, Sample Offset 2, alignment error\n" );
    do {
        Sleep(10);
        alGetSourcei(Sources[0], AL_SOURCE_STATE, &state);

        alGetSourcei64vSOFT(Sources[0], AL_SAMPLE_OFFSET_LATENCY_DEVICE_CLOCK_SOFTX, clockInfo);
        alGetSourcei64vSOFT(Sources[1], AL_SAMPLE_OFFSET_LATENCY_DEVICE_CLOCK_SOFTX, clockInfo2nd);
        if( (clockInfo[1] >> 32) + 2*WAVEHALFPERIOD > BUFFSIZE / 4 && !PlayingSecond )
        {
            // want to play at a point where the second wave cancels first
            double interval;
            clockToPlay =  51 * halfPeriod - ( clockInfo[1] % (2 * halfPeriod) );
            interval = (double)clockToPlay * (double)DEVCLK_TIMEVALS_PERSECOND /  ((ALint64SOFT)1 << 32) / frequency;
            clockToPlay = clockInfo[0] + (ALint64SOFT)interval;
            alSourcePlayTimeSOFTX(clockToPlay, Sources[1]);
            PlayingSecond = 1;
        }
        printf("%lld, %lld, %lld, %lld\n", clockInfo[0], clockInfo[1], clockInfo2nd[0], clockInfo2nd[1] );
        fflush(stdout);
        error = alGetError();

    } while(error == AL_NO_ERROR && state == AL_PLAYING);
    printf("\n");

    // check device sample output count without source
    alcGetInteger64vSOFTX( pALDevice, ALC_DEVICE_CLOCK_SOFTX, 1, clockInfo );
    printf("Total samples output: %lld\n", clockInfo[0]);


    /* All done. Delete resources, and close OpenAL. */
    alDeleteSources(2, &Sources[0]);
    alDeleteBuffers(1, &Buffer);

    CloseAL();

    return 0;
}
