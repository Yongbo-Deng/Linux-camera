
#include <convert_manager.h>

extern void jpeg_mem_src_tj(struct jpeg_decompress_struct *cinfo, unsigned char *buffer, unsigned long size);

static int isSupportMjpeg2Rgb(int iPixelFormatIn, int iPixelFormatOut) {
    if (iPixelFormatIn != V4L2_PIX_FMT_MJPEG) {
        return 0; // Not supported
    }
    if (iPixelFormatOut != V4L2_PIX_FMT_RGB565 && iPixelFormatOut != V4L2_PIX_FMT_RGB32) {
        return 0; // Not supported
    }
    return 1; // Supported
}

static void MyErrorExit(j_common_ptr cinfo) {
    // Custom error handling logic
}

static int CovertOneLine(int iWidth, int iBpp, unsigned char *input_ptr, unsigned char *output_ptr) {
    unsigned int dwRed;
    unsigned int dwGreen;
    unsigned int dwBlue;
    unsigned int dwColor;

    unsigned short *pwDstDatas16bpp = (unsigned short *)output_ptr;
    unsigned int *pdwDstDatas32bpp = (unsigned int *)output_ptr;

    int i;
    int pos = 0;

    if (iSrcBpp != 24) {
        return -1; // Unsupported source bits per pixel 
    }

    if (iDstBpp == 24) {
        memcpy(pudDstDatas, pudSrcDatas, iWidth * 3);
    } else {
        for (i = 0; i < iWidth; i++) {
            dwRed = pudSrcDatas[pos++];
            dwGreen = pudSrcDatas[pos++];
            dwBlue = pudSrcDatas[pos++];
            if (iDstBpp == 32) {
                /* RGB32 */
                dwColor = (dwRed << 16) | (dwGreen << 8) | dwBlue;
                *pwDstDatas32bpp++ = dwColor;
            } else if (iDstBpp == 16) {
                /* RGB565 */
                dwRed = dwRed >> 3; // 5 bits for red
                dwGreen = dwGreen >> 2; // 6 bits for green
                dwBlue = dwBlue >> 3; // 5 bits for blue
                dwColor = (dwRed << 11) | (dwGreen << 5) | dwBlue;
                *pwDstDatas16bpp++ = dwColor;
            }
        }
    }

    return 0; // Placeholder return value
}

static int Mjepg2Rgb565(unsigned char *input_ptr, unsigned char *output_ptr, unsigned int width, unsigned int height) {
    return 0; // Placeholder for MJPEG to RGB565 conversion logic
}

static int Mjpeg2Rgb32(unsigned char *input_ptr, unsigned char *output_ptr, unsigned int width, unsigned int height) {
    // Implement MJPEG to RGB32 conversion logic here
    return 0; // Placeholder return value
}

static int Mjpeg2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut) {
	struct jpeg_decompress_struct tDInfo;
	//struct jpeg_error_mgr tJErr;
    int iRet;
    int iRowStride;
    unsigned char *aucLineBuffer = NULL;
    unsigned char *pucDest;
	T_MyErrorMgr tJerr;
    PT_PixelDatas ptPixelDatas = &ptVideoBufOut->tPixelDatas;

	/* Initialize JPEG decompression structure */
    //tDInfo.err = jpeg_std_error(&tJErr);

	tDInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = MyErrorExit;

	if (setjmp(tJerr.setjmp_buffer)) {
        // Error occurred during decompression
        jpeg_destroy_decompress(&tDInfo);
        if (aucLineBuffer) {
            free(aucLineBuffer);
        }
        if (ptPixelDatas->aucPixelDatas) {
            free(ptPixelDatas->aucPixelDatas);
        }
		return -1;
	}

	jpeg_create_decompress(&tDInfo);

    //jpeg_stdio_src(&tDInfo, ptFileMap->tFp);
    /* Set the source of the JPEG data, from memeory */
    jpeg_mem_src_tj (&tDInfo, ptVideoBufIn->tPixelDatas.aucPixelDatas, ptVideoBufIn->tPixelDatas.iTotalBytes);
    

    iRet = jpeg_read_header(&tDInfo, TRUE);

    tDInfo.scale_num = tDInfo.scale_denom = 1;
    
	jpeg_start_decompress(&tDInfo);
    
	iRowStride = tDInfo.output_width * tDInfo.output_components;
	aucLineBuffer = malloc(iRowStride);

    if (NULL == aucLineBuffer) {
        return -1;
    }

	ptPixelDatas->iWidth  = tDInfo.output_width;
	ptPixelDatas->iHeight = tDInfo.output_height;
	//ptPixelDatas->iBpp    = iBpp;
	ptPixelDatas->iLineBytes    = ptPixelDatas->iWidth * ptPixelDatas->iBpp / 8;
    ptPixelDatas->iTotalBytes   = ptPixelDatas->iHeight * ptPixelDatas->iLineBytes;
	if (NULL == ptPixelDatas->aucPixelDatas) {
	    ptPixelDatas->aucPixelDatas = malloc(ptPixelDatas->iTotalBytes);
	}

    pucDest = ptPixelDatas->aucPixelDatas;

	// 循环调用jpeg_read_scanlines来一行一行地获得解压的数据
	while (tDInfo.output_scanline < tDInfo.output_height) {
        /* 得到一行数据,里面的颜色格式为0xRR, 0xGG, 0xBB */
		(void) jpeg_read_scanlines(&tDInfo, &aucLineBuffer, 1);

		// 转到ptPixelDatas去
		CovertOneLine(ptPixelDatas->iWidth, 24, ptPixelDatas->iBpp, aucLineBuffer, pucDest);
		pucDest += ptPixelDatas->iLineBytes;
	}
	
	free(aucLineBuffer);
	jpeg_finish_decompress(&tDInfo);
	jpeg_destroy_decompress(&tDInfo);

    return 0;
}


static int Mjpeg2RgbConvertExit(PT_VideoBuf ptVideoBufOut) {
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas) {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0; // Cleanup successful
}


static T_VideoConvert g_tMjpeg2RgbConvert = {
    .isSupport = isSupportMjpeg2Rgb,
    .Convert = Mjpeg2RgbConvert,
    .ConvertExit = Mjpeg2RgbConvertExit
};

int Mjpeg2RgbInit(void) {
    return RegisterVideoConvert(&g_tMjpeg2RgbConvert);
}
