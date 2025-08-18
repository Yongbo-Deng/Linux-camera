#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>


/* Usage: video2lcd </dev/videoX>
 */
int main (int argc, char *argv[]) {
    int iError;
    T_VideoDevice tVideoDevice;

    PT_VideoBuf ptVideoBufCur;
    T_VideoBuf tVideoBuf;
    T_VideoBuf tConvertBuf;
    T_VideoBuf tZoomBuf;
    T_VideoBuf tFrameBuf;
    
    PT_VideoConvert ptVideoConvert;
    int iPixelFormatOfVideo;
    int iPixelFormatOfDiplay;

    int iLcdWidth;
    int iLcdHeight;
    int iLcdBpp;
    float k;

    int iTopLeftX;
    int iTopLeftY;

    if (argc != 2) {
        printf("Usage: %s </dev/videoX>\n", argv[0]);
        return -1;
    }

    /* Init */
    DisplayInit();

    SelectAndInitDefaultDispDev("fb");
    GetDispResolution(&iLcdWidth, &iLcdHeight, &iLcdBpp);
    GetVideoBufForDisplay(&tFrameBuf);
    iPixelFormatOfDiplay = tFrameBuf.iPixelFormat;

    VideoInit();

    iError = VideoDeviceInit(argv[1], &tVideoDevice);

    if (iError) {
        DBG_PRINTF("VideoDeviceInit failed\n");
        return -1;
    }
    iPixelFormatOfVideo = tVideoDevice.ptOpr->GetFormat(&tVideoDevice);
    
    VideoConvertInit();
    ptVideoConvert = GetVideoConvertByFormat(iPixelFormatOfVideo, iPixelFormatOfDiplay);
    if (ptVideoConvert == NULL) {
        DBG_PRINTF("No suitable video convert found for format %d to %d\n", iPixelFormatOfVideo, iPixelFormatOfDiplay);
        return -1;
    }

    /* Start device. */
    iError = tVideoDevice.ptOpr->StartDevice(&tVideoDevice);
    if (iError) {
        DBG_PRINTF("Start video device failed\n");
        return -1;
    }

    memset(&tVideoBuf, 0, sizeof(tVideoBuf));
    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
    tVideoBuf.tPixelDatas.iBpp = iLcdBpp;
    memset(&tZoomBuf, 0, sizeof(tZoomBuf));

    while (1) {
        /* Read camera data. */
        iError = tVideoDevice.ptOpr->GetFrame(&tVideoDevice, &tVideoBuf);
        if (iError) {
            DBG_PRINTF("GetFrame failed\n");
            return -1;
        }

        /* Convert to RGB format. */
        if (iPixelFormatOfDiplay != iPixelFormatOfVideo) {
            iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
            if (iError) {
                DBG_PRINTF("Convert failed\n");
                return -1;
            }
        } else {
            /* No conversion needed, just use the original buffer. */
            memcpy(&tConvertBuf, &tVideoBuf, sizeof(tVideoBuf));
        }
        ptVideoBufCur = &tConvertBuf;

        /* If the video resolution is greater than LCD, zoom render. */
        if (tConvertBuf.tPixelDatas.iWidth > iLcdWidth || tConvertBuf.tPixelDatas.iHeight > iLcdHeight) {
            k = (float)tConvertBuf.tPixelDatas.iHeight / tConvertBuf.tPixelDatas.iWidth;
            tZoomBuf.tPixelDatas.iWidth = iLcdWidth;
            tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;
            if (tZoomBuf.tPixelDatas.iHeight > iLcdHeight) {
                tZoomBuf.tPixelDatas.iHeight = iLcdHeight;
                tZoomBuf.tPixelDatas.iWidth = iLcdHeight / k;
            }

            tZoomBuf.tPixelDatas.iBpp = iLcdBpp;
            tZoomBuf.tPixelDatas.iLineBytes = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
            tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;

            if (!tZoomBuf.tPixelDatas.aucPixelDatas) {
                tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
            }

            PicZoom(&tVideoBuf.tPixelDatas, &tZoomBuf.tPixelDatas);
            ptVideoBufCur = &tZoomBuf;
        } 

        /* Merge into frambuffer. */
        iTopLeftX = (iLcdWidth - ptVideoBufCur->tPixelDatas.iWidth) / 2;
        iTopLeftY = (iLcdHeight - ptVideoBufCur->tPixelDatas.iHeight) / 2;

        PicMerge(iTopLeftX, iTopLeftY, &tConvertBuf.tPixelDatas, &tZoomBuf.tPixelDatas);

        FlushPixelDatasToDev(&tFrameBuf.tPixelDatas);

        iError = tVideoDevice.ptOpr->PutFrame(&tVideoDevice, &tVideoBuf);
        if (iError) {
            DBG_PRINTF("PutFrame failed\n");
            return -1;
        }

        /* Send frambuffer to LCD. */
    }
    
}
