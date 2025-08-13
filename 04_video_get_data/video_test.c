#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>    // for videodevv2.h
#include <linux/videodev2.h>
#include <poll.h>

/* Usage: ./video_test </dev/videoX> 
 * Enable camera to capture images and save them on current directory as .jpg.
 */

int main (int argc, char *argv[]) {
    int fd;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum fsenum;
    int fmt_index = 0;
    int fram_index;
    int i;
    void *bufs[32];
    int buf_cnt;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct pollfd fds[1];
    char filename[32];
    int file_cnt = 0;

    if (argc != 2) {
        printf("Usage: %s </dev/videoX>\n", argv[0]);
        close(fd);
        return -1; // Invalid argument count
    }

    /* open */
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("Error opening video device %s\n", argv[1]);
        close(fd);
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

    /* Query capability */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(struct v4l2_capability));
    if (0 == ioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
            printf("Device %s does not support video capture\n", argv[1]);
            close(fd);
            return -1; // Device does not support video capture
        }
        if ((cap.capabilities & V4L2_CAP_STREAMING) == 0) {
            printf("Device %s does not support streaming\n", argv[1]);
            close(fd);
            return -1; // Device does not support streaming
        }
    } else {
        perror("Failed to query capabilities");
        close(fd);
        return -1; // Failed to query capabilities
    }

    /* Set format */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1024;
    fmt.fmt.pix.height = 768;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; 
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == 0) {
        printf("Set resolution to %d x %d, pixel format %s\n",
               fmt.fmt.pix.width, fmt.fmt.pix.height,
               (char *)&fmt.fmt.pix.pixelformat);
    } else {
        perror("Failed to set format");
        close(fd);
        return -1;
    }

    /* Request buffers */
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(rb));
    rb.count = 32;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &rb) == 0) {
        /* Mmap the buffers */
        buf_cnt = rb.count;
        for (i = 0; i < rb.count; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.index = i;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == 0) {
                bufs[i] = mmap(0,
                                buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                buf.m.offset);
                if (bufs[i] == MAP_FAILED) {
                    perror("Unable to map buffer.");
                    close(fd);
                    return -1;
                }
            } else {
                perror("Unable to query buffer.");
                return -1;
            }
        }
    } else {
        perror("Failed to request buffers");
        close(fd);
        return -1; // Failed to request buffers
    }
    printf("Requested %d buffers for video capture.\n", buf_cnt);

    /* Queue all buffers to free linked list. */
    for (i = 0; i < buf_cnt; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            perror("Unable to queue buffer"); 
            close(fd);
            return -1;
        }
    }
    printf("Queue buffers success.\n");

    /* Enable camera. */
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        perror("Unable to start capture.");
        close(fd);
        return -1;
    }
    printf("Start to capture.\n");

    while (1) {
        /* Poll */
        memset(&fds, 0, sizeof(fds));
        fds[0].fd = fd;
        fds[0].events = POLLIN;
        if (poll(fds, 1, -1) == 1) {
            /* Extract buffer from queue. */
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
                perror("Unable to dequeue buffer.");
                close(fd);
                return -1;
            }
            /* Save the buffer data into file. */
            sprintf(filename, "video_raw_data_%04d.jpg", file_cnt++);
            int fd_file = open(filename, O_RDWR | O_CREAT, 0666);
            if (fd_file < 0) {
                printf("Cannot create file: %s\n", filename);
            }
            write(fd_file, bufs[buf.index], buf.bytesused);
            printf("Saved buffer %d to file %s\n", buf.index, filename);
            close(fd_file);

            /* Queue the buffer. */
            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            perror("Unable to queue buffer"); 
            close(fd);
            return -1;
            }
        }
    }

    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        perror("Unable to stop capture.");
        close(fd);
        return -1;
    }
    printf("Stop to capture.\n");
    close(fd);
    return 0; // Success
}
