
#ifndef _CONVERT_MANAGER_H_
#define _CONVERT_MANAGER_H_

#include <video_manager.h>
#include <config.h>


typedef struct VideoConvert {
    int (*isSupport) (int iPixelFormatIn, int iPixelFormatOut);
    int (*Convert) (PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);
    int (*ConvertExit) (PT_VideoBuf ptVideoBufOut);
} T_VideoConvert, *PT_VideoConvert;


#endif