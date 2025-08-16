
#include <config.h>
#include <convert_manager.h>
#include <string.h>

static PT_VideoConvert g_ptVideoConvertHead = NULL;

int RegisterVideoConvert (PT_VideoConvert ptVideoConvert) {
    PT_VideoConvert ptTmp;

    if (!g_ptVideoConvertHead)  {
        g_ptVideoConvertHead = ptVideoConvert;
    } else {
        ptTmp = g_ptVideoConvertHead;
        while (ptTmp->next) {
            ptTmp = ptTmp->ptNext;
        }
        ptTmp->ptNext = ptVideoConvert;
        ptVideoConvert->ptNext = NULL;
    }

    return 0;
}

int VideoConvertInit (void) {
    int iError;

    iError = Yvu2RGBInit();
    iError |= MJPEG2RGBInit();
    iError |= Rgb2RgbInit();


    return 0;
}