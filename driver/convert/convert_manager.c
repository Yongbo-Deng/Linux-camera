
#include <config.h>
#include <convert_manager.h>
#include <string.h>

static PT_VideoConvert g_ptVideoConvertHead = NULL;

int RegisterVideoConvert (PT_VideoConvert ptVideoConvert) {
    PT_VideoConvert ptTmp;

    if (!g_ptVideoConvertHead)  {
        g_ptVideoConvertHead = ptVideoConvert;
        ptVideoConvert->ptNext = NULL;
    } else {
        ptTmp = g_ptVideoConvertHead;
        while (ptTmp->ptNext) {
            ptTmp = ptTmp->ptNext;
        }
        ptTmp->ptNext = ptVideoConvert;
        ptVideoConvert->ptNext = NULL;
    }

    return 0;
}

void ShowVideoConvert(void) {
    int i = 0;
    PT_VideoConvert ptTmp = g_ptVideoConvertHead;

    while (ptTmp) {
        printf("%02d %s\n", i++, ptTmp->name);
        ptTmp = ptTmp->ptNext;
    }
}

PT_VideoConvert GetVideoConvert (char* pcName) {
    PT_VideoConvert ptTmp = g_ptVideoConvertHead;

    while (ptTmp) {
        if (strcmp(ptTmp->name, pcName) == 0) {
            return ptTmp; // Found the video convert operation
        }
        ptTmp = ptTmp->ptNext;
    }
    return NULL; // Not found
}


PT_VideoConvert GetVideoConvertByFormat(int iPixelFormatIn, int iPixelFormatOut) {
    PT_VideoConvert ptTmp = g_ptVideoConvertHead;

    while (ptTmp) {
        if (ptTmp->isSupport(iPixelFormatIn, iPixelFormatOut)) {
            printf("Found convert: %s\n", ptTmp->name);
            return ptTmp; // Found a supported video convert operation
        }
        ptTmp = ptTmp->ptNext;
    }
    return NULL; // No supported video convert operation found
}


int VideoConvertInit (void) {
    int iError;

    iError = Yuv2RgbInit();
    iError |= Mjpeg2RgbInit();
    iError |= Rgb2RgbInit();

    return 0;
}
