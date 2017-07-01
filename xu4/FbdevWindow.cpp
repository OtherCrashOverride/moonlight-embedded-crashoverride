#include "FbdevWindow.h"

#include "Egl.h"
#include "Exception.h"
#include "OpenGL.h"
#include <stdio.h>
#include <memory>
#include <GLES2/gl2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>

FbdevWindow::FbdevWindow(int width, int height, const char* title)
{
    if (width < 1 || height < 1)
        throw Exception("Invalid window size.");


    this->width = width;
    this->height = height;

    // Get the EGLDisplay
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    Egl::CheckError();


    // Intialize EGL for use on the display
    Egl::Initialize(eglDisplay);


    // Find a config that matches what we want
    EGLConfig config = Egl::PickConfig(eglDisplay);


	// Get the display size
	int fd = open("/dev/fb0", O_RDWR);
	
	fb_var_screeninfo var_info;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &var_info) < 0)
	{
		throw Exception("FBIOGET_VSCREENINFO failed.");
	}

	window.width = var_info.xres;
	window.height = var_info.yres;

	close(fd);


    // Create the rendering surface
    EGLint windowAttr[] =
    {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };

    surface = eglCreateWindowSurface(eglDisplay, config, (EGLNativeWindowType)&window, windowAttr);
    Egl::CheckError();

    if (surface == EGL_NO_SURFACE)
        throw Exception("eglCreateWindowSurface failed.");


    // Create a GLES2 context
    EGLint contextAttribs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs );
    if ( context == EGL_NO_CONTEXT )
    {
        throw Exception("eglCreateContext failed.");
    }


    // Make the context current
    if (!eglMakeCurrent(eglDisplay, surface, surface, context) )
    {
        throw Exception("eglMakeCurrent failed.");
    }
}

FbdevWindow::~FbdevWindow()
{
    // Release the context
    if (!eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) )
    {
        throw Exception("eglMakeCurrent failed.");
    }

    // Destory the context
    if (!eglDestroyContext(eglDisplay, context))
    {
        throw Exception ("eglDestroyContext failed.");
    }

    // Destroy the surface
    if (!eglDestroySurface(eglDisplay, surface))
    {
        throw Exception ("eglDestroySurface failed.");
    }
}


void FbdevWindow::SwapBuffers()
{
    eglSwapBuffers(eglDisplay, surface);
    Egl::CheckError();
}

Action FbdevWindow::ProcessMessages()
{
    Action result = Action::Nothing;

    return result;
}

void FbdevWindow::MakeCurrent()
{
	// Make the context current
	if (!eglMakeCurrent(eglDisplay, surface, surface, context))
	{
		throw Exception("eglMakeCurrent failed.");
	}
}

void FbdevWindow::HideMouse()
{
	// Do nothing
}

void FbdevWindow::UnHideMouse()
{
	// Do nothing
}