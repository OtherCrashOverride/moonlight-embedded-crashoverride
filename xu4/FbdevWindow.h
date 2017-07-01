#pragma once

#include "Window.h"

#include <EGL/egl.h>




typedef struct fbdev_window
{
	unsigned short width;
	unsigned short height;
} fbdev_window;


class FbdevWindow : public virtual WindowBase
{
    int width = 0;
    int height = 0;
    EGLDisplay eglDisplay = 0;
    EGLSurface surface = 0;
    EGLContext context = 0;
	fbdev_window window;


public:

	EGLDisplay EglDisplay() const override
	{
		return eglDisplay;
	}


	FbdevWindow(int width, int height, const char* title);
    virtual ~FbdevWindow();


	void SwapBuffers() override;
	Action ProcessMessages() override;
	void MakeCurrent() override;
	void HideMouse() override;
	void UnHideMouse() override;
};

