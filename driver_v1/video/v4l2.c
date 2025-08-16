#include <config.h>
#include <video_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define NB_BUFFER 4

static int g_aiSupportedFromats[] = {
    V4L2_PIX_FMT_YUYV,
    V4L2_PIX_FMT_MJPEG,
    V4L2_PIX_FMT_RGB565,
    V4L2_PIX_FMT_RGB24,
    V4L2_PIX_FMT_RGB32
};

static int isSupportedFormat(int iFormat) {
    int i;
    for (i = 0; i < sizeof(g_aiSupportedFromats) / sizeof(int); i++) {
        if (g_aiSupportedFromats[i] == iFormat) {
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
    struct v4l2_format tFmtDesc;
    struct v4l2_format tFmt;
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
    memset(&tV4l2Cap, 0, sizeof(cap));
    iError = ioctl(iFd, VIDIOC_QUERYCAP, &tV4l2Cap);
    if (iError) {
        DBG_PRINTF("Failed to query video device capabilities.\n");
        goto err_exit;
    }
    if (tV4l2Cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        DBG_PRINTF("Device %s support video capture.\n", strDevName);
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
    if (ptVideoDevice->iPixelFormat == 0) {
        DBG_PRINTF("No supported pixel format found.\n");
        goto err_exit;
    }

    /* Set format */
    GetDispResolution(&iLcdWidth, &iLcdHeight, &iLcdBpp);

    memset(&tFmt, 0, sizeof(tFmt));
    tFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tFmt.fmt.pix.width = ptVideoDevice->iWidth;
    tFmt.fmt.pix.height = ptVideoDevice->iHeight;
    tFmt.fmt.pix.pixelformat = ptVideoDevice->iPixelFormat;
    tFmt.fmt.pix.field = V4L2_FIELD_ANY; 

    /* If the driver can't support some parameters such as resolution, 
     * it automatically adjust them and send to application
     */
    iError = ioctl(iFd, VIDIOC_S_FMT, &tFmt);
    if (iError) {
        DBG_PRINTF("Failed to set video format.\n");
        goto err_exit;
    }
    ptVideoDevice->iWidth = tFmt.fmt.pix.width;
    ptVideoDevice->iHeight = tFmt.fmt.pix.height;

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
            ptVideoDevice->pucVideoBuf[i] = mmap(0,
                            tV4l2Buf.length, PROT_READ, MAP_SHARED, iFd,
                            tV4l2Buf.m.offset);
            if (ptVideoDevice->ptVideoBufs[i] == MAP_FAILED) {
                DBG_PRINTF("Unable to map buffer %d.\n", i);
                goto err_exit;
            }
        }
    } else if (tV4l2Cap.capabilities & V4L2_CAP_READWRITE) {
        /* For read/write, we don't need to mmap buffers */
        ptVideoDevice->ivideoBufCnt = 1;
        ptVideoDevice->iVideoBufMaxLen = ptVideoDevice->iWidth * ptVideoDevice->iHeight * 4; // Assuming RGB32 format
        ptVideoDevice->pucVideoBufs[0]  = malloc(ptVideoDevice->iVideoBufMaxLen);
    }

    /* Queue buffers */
    memset(&tV4l2Buf, 0, sizeof(tV4l2Buf));
    for (i = 1; i < ptVideoDevice->iVideoBufCnt; i++) {
        tV4l2Buf.index = i;
        tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tV4l2Buf.memory = V4L2_MEMORY_MMAP;
        iError = ioctl(iFd, VIDIOC_QBUF, &tV4l2Buf);
        if (iError) {
            DBG_PRINTF("Unable to queue buffer %d.\n", i);
            goto err_exit;
        }
    }

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
}

static int V4l2GetFrame (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);

static int V4l2PutFrame (PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);

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

/* Create a VideoOpr structure. */
static T_VideoOpr g_tV4l2VideoOpr = {
    .name           = "v4l2",
    .InitDevice     = V4l2InitDevice,
    .ExitDevice     = V4l2ExitDevice,
    .GetFrame       = V4l2GetFrame,   
    .PutFrame       = V4l2PutFrame,
    .StartDevice    = V4l2StartDevice,
    .StopDevice     = V4l2StopDevice,
};

/* Register this structure. */
int V4l2Init (void) {
    int iError;

    iError = RegisterVideoOpr(&g_tV4l2VideoOpr);
    if (iError) {
        printf("Register V4L2 video operation failed.\n");
        return -1;
    }

    printf("V4L2 video operation registered successfully.\n");
    return 0;
}


