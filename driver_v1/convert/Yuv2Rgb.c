
#include <convert_manager.h>

int (Yuv2RgbisSupport)(int iPixelFormatIn, int iPixelFormatOut) {
    if (iPixelFormatIn != V4L2_PIX_FMT_YUYV) {
        return 0; // Not supported
    }
    if (iPixelFormatOut != V4L2_PIX_FMT_RGB565 && iPixelFormatOut != V4L2_PIX_FMT_RGB32) {
        return 0; // Not supported
    }
    return 1; // Supported
}

static int Pyuv422toRgb565 (unsigned char *input_ptr, unsigned char *output_ptr, unsigned int width, unsigned int height) {
    unsigned int i, size;
    unsigned char Y, Y1, U, V;
    unsigned char *buff = input_ptr;
    unsigned char *output_pt = output_ptr;

    unsigned int r, g, b;
    unsigned int color;

    size = image_width * image_height / 2; // YUV422, 2 pixels per 4 bytes
    for (i = size; i > 0; i--) {
        Y = buff[0];
        U = buff[1];
        Y1 = buff[2];
        V = buff[3];
        buff += 4;
        r = R_FROMYV(Y, V);
        g = G_FROMYUV(Y, U, V);
        b = B_FROMYU(Y, U);
        
        // Pack RGB into 16-bit format
        r = r >> 3; // 5 bits for red
        g = g >> 2; // 6 bits for green
        b = b >> 3; // 5 bits for blue
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xFF; // Lower byte
        *output_pt++ = (color >> 8) & 0xFF; // Upper byte

        // Process second pixel
        r = R_FROMYV(Y1, V);
        g = G_FROMYUV(Y1, U, V);
        b = B_FROMYU(Y1, U);

        r = r >> 3; // 5 bits for red
        g = g >> 2; // 6 bits for green
        b = b >> 3; // 5 bits for blue
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xFF; // Lower byte
        *output_pt++ = (color >> 8) & 0xFF; // Upper byte
    }
    
    return 0;
}

static int Pyuv422toRgb32 (unsigned char *input_ptr, unsigned char *output_ptr, unsigned int width, unsigned int height) {
    unsigned int i, size;
    unsigned char Y, Y1, U, V;
    unsigned char *buff = input_ptr;
    unsigned int *output_pt = output_ptr;
    unsigned int r, g, b;
    unsigned int color;

    size = width * height / 2; // YUV422, 2 pixels per 4 bytes

    for (i = size; i > 0; i--) {
        Y = buff[0];
        U = buff[1];
        Y1 = buff[2];
        V = buff[3];
        buff += 4;

        r = R_FROMYV(Y, V);
        g = G_FROMYUV(Y, U, V);
        b = B_FROMYU(Y, U);
        
        // Pack RGB into 32-bit format
        color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;

        // Process second pixel
        r = R_FROMYV(Y1, V);
        g = G_FROMYUV(Y1, U, V);
        b = B_FROMYU(Y1, U);
        color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
    }
    
    return 0;
}

static int (Yuv2RgbConvert)(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut) {
    PT_PixelDatas ptPixelDatasIn = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    ptPixelDatasOut->iWidth = ptPixelDatasIn->iWidth;
    ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;

    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565) {
        ptPixelDatasOut->iBpp = 16; // RGB565 has 16 bits per pixel
        ptPixelDatasOut->iLineByte = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalByte = ptPixelDatasOut->iLineByte * ptPixelDatasOut->iHeight;
        
        if (!ptPixelDataOut->aucPixelDatas) {
            ptPixelDatasOut = malloc(ptPixelDatasOut->iTotalByte);
        }

        return Pyuv422toRgb565(ptVideoBufIn->tPixelDatas.pucData, ptVideoBufOut->tPixelDatas.pucData, 
                               ptVideoBufIn->tPixelDatas.iWidth, ptVideoBufIn->tPixelDatas.iHeight);
    } else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32) {
        ptPixelDatasOut->iBpp = 32; // RGB32 has 32 bits per pixel
        ptPixelDatasOut->iLineByte = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalByte = ptPixelDatasOut->iLineByte * ptPixelDatasOut->iHeight;

        if (!ptPixelDataOut->aucPixelDatas) {
            ptPixelDatasOut = malloc(ptPixelDatasOut->iTotalByte);
        }
        return Pyuv422toRgb32(ptVideoBufIn->tPixelDatas.pucData, ptVideoBufOut->tPixelDatas.pucData, 
                              ptVideoBufIn->tPixelDatas.iWidth, ptVideoBufIn->tPixelDatas.iHeight);
    }

    return -1; // Unsupported output format
}

static int (Yuv2RgbConvertExit)(PT_VideoBuf ptVideoBufOut) {
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas) {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0; // Cleanup successful
}

static T_VideoConvert g_tYuv2RgbConvert = {
    .isSupport = Yuv2RgbisSupport,
    .Convert = Yuv2RgbConvert,
    .ConvertExit = Yuv2RgbConvertExit
};

extern void initLut(void);

int Yuv2RgbInit(void) {
    initLut(); // Initialize lookup tables for color conversion
    return RegisterVideoConvert(&g_tYuv2RgbConvert);
}