#include "LaserPointerTrack.h"
Point2f* corners = new Point2f[4];
int mouseClicked = 0;

Point2f pos;

CLEyeCameraCapture::CLEyeCameraCapture(LPSTR windowName, GUID cameraGUID, CLEyeCameraColorMode mode, CLEyeCameraResolution resolution, float fps) :
_cameraGUID(cameraGUID), _cam(NULL), _mode(mode), _resolution(resolution), _fps(fps), _running(false)
{
	strcpy(_windowName, windowName);
	pCapBuffer = NULL;
	_isTracking = false;
	_isCalibrated = false;
	trackingWindowName = "testWindow";
}
bool CLEyeCameraCapture::StartCapture()
{
	_running = true;
	namedWindow(_windowName, 0);
	namedWindow(trackingWindowName, CV_WINDOW_AUTOSIZE);
	// Start CLEye image capture thread
	_hThread = CreateThread(NULL, 0, &CLEyeCameraCapture::CaptureThread, this, 0, 0);
	if(_hThread == NULL)
	{
		//MessageBox(NULL,"Could not create capture thread","CLEyeMulticamTest", MB_ICONEXCLAMATION);
		return false;
	}
	return true;
}
void CLEyeCameraCapture::StopCapture()
{
	if(!_running)	return;
	_running = false;
	WaitForSingleObject(_hThread, 1000);
	destroyAllWindows();
}
void CLEyeCameraCapture::IncrementCameraParameter(int param)
{
	if(!_cam)	return;
	cout << "CLEyeGetCameraParameter " << CLEyeGetCameraParameter(_cam, (CLEyeCameraParameter)param) << endl;
	CLEyeSetCameraParameter(_cam, (CLEyeCameraParameter)param, CLEyeGetCameraParameter(_cam, (CLEyeCameraParameter)param)+10);
}
void CLEyeCameraCapture::DecrementCameraParameter(int param)
{
	if(!_cam)	return;
	cout << "CLEyeGetCameraParameter " << CLEyeGetCameraParameter(_cam, (CLEyeCameraParameter)param) << endl;
	CLEyeSetCameraParameter(_cam, (CLEyeCameraParameter)param, CLEyeGetCameraParameter(_cam, (CLEyeCameraParameter)param)-10);
}
void CLEyeCameraCapture::Run()
{	
	// Create camera instance
	_cam = CLEyeCreateCamera(_cameraGUID, _mode, _resolution, _fps);
	if(_cam == NULL)		return;
	// Get camera frame dimensions
	CLEyeCameraGetFrameDimensions(_cam, w, h);
	// Depending on color mode chosen, create the appropriate OpenCV image
	if(_mode == CLEYE_COLOR_PROCESSED || _mode == CLEYE_COLOR_RAW)
		pCapImage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 4);
	else
		pCapImage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);

	// Set some camera parameters
	CLEyeSetCameraParameter(_cam, CLEYE_GAIN, 45);
	CLEyeSetCameraParameter(_cam, CLEYE_EXPOSURE, 511);
	//CLEyeSetCameraParameter(_cam, CLEYE_AUTO_GAIN, true);
	//CLEyeSetCameraParameter(_cam, CLEYE_AUTO_EXPOSURE, true);
	CLEyeSetCameraParameter( _cam, CLEYE_HFLIP, false);

	// Start capturing
	CLEyeCameraStart(_cam);
	cvGetImageRawData(pCapImage, &pCapBuffer);
	pCapture = pCapImage;
	long frames = 0;
	long count = GetTickCount();
	long prevCount = 0;
	double fps = 0;
	// image capturing loop
	Mat src_gray, subImage, subImage_gray;

	Mat transformMatrix;

	vector<Vec3f> circles;
	Point center;
	Point n_center;
	int radius = 0;
	int counter = 0;

	char* fpsText = new char[5];
	char* pos_text = new char[10];

	Mat image = imread("test.jpg", CV_LOAD_IMAGE_COLOR);

	while(_running)
	{
		CLEyeCameraGetFrame(_cam, pCapBuffer);
		//check fps every 100 frames
		frames++;

		if((frames % 100) == 0){
			prevCount = count;
			count = GetTickCount();
			fps = 100000.0/(count - prevCount);
			//std::cout << "fps: " << fps << endl;
			sprintf(fpsText, "fps: %f", fps);
		}

		
		int countPixel = 0;
		int grayThreshold = 80;
		int p_x = 0;
		int p_y = 0;

		int result_x = 0;
		int result_y = 0;

		if(_isCalibrated == true){
			trackLaserSpot(pCapture, p_x, p_y, countPixel, grayThreshold);
			Mat sample = (Mat_<double>(3, 1)<< p_x, p_y, 1);
			Mat result = transformMatrix * sample;
			double s = result.at<double>(2, 0);

			result_x = result.at<double>(0, 0)/s;
			result_y = result.at<double>(1, 0)/s;
			cout << result_x << "\t" << result_y << endl;

			circle(image, Point2f(result_x, result_y), 2, Scalar(255, 0, 0));

			//cout << p_x << "   " << p_y << "  " << countPixel << endl;
		}
		else{
			transformMatrix = warpPerspectiveTransform(corners);

			if(mouseClicked == 4){
			for(int i = 0; i < 3; i++){
				cout << transformMatrix.at<double>(0, i) << "\t" << transformMatrix.at<double>(1, i) << "\t" << transformMatrix.at<double>(2, i) << endl; 
			}
			}
		}

		if(frames > 100){
			putText(pCapture, fpsText, Point(5, 20), CV_FONT_HERSHEY_PLAIN, 1, Scalar(255, 255, 255));
		}
		else{
			putText(pCapture, "calculating fps...", Point(5, 20), CV_FONT_HERSHEY_PLAIN, 1, Scalar(255, 255, 255));
		}


		imshow(trackingWindowName, image);
		imshow(_windowName, pCapture);
	}

	// Stop camera capture
	CLEyeCameraStop(_cam);
	// Destroy camera object
	CLEyeDestroyCamera(_cam);
	// Destroy the allocated OpenCV image
	cvReleaseImage(&pCapImage);
	_cam = NULL;
}

void CLEyeCameraCapture::trackLaserSpot(Mat& pCapture, int& p_x, int& p_y, int& countPixel,int& grayThreshold){

	int sum_value = 0;
	int sum_x = 0;
	int sum_y = 0;
	//int grayThreshold = 100;
	countPixel = 0;
	p_x = 0;
	p_y = 0;

	//find the centroid from laser spot pixels whose value is larger than threshold
	for(int j = 0; j < pCapture.rows ; j++){
		uchar *data = pCapture.ptr<uchar>(j) ;
		int value = 0;
		for(int i = 0; i < pCapture.cols; i++){
			value = (int)data[i];
			if(value > grayThreshold){
				sum_x += value*i;
				sum_y += value*j;
				sum_value += value;
				countPixel++;

			}
		}
		if(countPixel > 60){
			cout << "Please set a lower gain value or Check IR filter!!" << endl;
			break;
		}
	}

	if(sum_value > 0){

		p_x = (sum_x + 0.0)/sum_value;
		p_y = (sum_y + 0.0)/sum_value;
	}
}

Mat CLEyeCameraCapture::warpPerspectiveTransform(Point2f src[4]){
	Mat t;

	Point2f dst[4];
	dst[0].x = 1;
	dst[0].y = 1;
	dst[1].x = 1;
	dst[1].y = 240;
	dst[2].x = 320;
	dst[2].y = 1;
	dst[3].x = 320;
	dst[3].y = 240;

	if(mouseClicked == 4 && _isCalibrated == false){
		t = getPerspectiveTransform(src, dst);

		
		_isCalibrated = true;
	}
	return t;
}


Mat CLEyeCameraCapture::GetCaptureImage()
{
	return pCapture;
}



static void onMouse(int event, int x, int y, int, void*){
	if(event == CV_EVENT_LBUTTONDOWN && mouseClicked < 4){
		corners[mouseClicked] = Point(x, y);
		cout << corners[mouseClicked].x << "  " << mouseClicked << endl;
		mouseClicked++;
	}
}

// Main program entry point
int main(int argc, const char** argv[])
{
	CLEyeCameraCapture *cam[2] = { NULL };
	srand(GetTickCount());
	// Query for number of connected cameras
	int numCams = CLEyeGetCameraCount();
	if(numCams == 0)
	{
		cout << "No PS3Eye cameras detected" << endl;
		return -1;
	}

	cout << "Found " << numCams << " cameras" << endl;
	for(int i = 0; i < numCams; i++)
	{
		char windowName[64];
		// Query unique camera uuid
		GUID guid = CLEyeGetCameraUUID(i);
		printf("Camera %d GUID: [%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x]\n", 
			i+1, guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1], guid.Data4[2],
			guid.Data4[3], guid.Data4[4], guid.Data4[5],
			guid.Data4[6], guid.Data4[7]);
		sprintf(windowName, "Camera Window %d", i+1);
		
		// Create camera capture object
		cam[i] = new CLEyeCameraCapture(windowName, guid,  CLEYE_MONO_PROCESSED, CLEYE_QVGA, 180);
		cout << "Starting capture on camera " << i+1 << endl;
		cam[i]->StartCapture();
	}

	setMouseCallback("Camera Window 1", onMouse, 0);

	cout << "Use the following keys to change camera parameters:\n"
		"\t'1' - select camera 1\n"
		"\t'2' - select camera 2\n"
		"\t'g' - select gain parameter\n"
		"\t'e' - select exposure parameter\n"
		"\t'z' - select zoom parameter\n"
		"\t'r' - select rotation parameter\n"
		"\t'+' - increment selected parameter\n"
		"\t'-' - decrement selected parameter\n" << endl;
	// The <ESC> key will exit the program
	CLEyeCameraCapture *pCam = NULL;
	int param = -1, key;
	while((key = cvWaitKey(0)) != 0x1b)
	{
		switch(key)
		{
		case 'g':	case 'G':	cout << "Parameter Gain" << endl;		param = CLEYE_GAIN;		break;
		case 'e':	case 'E':	cout << "Parameter Exposure" << endl;	param = CLEYE_EXPOSURE;	break;
		case 'z':	case 'Z':	cout << "Parameter Zoom" << endl;		param = CLEYE_ZOOM;		break;
		case 'r':	case 'R':	cout << "Parameter Rotation" << endl;	param = CLEYE_ROTATION;	break;
		case '1':				cout << "Selected camera 1" << endl;	pCam = cam[0];			break;
		case '2':				cout << "Selected camera 2" << endl;	pCam = cam[1];			break;
		case '+':	if(numCams == 1){
			pCam = cam[0];
					}
					if(pCam)	pCam->IncrementCameraParameter(param);		break;
		case '-':	if(numCams == 1){
			pCam = cam[0];
					}
					if(pCam)	pCam->DecrementCameraParameter(param);		break;
		}
	}
	//delete cams
	for(int i = 0; i < numCams; i++)
	{
		cout << "Stopping capture on camera " << i+1 << endl;
		cam[i]->StopCapture();
		delete cam[i];
	}
	return 0;
}
