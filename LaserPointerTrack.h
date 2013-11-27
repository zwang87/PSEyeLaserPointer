#pragma once
#include <conio.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <math.h>

#include "CLEyeMulticam.h"
#include "opencv.hpp"
//#include <opencv\cv.h>
//#include <opencv\highgui.h>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\imgproc\imgproc.hpp>
//#include <opencv2\opencv.hpp>
//C:\Program Files (x86)\Code Laboratories\CL-Eye Platform SDK\Include\opencv2
using namespace cv;
using namespace std;

static LARGE_INTEGER _frequencyPT;

static inline PVOID ProfileMSStart()
{
	PLARGE_INTEGER start = new LARGE_INTEGER;
	QueryPerformanceFrequency(&_frequencyPT);
	QueryPerformanceCounter(start);
	return (PVOID)start;
}

static inline double ProfileMSEnd(PVOID p)
{
	LARGE_INTEGER stop, diff;
	PLARGE_INTEGER start = (PLARGE_INTEGER)p;
	QueryPerformanceCounter(&stop);
	diff.QuadPart = stop.QuadPart - start->QuadPart;
	double timeMs = 1000.0 * ((double)diff.QuadPart / (double)_frequencyPT.QuadPart);
	delete start;
	return timeMs;
}


//camera capture class
class CLEyeCameraCapture
{
	CHAR _windowName[256];
	GUID _cameraGUID;
	CLEyeCameraInstance _cam;
	CLEyeCameraColorMode _mode;
	CLEyeCameraResolution _resolution;
	char* trackingWindowName;
	float _fps;
	HANDLE _hThread;
	bool _running;
	bool _isTracking;
	bool _isCalibrated;
	Point squareCorner[4];

public:

	//CLEyeCameraCapture();
	//~CLEyeCameraCapture();

	CLEyeCameraCapture(LPSTR windowName, GUID cameraGUID, CLEyeCameraColorMode mode, CLEyeCameraResolution resolution, float fps);

	bool StartCapture();

	void StopCapture();

	void IncrementCameraParameter(int param);

	void DecrementCameraParameter(int param);

	void Run();

	void trackLaserSpot(Mat& pCapture, int& p_x, int& p_y, int& countPixel, int& grayThreshold);

	Mat warpPerspectiveTransform(Point2f corners[4]);

	void calibrate(char* winName, Mat& input);

	Mat GetCaptureImage();

	static DWORD WINAPI CaptureThread(LPVOID instance)
	{
		// seed the rng with current tick count and thread id
		srand(GetTickCount() + GetCurrentThreadId());
		// forward thread to Capture function
		CLEyeCameraCapture *pThis = (CLEyeCameraCapture *)instance;
		pThis->Run();
		return 0;
	}

private:
	IplImage* pCapImage;
	Mat pCapture;
	int w, h;
	PBYTE pCapBuffer;
};

