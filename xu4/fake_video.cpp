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

#include <Limelight.h>

#include "X11Window.h"
#include "Exception.h"
#include "Mfc.h"
#include "Scene.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <signal.h>

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

extern "C"
{
#include <X11/extensions/dri2.h>
}


std::shared_ptr<X11Window> window;
int drmfd = -1;
std::shared_ptr<Mfc> mfc;
std::shared_ptr<Scene> scene;
bool isStreaming = false;
bool isRunning = true;
std::thread thread;
std::mutex mutex;

typedef std::shared_ptr<std::vector<char>> VectorSPTR;
std::queue<VectorSPTR> freeBuffers;
std::queue<VectorSPTR> filledBuffers;


int OpenDRM(Display* dpy)
{
	int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0)
	{
		throw Exception("DRM device open failed.");
	}

	uint64_t hasDumb;
	if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &hasDumb) < 0 ||
		!hasDumb)
	{
		throw Exception("DRM device does not support dumb buffers");
	}


	// Obtain DRM authorization
	drm_magic_t magic;
	if (drmGetMagic(fd, &magic))
	{
		throw Exception("drmGetMagic failed");
	}

	Window root = RootWindow(dpy, DefaultScreen(dpy));
	if (!DRI2Authenticate(dpy, root, magic))
	{
		throw Exception("DRI2Authenticate failed");
	}


	return fd;
}

void Render()
{
	printf("Render: entered.\n");

	window = std::make_shared<X11Window>(1280, 720, "moonlight-embedded");
	printf("Render: window created.\n");

	drmfd = OpenDRM(window->X11Display());

	scene = std::make_shared<Scene>(window.get());
	printf("Render: scene created.\n");
	
	scene->Load();
	printf("Render: scene loaded.\n");

	mfc = std::make_shared<Mfc>();

	printf("Render: entering main loop.\n");
	while (isRunning)
	{
		window->ProcessMessages();

		mutex.lock();
		
		if (filledBuffers.size() > 0)
		{
			VectorSPTR buffer = filledBuffers.front();
			filledBuffers.pop();

			mfc->Enqueue(&buffer->operator[](0), buffer->size());

			freeBuffers.push(buffer);

			if (!isStreaming)
			{
				int captureWidth;
				int captureHeight;
				v4l2_crop crop;
				mfc->StreamOn(captureWidth, captureHeight, crop);

				printf("MFC: width=%d, height=%d\n", captureWidth, captureHeight);

				// Creat the rendering textures
				scene->SetTextureProperties(captureWidth, captureHeight,
				crop.c.left, crop.c.top,
				crop.c.width, crop.c.height);

				isStreaming = true;
			}
		}

		mutex.unlock();


		if (isStreaming && mfc->Dequeue(scene))
		{
			window->SwapBuffers();
		}


		std::this_thread::yield();
	}

	printf("Cleaning up MFC.\n");
	fflush(stdout);

	mfc->StreamOff();

	printf("Render: exited.\n");
	fflush(stdout);
}

// Signal handler for Ctrl-C
void SignalHandler(int s)
{
	if (mfc)
	{
		mfc->StreamOff();
	}
}

void decoder_renderer_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags)
{
	/*window = std::make_shared<X11Window>(width, height, "moonlight-embedded");*/
	/*drmfd = OpenDRM(window->X11Display());*/
	/*mfc = std::make_shared<Mfc>();*/
	
	//scene = std::make_shared<Scene>(window.get());
	//scene->Load();

	// Trap signal to clean up
	signal(SIGINT, SignalHandler);

	thread = std::thread(Render);
}

void decoder_renderer_cleanup()
{
	printf("decoder: terminating.\n");
	fflush(stdout);

	isRunning = false;
	thread.join();

	////close(drmfd);
	//printf("Cleaning up MFC.\n");
	//mfc->StreamOff();

	//mfc.reset();

	//delete scene;
	//delete mfc;
	//delete window;
}

int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit)
{
	mutex.lock();

	size_t totalBytes = 0;

	if (freeBuffers.size() < 1)
	{
		freeBuffers.push(std::make_shared<std::vector<char>>());
	}

	VectorSPTR buffer = freeBuffers.front();
	freeBuffers.pop();


	PLENTRY entry = decodeUnit->bufferList;
	while (entry != NULL)
	{
	//fwrite(entry->data, entry->length, 1, fd);
		
		//mfc->Enqueue(entry->data, entry->length);
		buffer->resize(totalBytes + entry->length);
		memcpy(&buffer->operator[](0) + totalBytes, entry->data, entry->length);

		totalBytes += entry->length;
		entry = entry->next;		
	}

	filledBuffers.push(buffer);

	mutex.unlock();

  //if (!isStreaming && scene)
  //{
	 // mutex.lock();

	 // int captureWidth;
	 // int captureHeight;
	 // v4l2_crop crop;
	 // mfc->StreamOn(captureWidth, captureHeight, crop);

	 // printf("MFC: width=%d, height=%d\n", captureWidth, captureHeight);

	 // // Creat the rendering textures
	 // scene->SetTextureProperties(captureWidth, captureHeight,
		//  crop.c.left, crop.c.top,
		//  crop.c.width, crop.c.height);

	 // isStreaming = true;

	 // mutex.unlock();
  //}

  //while (mfc->Dequeue(scene))
  //{
	 // window->SwapBuffers();
  //}

  //window->ProcessMessages();

	return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_fake = {
  .setup = decoder_renderer_setup,
  .cleanup = decoder_renderer_cleanup,
  .submitDecodeUnit = decoder_renderer_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(4),
};
