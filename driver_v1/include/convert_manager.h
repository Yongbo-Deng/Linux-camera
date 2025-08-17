
#ifndef _CONVERT_MANAGER_H
#define _CONVERT_MANAGER_H

#include <video_manager.h>
#include <config.h>


typedef struct VideoConvert {
    int (*isSupport) (int iPixelFormatIn, int iPixelFormatOut);
    int (*Convert) (PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);
    int (*ConvertExit) (PT_VideoBuf ptVideoBufOut);
} T_VideoConvert, *PT_VideoConvert;

int RegisterVideoConvert (PT_VideoConvert ptVideoConvert);
void ShowVideoConvert(void);
int GetVideoConvert (char* pcName);
PT_VideoConvert GetVideoConvertByFormat(int iPixelFormatIn, int iPixelFormatOut);
int VideoConvertInit(void);

#endif