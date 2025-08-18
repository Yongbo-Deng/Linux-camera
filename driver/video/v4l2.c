#include <config.h>
#include <video_manager.h>
#include <disp_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>

#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

#define NB_BUFFER 4

static int V4l2GetFrameForReadWrite (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
static int V4l2PutFrameForReadWrite (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);

static T_VideoOpr g_tV4l2VideoOpr;

static int g_SupportedFromats[] = {
    V4L2_PIX_FMT_YUYV,
    V4L2_PIX_FMT_MJPEG,
    V4L2_PIX_FMT_RGB565
};

static int isSupportedFormat(int iFormat) {
    int i;
    for (i = 0; i < sizeof(g_SupportedFromats) / sizeof(int); i++) {
        if (g_SupportedFromats[i] == iFormat) {
            return 1; // Format is supported
        }
    }
    return 0; // Format is not supported
}

/* This function initializes the video device using V4L2 API.
** 1. Open video device
** 2. Query capabilities
** 3. Enumerate formats 
** 4. Set format
** 5. Request buffers
** 6. Query buffer
** 7. Map buffers
** 8. Queue buffers
** 9. Start streaming
** 10. Poll data
** 11. Dequeue buffer
** 12. Process data...
** 13. Queue buffer again
** ....
** For read, write:
** 1. read
** 2. Process data...
** 3. write
** End: Device exiting
 */
static int V4l2InitDevice (char *strDevName, PT_VideoDevice ptVideoDevice) {
    int i;
    int iFd;
    int iError;
    struct v4l2_capability tV4l2Cap;
    struct v4l2_fmtdesc tFmtDesc;
    struct v4l2_format tV4l2Fmt;
    struct v4l2_requestbuffers tReqBufs;
    struct v4l2_buffer tV4l2Buf;

    int iLcdWidth;
    int iLcdHeight;
    int iLcdBpp;
    
    /* Open video device */
    iFd = open(strDevName, O_RDWR);
    if (iFd < 0) {
        DBG_PRINTF("Failed to open video device %s\n", strDevName);
        return -1;
    }
    ptVideoDevice->iFd = iFd;

    /* Query capability */
    iError = ioctl(iFd, VIDIOC_QUERYCAP, &tV4l2Cap);
    memset(&tV4l2Cap, 0, sizeof(struct v4l2_capability));
    iError = ioctl(iFd, VIDIOC_QUERYCAP, &tV4l2Cap);
    if (iError) {
        DBG_PRINTF("Failed to query video device capabilities.\n");
        goto err_exit;
    }
    if (!(tV4l2Cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        DBG_PRINTF("Device %s does not support video capture.\n", strDevName);
        goto err_exit;
    } 
    if (tV4l2Cap.capabilities & V4L2_CAP_STREAMING) {
        DBG_PRINTF("Device %s support streaming.\n", strDevName);
    }
    if (tV4l2Cap.capabilities & V4L2_CAP_READWRITE) {
        DBG_PRINTF("Device %s support read/write.\n", strDevName);
    }

    /* Enumerate formats */
    memset(&tFmtDesc, 0, sizeof(tFmtDesc));
    tFmtDesc.index = 0;
    tFmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(iFd, VIDIOC_ENUM_FMT, &tFmtDesc) == 0) {
        if (isSupportedFormat(tFmtDesc.pixelformat)) {
            ptVideoDevice->iPixelFormat = tFmtDesc.pixelformat;
            break;
        }
        tFmtDesc.index++;
    }
    if (!ptVideoDevice->iPixelFormat) {
        DBG_PRINTF("No supported pixel format found.\n");
        goto err_exit;
    }

    /* Set format */
    GetDispResolution(&iLcdWidth, &iLcdHeight, &iLcdBpp);
    memset(&tV4l2Fmt, 0, sizeof(struct v4l2_format));
    tV4l2Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Fmt.fmt.pix.pixelformat = ptVideoDevice->iPixelFormat;
    tV4l2Fmt.fmt.pix.width       = iLcdWidth;
    tV4l2Fmt.fmt.pix.height      = iLcdHeight;
    tV4l2Fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    DBG_PRINTF("Disp format: %d, %d, %d\n", 
           tV4l2Fmt.fmt.pix.width, tV4l2Fmt.fmt.pix.height, tV4l2Fmt.fmt.pix.pixelformat);


    /* If the driver can't support some parameters such as resolution, 
     * it automatically adjust them and send to application
     */
    iError = ioctl(iFd, VIDIOC_S_FMT, &tV4l2Fmt);
    if (iError) {
        DBG_PRINTF("Failed to set video format.\n");
        goto err_exit;
    }
    ptVideoDevice->iWidth = tV4l2Fmt.fmt.pix.width;
    ptVideoDevice->iHeight = tV4l2Fmt.fmt.pix.height;

    /* Request buffer */
    memset(&tReqBufs, 0, sizeof(tReqBufs));
    tReqBufs.count = NB_BUFFER;
    tReqBufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tReqBufs.memory = V4L2_MEMORY_MMAP;
    iError = ioctl(iFd, VIDIOC_REQBUFS, &tReqBufs);
    if (iError) {
        DBG_PRINTF("Failed to request buffers.\n");
        goto err_exit;
    }

    ptVideoDevice->iVideoBufCnt = tReqBufs.count;
    if (tV4l2Cap.capabilities & V4L2_CAP_STREAMING) {
        /* Map buffers */
        for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++) {
            memset(&tV4l2Buf, 0, sizeof(tV4l2Buf));
            tV4l2Buf.index = i;
            tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            tV4l2Buf.memory = V4L2_MEMORY_MMAP;
            
            iError = ioctl(iFd, VIDIOC_QUERYBUF, &tV4l2Buf);
            if (iError) {
                DBG_PRINTF("Unable to query buffer %d.\n", i);
                goto err_exit;
            }
            ptVideoDevice->iVideoBufMaxLen = tV4l2Buf.length;
            ptVideoDevice->pucVideoBufs[i] = mmap(0,
                            tV4l2Buf.length, PROT_READ, MAP_SHARED, iFd,
                            tV4l2Buf.m.offset);
            if (ptVideoDevice->pucVideoBufs[i] == MAP_FAILED) {
                DBG_PRINTF("Unable to map buffer %d.\n", i);
                goto err_exit;
            }
        }

        /* Queue buffers */
        for (i = 1; i < ptVideoDevice->iVideoBufCnt; i++) {
            memset(&tV4l2Buf, 0, sizeof(tV4l2Buf));
            tV4l2Buf.index = i;
            tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            tV4l2Buf.memory = V4L2_MEMORY_MMAP;
            iError = ioctl(iFd, VIDIOC_QBUF, &tV4l2Buf);
            if (iError) {
                DBG_PRINTF("Unable to queue buffer %d.\n", i);
                goto err_exit;
            }
        }
    } else if (tV4l2Cap.capabilities & V4L2_CAP_READWRITE) {
        g_tV4l2VideoOpr.GetFrame = V4l2GetFrameForReadWrite;
        g_tV4l2VideoOpr.PutFrame = V4l2PutFrameForReadWrite;
        /* For read/write, we don't need to mmap buffers */
        ptVideoDevice->iVideoBufCnt = 1;
        ptVideoDevice->iVideoBufMaxLen = ptVideoDevice->iWidth * ptVideoDevice->iHeight * 4; // Assuming RGB32 format
        ptVideoDevice->pucVideoBufs[0]  = malloc(ptVideoDevice->iVideoBufMaxLen);
    }

    ptVideoDevice->ptOpr = &g_tV4l2VideoOpr;
    return 0;

err_exit:
    close(iFd);
    return -1;
}

static int V4l2ExitDevice (PT_VideoDevice ptVideoDevice) {
    int i;

    for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++) {
        if (ptVideoDevice->pucVideoBufs[i]) {
            munmap(ptVideoDevice->pucVideoBufs[i], ptVideoDevice->iVideoBufMaxLen);
            ptVideoDevice->pucVideoBufs[i] = NULL;
        }
    }
    close(ptVideoDevice->iFd);
    return 0;
}

static int V4l2GetFrameForStreaming (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf) {
    struct pollfd fds[1];
    struct v4l2_buffer tV4l2Buf;
    int iRet;

    /* Poll */
    fds[0].fd = ptVideoDevice->iFd;
    fds[0].events = POLLIN;

    iRet = poll(fds, 1, -1); // Wait indefinitely
    if (iRet <= 0) {
        DBG_PRINTF("Poll failed.\n");
        return -1;
    }

    /* VIDIOC_DQBUF */
    memset(&tV4l2Buf, 0, sizeof(tV4l2Buf));
    tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Buf.memory = V4L2_MEMORY_MMAP;
    iRet = ioctl(ptVideoDevice->iFd, VIDIOC_DQBUF, &tV4l2Buf);
    if (iRet < 0) {
        DBG_PRINTF("Unable to dequeue buffer.\n");
        return -1;
    }
    ptVideoDevice->iVideoBufCurIndex = tV4l2Buf.index;

    ptVideoBuf->iPixelFormat        = ptVideoDevice->iPixelFormat;
    ptVideoBuf->tPixelDatas.iWidth  = ptVideoDevice->iWidth;
    ptVideoBuf->tPixelDatas.iHeight = ptVideoDevice->iHeight;
    ptVideoBuf->tPixelDatas.iBpp    =  (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_YUYV) ? 16 :  \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_MJPEG) ? 0 : \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_RGB565) ? 16:
                                        0;
    ptVideoBuf->tPixelDatas.iLineBytes       = ptVideoDevice->iWidth * ptVideoBuf->tPixelDatas.iBpp / 8; 
    ptVideoBuf->tPixelDatas.iTotalBytes      = tV4l2Buf.bytesused;
    ptVideoBuf->tPixelDatas.aucPixelDatas   = ptVideoDevice->pucVideoBufs[tV4l2Buf.index];

    return 0;
}


static int V4l2PutFrameForStreaming (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf) {
    struct v4l2_buffer tV4l2Buf;
    int iError;

    /* Prepare buffer for queue */
    memset(&tV4l2Buf, 0, sizeof(tV4l2Buf));
    tV4l2Buf.index = ptVideoDevice->iVideoBufCurIndex;
    tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Buf.memory = V4L2_MEMORY_MMAP;

    iError = ioctl(ptVideoDevice->iFd, VIDIOC_QBUF, &tV4l2Buf);
    if (iError) {
        DBG_PRINTF("Unable to queue buffer.\n");
        return -1;  
    }

    return 0;
}

static int V4l2GetFrameForReadWrite (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf) {
    int iRet;

    iRet = read(ptVideoDevice->iFd, ptVideoDevice->pucVideoBufs[0], ptVideoDevice->iVideoBufMaxLen);
    if (iRet <= 0) {
        DBG_PRINTF("Failed to read from video device.\n");
        return -1;
    }
    
    ptVideoBuf->iPixelFormat        = ptVideoDevice->iPixelFormat;
    ptVideoBuf->tPixelDatas.iWidth  = ptVideoDevice->iWidth;
    ptVideoBuf->tPixelDatas.iHeight = ptVideoDevice->iHeight;
    ptVideoBuf->tPixelDatas.iBpp    =  (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_YUYV) ? 16 :  \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_MJPEG) ? 0 : \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_RGB565) ? 16 : \
                                        0;
    ptVideoBuf->tPixelDatas.iLineBytes       = ptVideoDevice->iWidth * ptVideoBuf->tPixelDatas.iBpp / 8; 
    ptVideoBuf->tPixelDatas.iTotalBytes      = iRet;
    ptVideoBuf->tPixelDatas.aucPixelDatas   = ptVideoDevice->pucVideoBufs[0];

    return 0;
}

static int V4l2PutFrameForReadWrite (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf) {
    return 0;
}

static int V4l2StartDevice (PT_VideoDevice ptVideoDevice) {
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    iError = ioctl(ptVideoDevice->iFd, VIDIOC_STREAMON, &iType);
    if (iError) {
        DBG_PRINTF("Failed to start video device streaming.\n");
        return -1;
    }

    return 0;
}

static int V4l2StopDevice (PT_VideoDevice ptVideoDevice) {
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    iError = ioctl(ptVideoDevice->iFd, VIDIOC_STREAMOFF, &iType);
    if (iError) {
        DBG_PRINTF("Failed to stop video device streaming.\n");
        return -1;
    }

    return 0;
}

static int V4l2GetFormat (PT_VideoDevice ptVideoDevice) {
    if (!ptVideoDevice->iPixelFormat) {
        printf("Device not initialized yet.\n");
        return -1;
    }
    return ptVideoDevice->iPixelFormat;
}

/* Create a VideoOpr structure. */
static T_VideoOpr g_tV4l2VideoOpr = {
    .name           = "v4l2",
    .InitDevice     = V4l2InitDevice,
    .ExitDevice     = V4l2ExitDevice,
    .GetFrame       = V4l2GetFrameForStreaming,
    .GetFormat      = V4l2GetFormat,
    .PutFrame       = V4l2PutFrameForStreaming,
    .StartDevice    = V4l2StartDevice,
    .StopDevice     = V4l2StopDevice,
};

/* Register this structure. */
int V4l2Init (void) {
    printf("register v4l2VideoOpr\n");
    return RegisterVideoOpr(&g_tV4l2VideoOpr);
}


