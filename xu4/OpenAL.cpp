/*
*
* Copyright (C) 2017 OtherCrashOverride
*
* Moonlight is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* Moonlight is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
*/

#include "../src/audio.h"

#include <stdlib.h>
#include <stdio.h>
#include <opus_multistream.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <queue>

#define MAX_CHANNEL_COUNT 6
#define FRAME_SIZE 240

static OpusMSDecoder* decoder;
static short pcmBuffer[FRAME_SIZE * MAX_CHANNEL_COUNT];
static ALCdevice *device;
static ALCcontext *context;
static ALuint source;
static std::queue<ALuint> freeBuffers;
static int channelCount;
static int sampleRate;

static void OpenAL_init(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig)
{
	channelCount = opusConfig->channelCount;
	sampleRate = opusConfig->sampleRate;

	printf("OpenAL: channels=%d, sampleRate=%d\n",
		channelCount, sampleRate);


	unsigned char openALMapping[MAX_CHANNEL_COUNT];

	// TODO: Validate OpenAL channel mapping

	/* The supplied mapping array has order: FL-FR-C-LFE-RL-RR
	* ALSA expects the order: FL-FR-RL-RR-C-LFE
	* We need copy the mapping locally and swap the channels around.
	*/
	openALMapping[0] = opusConfig->mapping[0];
	openALMapping[1] = opusConfig->mapping[1];
	if (opusConfig->channelCount == 6) 
	{
		openALMapping[2] = opusConfig->mapping[4];
		openALMapping[3] = opusConfig->mapping[5];
		openALMapping[4] = opusConfig->mapping[2];
		openALMapping[5] = opusConfig->mapping[3];
	}

	int error;
	decoder = opus_multistream_decoder_create(opusConfig->sampleRate,
		opusConfig->channelCount,
		opusConfig->streams,
		opusConfig->coupledStreams,
		openALMapping,
		&error);

	// TODO check opus error

	device = alcOpenDevice(NULL);
	if (!device)
	{
		printf("OpenAL: alcOpenDevice failed.\n");
		exit(1);
	}

	context = alcCreateContext(device, NULL);
	if (!alcMakeContextCurrent(context))
	{
		printf("OpenAL: alcMakeContextCurrent failed.\n");
		exit(1);
	}

	alGenSources((ALuint)1, &source);

	alSourcef(source, AL_PITCH, 1);
	alSourcef(source, AL_GAIN, 1);
	alSource3f(source, AL_POSITION, 0, 0, 0);
	alSource3f(source, AL_VELOCITY, 0, 0, 0);
	alSourcei(source, AL_LOOPING, AL_FALSE);

	const int BUFFER_COUNT = 20;
	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		ALuint buffer;
		alGenBuffers((ALuint)1, &buffer);

		freeBuffers.push(buffer);
	}
}

static void OpenAL_cleanup()
{
	if (decoder != NULL)
	{
		opus_multistream_decoder_destroy(decoder);
	}

	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);
}

static void OpenAL_decode_and_play_sample(char* data, int length)
{
	// Pull completed buffers off the OpenAL queue
	ALint buffersProcessed;
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &buffersProcessed);

	for (int i = 0; i < buffersProcessed; ++i)
	{		
		ALuint openALBufferID;
		alSourceUnqueueBuffers(source, 1, &openALBufferID);

		freeBuffers.push(openALBufferID);
	}


	// Always decode Opus stream to avoid errors
	int decodeLen = opus_multistream_decode(decoder,
		(unsigned char*)data,
		length,
		pcmBuffer,
		FRAME_SIZE,
		0);


	// Process audio
	if (freeBuffers.size() < 1)
	{
		// If there are no free buffers, drop the audio.
		printf("OpenAL: No free buffers.\n");
	}
	else
	{
		if (decodeLen > 0)
		{
			// Get a buffer
			ALuint openALBufferID = freeBuffers.front();
			freeBuffers.pop();

			ALuint format;
			switch (channelCount)
			{
			case 2:
				format = AL_FORMAT_STEREO16;
				break;

			case 6:
				format = AL_FORMAT_51CHN16;
				break;

			default:
				printf("OpenAL: channelCount (%d) not supported.\n", channelCount);
				exit(1);
			}

			int dataByteLength = decodeLen * channelCount * sizeof(short);
			alBufferData(openALBufferID, format, pcmBuffer, dataByteLength, sampleRate);

			alSourceQueueBuffers(source, 1, &openALBufferID);

			ALint result;
			alGetSourcei(source, AL_SOURCE_STATE, &result);

			if (result != AL_PLAYING)
			{
				alSourcePlay(source);
			}
		}
	}
}

AUDIO_RENDERER_CALLBACKS audio_callbacks_alsa = {
	.init = OpenAL_init,
	.cleanup = OpenAL_cleanup,
	.decodeAndPlaySample = OpenAL_decode_and_play_sample,
	.capabilities = CAPABILITY_DIRECT_SUBMIT,
};