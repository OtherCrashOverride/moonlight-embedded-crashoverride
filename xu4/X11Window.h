#pragma once

#include "Window.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <EGL/egl.h>



class X11Window : public virtual WindowBase
{
    int width = 0;
    int height = 0;
    Display* x11Display = 0;
    Window xwin = 0;
    Colormap colormap = 0;
    EGLDisplay eglDisplay = 0;
    EGLSurface surface = 0;
    EGLContext context = 0;
    Atom wm_delete_window;

public:

	Display* X11Display() const 
	{
		return x11Display;
	}

	EGLDisplay EglDisplay() const override
	{
		return eglDisplay;
	}


    X11Window(int width, int height, const char* title);
    virtual ~X11Window();


    void SwapBuffers() override;
    Action ProcessMessages() override;
	void MakeCurrent() override;
	void HideMouse() override;
	void UnHideMouse() override;
};

