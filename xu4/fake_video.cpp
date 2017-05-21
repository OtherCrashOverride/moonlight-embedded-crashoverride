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
#include <poll.h>

#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <chrono>

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
std::thread feedThread;

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

bool isFrameAvailable = false;
unsigned int frameIndex;
int frameY = 0;
int frameUV = 0;
std::mutex frameMutex;
std::thread decodeThread;

void Decode()
{
	printf("Decode thread started.\n");

	while (isRunning)
	{

		
		unsigned int i;
		int y;
		int uv;
		
		if (mfc->Dequeue(&i, &y, &uv))
		{
			frameMutex.lock();

			isFrameAvailable = true;
			frameIndex = i;
			frameY = y;
			frameUV = uv;

			//// flush any backed up buffers
			//while(mfc->Dequeue(&i, &y, &uv))
			//{
			//	mfc->DequeueDone(i);
			//}

			frameMutex.unlock();
		}
		

		//if (!temp)
		//{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			//std::this_thread::yield();
		//}
	}

	printf("Decode thread exited.\n");
}

void FeedMfc()
{
	while (isRunning)
	{
		mutex.lock();

		while (filledBuffers.size() > 0 &&
			mfc->CheckEnqueueBuffersAvailable())
		{
			//printf("filledBuffers=%d, freeBuffers=%d\n", filledBuffers.size(), mfc->FreeBufferCount());

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

		//if (mfc->CheckEnqueueBuffersAvailable())
		{
			// MFC has buffers, waiting for network to deliver data.

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		//		else
		//		{
		//			// MFC buffers are all used, wait for a free one.
		//
		//			pollfd fds = { 0 };
		//			fds.fd = mfc->FileDescriptor();
		//			fds.events = POLLOUT; //POLLOUT | POLLWRNORM; //POLLIN | POLLRDNORM 
		//
		//			int ret = poll(&fds, 1, 500); //500ms
		//			if (ret < 0)
		//			{
		//				printf("poll: error=%d\n", errno);
		//			}
		//			else if (ret == 0)
		//			{
		//				printf("poll: timeout\n");
		//			}
		//			else if (ret > 0)
		//			{
		//#if 0
		//				/* An event on one of the fds has occurred. */
		//				printf("poll: ");
		//
		//				if (fds.revents & POLLWRNORM) {
		//					printf("POLLWRNORM ");
		//				}
		//				if (fds.revents & POLLOUT) {
		//					printf("POLLOUT ");
		//				}
		//				if (fds.revents & POLLIN) {
		//					printf("POLLIN ");
		//				}
		//				//if (fds.revents & POLLHUP) {
		//				//	printf("POLLHUP ");
		//				//}
		//				if (fds.revents & POLLPRI) {
		//					printf("POLLPRI ");
		//				}
		//				printf("\n");
		//#endif
		//			}
		//		}
	}
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

	//mfc = std::make_shared<Mfc>();
	feedThread = std::thread(FeedMfc);
	decodeThread = std::thread(Decode);

	printf("Render: entering main loop.\n");

	pollfd fds;
	fds.fd = mfc->FileDescriptor();
	fds.events = POLLOUT; //POLLOUT | POLLWRBAND; //POLLIN | POLLRDNORM 

	while (isRunning)
	{
		window->ProcessMessages();

		//mutex.lock();
		//
		//while (filledBuffers.size() > 0 && 
		//	   mfc->CheckEnqueueBuffersAvailable())
		//{
		//	//printf("filledBuffers=%d, freeBuffers=%d\n", filledBuffers.size(), mfc->FreeBufferCount());

		//	VectorSPTR buffer = filledBuffers.front();
		//	filledBuffers.pop();

		//	mfc->Enqueue(&buffer->operator[](0), buffer->size());

		//	freeBuffers.push(buffer);

		//	if (!isStreaming)
		//	{
		//		int captureWidth;
		//		int captureHeight;
		//		v4l2_crop crop;
		//		mfc->StreamOn(captureWidth, captureHeight, crop);

		//		printf("MFC: width=%d, height=%d\n", captureWidth, captureHeight);

		//		// Creat the rendering textures
		//		scene->SetTextureProperties(captureWidth, captureHeight,
		//		crop.c.left, crop.c.top,
		//		crop.c.width, crop.c.height);

		//		isStreaming = true;
		//	}
		//}

		//mutex.unlock();

		//bool frame = false;
		//while (isStreaming)
		//{
		//	bool tmp = mfc->Dequeue(scene);
		//	frame |= tmp;

		//	if (!tmp)
		//		break;
		//}
		
		frameMutex.lock();
		
		if (isFrameAvailable)
		{
			//printf("Render: frame.\n");

			scene->Draw(frameY, frameUV);


			mfc->DequeueDone(frameIndex);

			isFrameAvailable = false;
			frameMutex.unlock();

#if 1
			// Wait for GPU
			glFinish();

			// Wait for VSYNC
			drmVBlank vbl =
			{
				.request =
				{
					.type = DRM_VBLANK_RELATIVE,
					.sequence = 1,
				}
			};

			int io = drmWaitVBlank(drmfd, &vbl);
			if (io)
			{
				throw Exception("drmWaitVBlank failed.");
			}
#endif

			window->SwapBuffers();
		}
		else
		{
			frameMutex.unlock();

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		//std::this_thread::yield();

//		int ret = poll(&fds, 1, 500); //500ms
//		if (ret > 0)
//		{
//#if 0
//			/* An event on one of the fds has occurred. */
//			printf("poll: ");
//
//			if (fds.revents & POLLWRBAND) {
//				/* Priority data may be written on device number i. */
//				printf("POLLWRBAND ");
//			}
//			if (fds.revents & POLLOUT) {
//				/* Data may be written on device number i. */
//				printf("POLLOUT ");
//			}
//			if (fds.revents & POLLHUP) {
//				/* A hangup has occurred on device number i. */
//				printf("POLLHUP ");
//			}
//			printf("\n");
//#endif
//		}
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
	mfc = std::make_shared<Mfc>();
	
	//scene = std::make_shared<Scene>(window.get());
	//scene->Load();

	// Trap signal to clean up
	//signal(SIGINT, SignalHandler);

	thread = std::thread(Render);
	//feedThread = std::thread(FeedMfc);
	//decodeThread = std::thread(Decode);
}

void decoder_renderer_cleanup()
{
	printf("decoder: terminating.\n");
	fflush(stdout);

	isRunning = false;
	thread.join();
	feedThread.join();
	decodeThread.join();

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


	////while (filledBuffers.size() > 0 &&
	////	mfc->CheckEnqueueBuffersAvailable())
	//{
	//	//printf("filledBuffers=%d, freeBuffers=%d\n", filledBuffers.size(), mfc->FreeBufferCount());

	//	//VectorSPTR buffer = filledBuffers.front();
	//	//filledBuffers.pop();

	//	mfc->Enqueue(&buffer->operator[](0), buffer->size());

	//	freeBuffers.push(buffer);

	//	if (!isStreaming)
	//	{
	//		int captureWidth;
	//		int captureHeight;
	//		v4l2_crop crop;
	//		mfc->StreamOn(captureWidth, captureHeight, crop);

	//		printf("MFC: width=%d, height=%d\n", captureWidth, captureHeight);

	//		// Creat the rendering textures
	//		scene->SetTextureProperties(captureWidth, captureHeight,
	//			crop.c.left, crop.c.top,
	//			crop.c.width, crop.c.height);

	//		isStreaming = true;
	//	}
	//}


	return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_fake = {
  .setup = decoder_renderer_setup,
  .cleanup = decoder_renderer_cleanup,
  .submitDecodeUnit = decoder_renderer_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(4),
};
