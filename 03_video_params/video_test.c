#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>    // for videodevv2.h
#include <linux/videodev2.h>

/* ./video_test </dev/videoX> 
** List all supported pixel formats and frame sizes
*/

int main(int argc, char *argv[]) {
    int fd;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum fsenum;
    int fmt_index = 0;
    int fram_index;

    if (argc != 2) {
        printf("Usage: %s </dev/videoX>\n", argv[0]);
        return -1; // Invalid argument count
    }

    /* open */
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Error opening video device %s\n", argv[1]);
        return -1; // Failed to open video device
    }

    while (1) {
        /* Enumerate format*/
        fmtdesc.index = fmt_index;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(0 != ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            break; // No more formats to enumerate
        }
        
        fram_index = 0;
        while (1) {
            /* Enumerate the frame size*/
            memset(&fsenum, 0, sizeof(struct v4l2_frmsizeenum));
            fsenum.pixel_format = fmtdesc.pixelformat;
            fsenum.index = fram_index;
            if(0 == ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsenum)) {
                printf("Format: %s, %d, framesize %d: %d x %d\n",
                       fmtdesc.description, fsenum.pixel_format,
                       fram_index, fsenum.discrete.width, fsenum.discrete.height);
            } 
            else {
                break; // No more frame sizes for this format
            }
        fram_index++;
        }

        fmt_index++;
    }

    return 0; // Success
}

