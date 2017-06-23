#include "Viewer.h"

#include <math.h>
#include <string.h>

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>

using namespace cv;
using namespace std;

SampleViewer* SampleViewer::ms_self = NULL;

SampleViewer::SampleViewer(const char* strSampleName, const char* deviceUri)
{
#ifdef DEPTH
	m_pClosestPoint = NULL;
	m_pClosestPointListener = NULL;
	m_pClosestPoint = new closest_point::ClosestPoint(deviceUri);
	skelD = NULL;
#else
    if( deviceUri==NULL )
    {
        int camera = 0;
        if(!capture.open(camera))
            cout << "Capture from camera #" <<  camera << " didn't work" << endl;
    }
    else
    {
        Mat image = imread( deviceUri, 1 );
        if( image.empty() )
        {
            if(!capture.open( deviceUri ))
                cout << "Could not read " << deviceUri << endl;
        }
    }
#endif
	ms_self = this;
	strncpy(m_strSampleName, strSampleName, strlen(strSampleName));

	skel = NULL;
	subSample = 2;
	frameCount = 0;
}
SampleViewer::~SampleViewer()
{
	finalize();

	delete[] m_pTexMap;

	ms_self = NULL;

	if (skel)
            delete skel;
#ifdef DEPTH
	if (skelD)
            delete skelD;
#endif
}

void SampleViewer::finalize()
{
#ifdef DEPTH
	if (m_pClosestPoint != NULL)
	{
		m_pClosestPoint->resetListener();
		delete m_pClosestPoint;
		m_pClosestPoint = NULL;
	}
	if (m_pClosestPointListener != NULL)
	{
		delete m_pClosestPointListener;
		m_pClosestPointListener = NULL;
	}
#endif
}

int SampleViewer::init(int argc, char **argv)
{
	m_pTexMap = NULL;

#ifdef DEPTH
	if (!m_pClosestPoint->isValid())
	{
		return openni::STATUS_ERROR;
	}

	m_pClosestPointListener = new MyMwListener;
	m_pClosestPoint->setListener(*m_pClosestPointListener);

	return openni::STATUS_OK;
#else
	return 0;
#endif

}
int SampleViewer::run()	//Does not return
{
#ifdef DEPTH
printf("Compilado com Depth\n");
#else
printf("Compilado SEM Depth\n");
#endif

	while (1) {
		display();
		char c = (char)waitKey(10);
	        if( c == 27 || c == 'q' || c == 'Q' )
        	        break;
	}
#ifdef DEPTH
	return openni::STATUS_OK;
#else
	return 0;
#endif
}
void SampleViewer::display()
{
	int sizePixel = 3;
#ifdef DEPTH
	sizePixel = sizeof(openni::RGB888Pixel);
	if (!m_pClosestPointListener->isAvailable())
	{
		return;
	}
	
	// depthFrame
	openni::VideoFrameRef srcFrame = m_pClosestPointListener->getFrame();
	const closest_point::IntPoint3D& closest = m_pClosestPointListener->getClosestPoint();
	m_pClosestPointListener->setUnavailable();
#else
	Mat srcFrame;
	srcFrame.data = NULL;
	if( capture.isOpened() ) {
		Mat framec;
		capture >> framec;
		if(! framec.empty() ) {
			srcFrame = cv::Mat(framec.size(), CV_8UC1);
			cvtColor(framec, srcFrame, CV_RGB2GRAY);
			//srcFrame = framec.clone();
		}
		else
			printf("frame nulo\n");
	}
#endif




#ifdef DEPTH
	if (m_pTexMap == NULL)
	{
		// Texture map init
		m_nTexMapX = srcFrame.getWidth();
		m_nTexMapY = srcFrame.getHeight();
#else
	if (m_pTexMap == NULL && srcFrame.data!=NULL)
	{
		// TODO pegar da webcam opencv
		m_nTexMapX = srcFrame.cols;
		m_nTexMapY = srcFrame.rows;
#endif

		m_pTexMap = new unsigned char[m_nTexMapX * m_nTexMapY * sizePixel];

		skel = new Skeleton(m_nTexMapX, m_nTexMapY, subSample);
#ifdef DEPTH
		skelD = new SkeletonDepth(m_nTexMapX, m_nTexMapY, subSample);
#endif
		cvNamedWindow("Skeleton Traker", CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO | CV_GUI_EXPANDED);
		//cvSetWindowProperty("Skeleton Traker", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
		resizeWindow("Skeleton Traker", m_nTexMapX*2, m_nTexMapY*2);
	}

//printf("sizeof(openni::RGB888Pixel)=%ld\n", sizeof(openni::RGB888Pixel) );

	frameCount++;

	// check if we need to draw depth frame to texture
#ifdef DEPTH
	if (srcFrame.isValid())
	{
		Mat binarized(cv::Size(m_nTexMapX/subSample, m_nTexMapY/subSample), CV_8UC1, cv::Scalar(0));

		memset(m_pTexMap, 0, m_nTexMapX*m_nTexMapY*sizeof(openni::RGB888Pixel));

		skelD->prepareAnalisa(closest);
		//colore e obtem a imagem binarizada
		skelD->paintDepthCopy((openni::RGB888Pixel*)m_pTexMap, srcFrame, &binarized);

		// Converte o openni::VideoFrameRef (srcFrame) para um cv::Mat (frame)
		Mat frame = Mat(Size(m_nTexMapX, m_nTexMapY), CV_8UC3);
		memcpy(frame.data, m_pTexMap, m_nTexMapX*m_nTexMapY*sizePixel);
#else
	if( srcFrame.data != NULL )
	{
		Mat binarizedFirst(cv::Size(m_nTexMapX/subSample, m_nTexMapY/subSample), CV_8UC1, cv::Scalar(0));
		Mat binarized     (cv::Size(m_nTexMapX/subSample, m_nTexMapY/subSample), CV_8UC1, cv::Scalar(0));

		cv::resize(srcFrame, binarizedFirst, binarizedFirst.size());

		cv::threshold(binarizedFirst, binarized, 50, 255, cv::THRESH_BINARY_INV);
		

		Mat frame = srcFrame;
		//Mat frame = binarizedFirst;

		// mode webcam RGB, discard the first 10 frames, because they can be too white.
		if (frameCount>10) {
#endif

		Mat binarizedCp = binarized.clone();
		skel->detectBiggerRegion(&binarized);

		//Mat binarized2    (cv::Size(m_nTexMapX/subSample, m_nTexMapY/subSample), CV_8UC1, cv::Scalar(0));
		//Mat binarizedFirst(cv::Size(m_nTexMapX/subSample, m_nTexMapY/subSample), CV_8UC1, cv::Scalar(0));
		//cv::resize(frame, binarizedFirst, binarizedFirst.size());
		//Canny(binarizedFirst, binarized2, 50, 200, 3);

		//Canny(binarized, binarized2, 50, 200, 3);

		/*std::vector<Vec4i> lines;
		HoughLinesP(binarized, lines, 1, CV_PI/90, 100, 50, 10);
printf("lines.size=%d\n", (int)lines.size());
		for (size_t ii=0; ii< lines.size() ; ii++) {
			Vec4i l = lines[ii];
			line(frame, Point(l[0]*subSample, l[1]*subSample), Point(l[2]*subSample, l[3]*subSample), Scalar(0, 0, 255), 3, CV_AA);
		}*/

		Mat * skeleton = skel->thinning(binarized);
		skel->removeSmallsRegions(skeleton);
		skel->locateMaximus(skeleton);
		std::vector<cv::Point> bdireito = skel->getSkeletonArm(skeleton, true);


		skel->locateShoulders(&binarizedCp);
		skel->locateMainPoints(binarizedCp);

		//skel->drawOverFrame(&binarized2, &frame);
		skel->drawOverFrame(skeleton, &frame);
		skel->drawOverFrame(bdireito, &frame);

		skel->drawMarkers(frame);
#ifndef DEPTH
		}
#endif
		//imshow("Skeleton Traker", *skeleton);
		imshow("Skeleton Traker", frame );
		//imshow("Skeleton Traker", binarized2 );
//printf("aqui 6\n");
	}

}


