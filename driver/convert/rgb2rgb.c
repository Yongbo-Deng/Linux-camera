
#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>

static int isSupportRgb2Rgb(int iPixelFormatIn, int iPixelFormatOut) {
    if (iPixelFormatIn != V4L2_PIX_FMT_RGB565) {
        return 0; // Not supported
    }
    if (iPixelFormatOut != V4L2_PIX_FMT_RGB565 && iPixelFormatOut != V4L2_PIX_FMT_RGB32) {
        return 0; // Not supported
    }
    return 1; // Supported
}

static int Rgb2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut) {
    PT_PixelDatas ptPixelDatasIn = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    int y;
    int x;
    int r, g, b;
    int color;
    unsigned short *pwSrc = (unsigned short *)ptPixelDatasIn->aucPixelDatas;
    unsigned int *pwDst = (unsigned int *)ptPixelDatasOut->aucPixelDatas;

    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565) {
        // No conversion needed, just copy the data
        ptPixelDatasOut->iWidth = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp = 16;
        ptPixelDatasOut->iLineBytes = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        if (!ptPixelDatasOut->aucPixelDatas) {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        memcpy(ptPixelDatasOut->aucPixelDatas, ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->iTotalBytes);
    } else {
        // Conversion logic for
        ptPixelDatasOut->iWidth = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp = 32;
        ptPixelDatasOut->iLineBytes = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        if (!ptPixelDatasOut->aucPixelDatas) {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        for (y = 0; y < ptPixelDatasOut->iHeight; y++) {
            for (x = 0; x < ptPixelDatasOut->iWidth; x++) {
                color = *pwSrc++;
                r = (color >> 11) & 0x1F; 
                g = (color >> 5) & 0x3F;  
                b = color & 0x1F;         

                color = ((r << 3) << 16) | ((g << 2) << 8) | (b << 3); // Combine into RGB32 format
                *pwDst++ = color;
            }
        }
    }

    return 0; // Conversion successful

}

static int Rgb2RgbConvertExit(PT_VideoBuf ptVideoBufOut) {
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas) {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0; // Placeholder return value
}

static T_VideoConvert g_tRgb2RgbConvert = {
    .name = "rgb2rgb",
    .isSupport = isSupportRgb2Rgb,
    .Convert = Rgb2RgbConvert,
    .ConvertExit = Rgb2RgbConvertExit
};

int Rgb2RgbInit(void) {
    return RegisterVideoConvert(&g_tRgb2RgbConvert);
}