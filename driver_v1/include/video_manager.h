
#ifndef _VIDEO_MANAGER_H_
#define _VIDEO_MANAGER_H_

#include <config.h>
#include <pic_operation.h>
#include <linux/videodev2.h>

#define NB_BUFFER 4

struct VideoDevice;
struct VideoOpr;
typedef struct VideoDevice T_VideoDevice, *PT_VideoDevice;
typedef struct VideoOpr T_VideoOpr, *PT_VideoOpr;

typedef struct VideoDevice {
    int iFd;
    int iPixelFormat;
    int iWidth;
    int iHeight;

    int iVideoBufCnt;
    int iVideoBufMaxLen;
    int iVideoBufCurIndex;  
    unsigned char *pucVideoBufs[NB_BUFFER];

    /* Functions */
    PT_VideoOpr ptOpr;
}T_VideoDevice, *PT_VideoDevice;

typedef struct VideoBuf {
    T_PixelDatas tPixelDatas;
    int iPixelFormat;
} T_VideoBUf, *PT_VideoBuf;

typedef struct VideoOpr {
    int (*InitDevice) (char *strDevName, PT_VideoDevice ptVideoDevice);
    int (*ExitDevice) (PT_VideoDevice ptVideoDevice);
    int (*GetFrame) (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
    int (*PutFrame) (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
    int (*StartDevice) (PT_VideoDevice ptVideoDevice);
    int (*StopDevice) (PT_VideoDevice ptVideoDevice);
} T_VideoOpr, *PT_VideoOpr;

int V4l2Init(void);

#endif