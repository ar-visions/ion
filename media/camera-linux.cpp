
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <media/camera-linux.hpp>


namespace ion {

static int mapFourccToConstant(const char* fourcc) {
    if (strcmp(fourcc, "MJPEG") == 0) {
        return V4L2_PIX_FMT_MJPEG;
    } else if (strcmp(fourcc, "YUYV") == 0) {
        return V4L2_PIX_FMT_YUYV;
    } else if (strcmp(fourcc, "NV12") == 0) {
        return V4L2_PIX_FMT_NV12;
    }
    // Return a default value or handle unknown formats accordingly.
    return -1; // You can define a constant for an "unknown" format if needed.
}

// return fd if v4l compatible
static int camera_open(const char* dev) {
    int fd = open(dev, O_RDWR);
    if (fd == -1) {
        perror("Failed to open device");
        return -1;
    }

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("Failed to query capabilities");
        close(fd);
        return -1;
    }

    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) && (cap.capabilities & V4L2_CAP_STREAMING))
        return fd;

    close(fd);
    return -1;
}

// Function to set the camera format (e.g., NV12, YUYV, MJPEG).
static bool camera_init(int fd, unsigned int format, int width, int height, int hz) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width =  width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = format;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Failed to set format");
        return false;
    }

    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) == -1) {
        perror("Failed to get capture parameters");
        return false;
    }

    // Modify the frame rate.
    streamparm.parm.capture.timeperframe.numerator = 1;   // Numerator of the frame rate.
    streamparm.parm.capture.timeperframe.denominator = hz;  // Denominator of the frame rate (e.g., 30 for 30 FPS).

    if (ioctl(fd, VIDIOC_S_PARM, &streamparm) == -1) {
        perror("Failed to set capture parameters");
        return false;
    }
    return true;
}

static int camera_select(const char *video_alias) {
    int selected = -1;
    for (int i = 0; i < 10; i++) {
        char dev[20];
        snprintf(dev, sizeof(dev), "%s%d", "/dev/video", i);
        int fd = camera_open(dev);
        if (fd != -1) {
            // query the device capabilities to get the device name
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
                perror("Failed to query device capabilities");
                close(fd);
                break;
            }
            // get device name (card) from v4l2_capability
            const char* device_name = (const char*)cap.card;
            if (strstr(device_name, video_alias)) {
                selected = fd; 
                break;
            }
            close(fd);
        }
    }
    return selected;
}

static bool camera_select_format(int fd, const array<ion::Media> &priority, int *selected_v4l, ion::Media *selected_format) {
    int     format_index = -1;
    int     index = 0;

    struct v4l2_fmtdesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    struct fmt_table_t {
        int             v4l;
        Media           media;
    } fmt_table[3] = {
        { V4L2_PIX_FMT_MJPEG, Media::MJPEG },
        { V4L2_PIX_FMT_YUYV,  Media::YUY2  },
        { V4L2_PIX_FMT_NV12,  Media::NV12  }
    };

    /// find most prioritized format available on this device
    while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
        int v4l = mapFourccToConstant((const char *)desc.description);
        for (int i = 0; i < sizeof(fmt_table) / sizeof(fmt_table_t); i++) {
            fmt_table_t *t = &fmt_table[i];
            if (t->v4l == v4l) {
                if (t->media != Media::undefined) {
                    int index = priority.index_of(t->media);
                    if (index >= 0 && (index < format_index || format_index == -1)) {
                        format_index     = index;
                        *selected_format = t->media;
                        *selected_v4l    = v4l;
                    }
                }
                break;
            }
        }
        index++;
        desc.index = index;
    }
    return format_index >= 0;
}

static bool camera_frame(int fd, u8** buffer_data, size_t *buffer_length, u8* mmap_start) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1)
        return false;
    
    if (buf.memory == V4L2_MEMORY_USERPTR) {     // for V4L2_MEMORY_USERPTR:
        *buffer_data   = (u8*)buf.m.userptr;
        *buffer_length = buf.length;
    } else if (buf.memory == V4L2_MEMORY_MMAP) { // for V4L2_MEMORY_MMAP:
        *buffer_data   = mmap_start + buf.m.offset;
        *buffer_length = buf.length;
    } else
        return false;

    // queue the buffer for the next frame.
    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
        return false;

    return true;
}

Streams camera(
        array<StreamType> stream_types, array<Media> priority,
        str video_alias, str audio_alias, int width, int height) {
    return Streams(stream_types, priority, [stream_types, priority, video_alias, audio_alias, width, height](Streams s) -> void {
        int fd = camera_select(video_alias.data); /// find camera with this name
        assert(fd >= 0);

        int     selected_v4l;
        Media   selected_format;
        assert(camera_select_format(fd, priority, &selected_v4l, &selected_format)); /// query formats and priority by Media order
        assert(camera_init(fd, selected_v4l, width, height, 30)); /// set format, resolution and rate (30 = hard requirement)
        
        size_t  buffer_size = width * height * 4 * 2;
        u8*     mmap_start = (u8*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        assert(mmap_start != MAP_FAILED);

        s.ready();
        while (s->rt->stop) {
            u8*    buffer_data;
            size_t buffer_length;
            if (!camera_frame(fd, &buffer_data, &buffer_length, mmap_start))
                break;
            s.push(MediaBuffer(selected_format, buffer_data, buffer_length));
        }

        close(fd);
        if (!s->ready)
            s.cancel();
    });
}

}