#include<stdio.h>
#include<opencv/highgui.h>
#include<opencv/cv.h>
#include<opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <linux/videodev.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include<linux/videodev2.h>
 
 
 
int main()
{
    char c;
     
    CvCapture *camera=cvCaptureFromCAM(0);     //   0=default, -1=any camera , 1..99=your camera
 
    if(!camera)
            printf("No camera detected %u... \n",camera);
    else
            printf("detected\n");
     
    //cvNamedWindow("erfolg", CV_WINDOW_AUTOSIZE);
     
    //IplImage *frame=cvQueryFrame(camera);
     
     
    //IplImage *frame;
     
    //cvGrabFrame(camera);
    /*
     
    if(!frame)
            printf("Frame not detected %u... \n",camera);
    else
            printf("Frame detected\n");
     
    /*cvGrabFrame(camera);
     
    frame=cvRetrieveFrame(camera);*/
    /*
    while(1)
    {
        frame=cvQueryFrame(camera);
     
        if(!frame)
        {
            printf("no frames ...");
            return -1;
        }
        else
        {
            cvShowImage("erfolg",frame);
            int w=cvWaitKey(100);
            break;
        }
    }
    */
    //cvDestroyWindow("erfolg");
    cvReleaseCapture(&camera);
    return 0;
}
