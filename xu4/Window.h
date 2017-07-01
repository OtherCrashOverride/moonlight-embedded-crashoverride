#pragma once

#include <EGL/egl.h>

enum class Action
{
	Nothing = 0,    //None is a #define
	Quit,
	ChangeScene

};


// Note: The name "Window" collides, so "WindowBase"
//       is used instead
class WindowBase
{

protected:
	WindowBase()
	{
	}

public:

	virtual EGLDisplay EglDisplay() const = 0;


	virtual ~WindowBase()
	{
	}


	virtual void SwapBuffers() = 0;
	virtual Action ProcessMessages() = 0;
	virtual void MakeCurrent() = 0;
	virtual void HideMouse() = 0;
	virtual void UnHideMouse() = 0;
};

