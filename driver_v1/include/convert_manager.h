
#ifndef _CONVERT_MANAGER_H
#define _CONVERT_MANAGER_H

#include <video_manager.h>
#include <config.h>
#include <linux/videodev2.h>


typedef struct VideoConvert {
    char *name;
    int (*isSupport) (int iPixelFormatIn, int iPixelFormatOut);
    int (*Convert) (PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);
    int (*ConvertExit) (PT_VideoBuf ptVideoBufOut);
    struct VideoConvert *ptNext;
} T_VideoConvert, *PT_VideoConvert;

int RegisterVideoConvert (PT_VideoConvert ptVideoConvert);
void ShowVideoConvert(void);
PT_VideoConvert GetVideoConvert (char* pcName);
PT_VideoConvert GetVideoConvertByFormat(int iPixelFormatIn, int iPixelFormatOut);
int VideoConvertInit(void);

int Yuv2RgbInit();
int Mjpeg2RgbInit();
int Rgb2RgbInit();

#endif
