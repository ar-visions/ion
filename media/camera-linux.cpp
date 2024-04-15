
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <getopt.h>             /* getopt_long() */
#include <errno.h>

#include <alsa/asoundlib.h>

#include <media/camera.hpp>

namespace ion {

#define CLEAR(x) memset(&(x), 0, sizeof(x))

enum io_method {
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
};

struct Buffer {
	void   *start;
	size_t  length;
};

struct camera_t {
    char             dev_name[128];
    char             alias[128];
    enum io_method   io = IO_METHOD_MMAP;
    ion::Media       video_format;
    int              width = 640, height = 360, hz = 60;
    int              fd = -1;
    Buffer          *buffers;
    unsigned int     n_buffers;
    int		         out_buf;
    int              format = V4L2_PIX_FMT_YUYV;
    lambda<void(camera_t*, mx&)> process_fn;

    bool select(const char *video_alias);

    operator bool() {
        return fd >= 0;
    }
};

static int mapFourccToConstant(const char* fourcc) {
    if (strncmp(fourcc, "MJPEG", 5) == 0) {
        return V4L2_PIX_FMT_MJPEG;
    } else if (strncmp(fourcc, "YUYV", 4) == 0) {
        return V4L2_PIX_FMT_YUYV;
    } else if (strncmp(fourcc, "NV12", 4) == 0) {
        return V4L2_PIX_FMT_NV12;
    }
    // Return a default value or handle unknown formats accordingly.
    return -1; // You can define a constant for an "unknown" format if needed.
}

bool camera_t::select(const char *video_alias) {
    int selected = -1;
    for (int i = 0; i < 10; i++) {
        char dev[20];
        snprintf(dev, sizeof(dev), "%s%d", "/dev/video", i);
        int fd = open(dev, O_RDWR);
        if (fd != -1) {
            // query the device capabilities to get the device name
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
                perror("Failed to query device capabilities");
                close(fd);
                break;
            }
            close(fd);
            // get device name (card) from v4l2_capability
            const char* alias_name = (const char*)cap.card;
            if (strstr(alias_name, video_alias)) {
                selected = fd; 
                strcpy(dev_name,  dev);
                strcpy(alias,     alias_name);
                return true;
            }
        }
    }
    return false;
}

static bool camera_select_format(camera_t *cam, const Array<ion::Media> &priority, int *selected_v4l, ion::Media *video_format) {
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
    while (ioctl(cam->fd, VIDIOC_ENUM_FMT, &desc) == 0) {
        int v4l = mapFourccToConstant((const char *)desc.description);
        for (int i = 0; i < sizeof(fmt_table) / sizeof(fmt_table_t); i++) {
            fmt_table_t *t = &fmt_table[i];
            if (t->v4l == v4l) {
                int index = priority.index_of(t->media);
                if (index >= 0 && (index < format_index || format_index == -1)) {
                    format_index     = index;
                    *video_format = t->media;
                    *selected_v4l    = v4l;
                }
                break;
            }
        }
        index++;
        desc.index = index;
    }
    cam->video_format = *video_format;
    return format_index >= 0;
}

static void errno_exit(const char *s) {
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, unsigned long int request, void *arg) {
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static void process_image(camera_t *cam, mx &data) {
    cam->process_fn(cam, data);
}

static int read_frame(camera_t *cam)
{
	struct v4l2_buffer buf;
	unsigned int i;

	switch (cam->io) {
	case IO_METHOD_MMAP: {
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (-1 == xioctl(cam->fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
                usleep(10);
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("VIDIOC_DQBUF");
			}
		}
		assert(buf.index < cam->n_buffers);

        Array<u8> buffer(buf.bytesused / sizeof(u8));
        memcpy(buffer.window, cam->buffers[buf.index].start, buf.bytesused);
        buffer.set_size(buf.bytesused / sizeof(u8));
        process_image(cam, buffer);
        
		if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");

		break;
    }
    default:
        console.fault("not supported");
        break;
	}
	return 1;
}

static void stop_capturing(camera_t *cam)
{
	enum v4l2_buf_type type;
	switch (cam->io) {
	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(cam->fd, VIDIOC_STREAMOFF, &type))
			errno_exit("VIDIOC_STREAMOFF");
		break;
	}
}

static void start_capturing(camera_t *cam)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (cam->io) {
	case IO_METHOD_MMAP:
		for (i = 0; i < cam->n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(cam->fd, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < cam->n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long)cam->buffers[i].start;
			buf.length = cam->buffers[i].length;

			if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(cam->fd, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");
		break;
	}
}

static void uninit_device(camera_t *cam)
{
	unsigned int i;

	switch (cam->io) {
	case IO_METHOD_MMAP:
		for (i = 0; i < cam->n_buffers; ++i)
			if (-1 == munmap(cam->buffers[i].start, cam->buffers[i].length))
				errno_exit("munmap");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < cam->n_buffers; ++i)
			free(cam->buffers[i].start);
		break;
	}

	free(cam->buffers);
}

static void init_read(camera_t *cam, unsigned int buffer_size)
{
	cam->buffers = (Buffer*)calloc(1, sizeof(*cam->buffers));

	if (!cam->buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	cam->buffers[0].length = buffer_size;
	cam->buffers[0].start = malloc(buffer_size);

	if (!cam->buffers[0].start) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
}

static void init_mmap(camera_t *cam)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				 "memory mapping\n", cam->dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
			 cam->dev_name);
		exit(EXIT_FAILURE);
	}

	cam->buffers = (Buffer*)calloc(req.count, sizeof(*cam->buffers));

	if (!cam->buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (cam->n_buffers = 0; cam->n_buffers < req.count; ++cam->n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = cam->n_buffers;

		if (-1 == xioctl(cam->fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		cam->buffers[cam->n_buffers].length = buf.length;
		cam->buffers[cam->n_buffers].start =
			mmap(NULL /* start anywhere */,
			      buf.length,
			      PROT_READ | PROT_WRITE /* required */,
			      MAP_SHARED /* recommended */,
			      cam->fd, buf.m.offset);

		if (MAP_FAILED == cam->buffers[cam->n_buffers].start)
			errno_exit("mmap");
	}
}

static void init_userp(camera_t *cam, unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count  = 4;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				 "user pointer i/o\n", cam->dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	cam->buffers = (Buffer*)calloc(4, sizeof(*cam->buffers));

	if (!cam->buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (cam->n_buffers = 0; cam->n_buffers < 4; ++cam->n_buffers) {
		cam->buffers[cam->n_buffers].length = buffer_size;
		cam->buffers[cam->n_buffers].start  = malloc(buffer_size);

		if (!cam->buffers[cam->n_buffers].start) {
			fprintf(stderr, "Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}
}

static void init_device(camera_t *cam, int format, int width, int height, int hz)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;

    cam->format = format;
    cam->width  = width;
    cam->height = height;
    cam->hz     = hz;

	if (-1 == xioctl(cam->fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",
				 cam->dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",
			 cam->dev_name);
		exit(EXIT_FAILURE);
	}

	switch (cam->io) {
	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "%s does not support streaming i/o\n",
				 cam->dev_name);
			exit(EXIT_FAILURE);
		}
		break;
	}

	CLEAR(fmt);
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = cam->format;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    /* Note VIDIOC_S_FMT may change width and height. */
    if (-1 == xioctl(cam->fd, VIDIOC_S_FMT, &fmt))
        errno_exit("VIDIOC_S_FMT");

    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(cam->fd, VIDIOC_G_PARM, &streamparm) == -1) {
        perror("Failed to get capture parameters");
        return;
    }

    // Modify the frame rate.
    streamparm.parm.capture.timeperframe.numerator = 1;   // Numerator of the frame rate.
    streamparm.parm.capture.timeperframe.denominator = hz;  // Denominator of the frame rate (e.g., 30 for 30 FPS).

    if (ioctl(cam->fd, VIDIOC_S_PARM, &streamparm) == -1) {
        perror("Failed to set capture parameters");
        return;
    }

	switch (cam->io) {
	case IO_METHOD_MMAP:
		init_mmap(cam);
		break;

	case IO_METHOD_USERPTR:
		init_userp(cam, fmt.fmt.pix.sizeimage);
		break;
	}
}

static void close_device(camera_t *cam) {
	if (-1 == close(cam->fd))
		errno_exit("close");
	cam->fd = -1;
}

static void open_device(camera_t *cam) {
	struct stat st;
	if (-1 == stat(cam->dev_name, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n",
			 cam->dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", cam->dev_name);
		exit(EXIT_FAILURE);
	}
	cam->fd = open(cam->dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
	if (-1 == cam->fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			 cam->dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

struct audio_t {
    str        dev_name;
    Media      audio_format;
    u32        sample_rate = 48000;
    int        card_id;
    int        sub_id;
    int        channels = 1;
    snd_pcm_t *handle;
    u8        *buffer; /// handle if its S16_LE or FLOAT_LE
    snd_pcm_uframes_t frame_size;
    int        unit_size;

    operator bool() {
        return dev_name;
    }

    bool read_frames(u8** dst) {
        int read_cursor = 0;
        int remaining   = frame_size / channels;
        while (read_cursor < frame_size) {
            int r = snd_pcm_readi(handle, &buffer[read_cursor], remaining);
            if (r > 0) {
                //printf("audio id: %d\n", f->audio->id);
                //FILE *ftest = fopen("/home/kalen/test3.raw", "a");
                //fwrite(&buffer[read_cursor], r * channels * sizeof(short), 1, ftest);
                //fclose(ftest);

                remaining   -= r;
                read_cursor += r * channels;
            }
            else if (r == -EAGAIN || (r >= 0 && (size_t)r < remaining)) {
			    snd_pcm_wait(handle, 1000);
		    }
            else if (r == -EPIPE) {
                xrun(); /// ?
            }
            else {
                *dst = null;
                return false;
            }
        }
        *dst = buffer;
        return true;
    }

    /* I/O error handler */
    void xrun(void)
    {
        snd_pcm_status_t *status;
        int res;
        
        snd_pcm_status_alloca(&status);
        if ((res = snd_pcm_status(handle, status))<0) {
            printf("status error: %s", snd_strerror(res));
            exit(EXIT_FAILURE);
        }
        if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
            struct timeval now, diff, tstamp;
            gettimeofday(&now, 0);
            snd_pcm_status_get_trigger_tstamp(status, &tstamp);
            timersub(&now, &tstamp, &diff);
            if ((res = snd_pcm_prepare(handle)) < 0) {
                printf("xrun: prepare error: %s", snd_strerror(res));
                exit(EXIT_FAILURE);
            }
            return;		/* ok, data should be accepted again */
        }
        if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
            printf("capture stream format change? attempting recover...\n");
            if ((res = snd_pcm_prepare(handle))<0) {
                printf("xrun(DRAINING): prepare error: %s", snd_strerror(res));
                exit(EXIT_FAILURE);
            }
            return;
        }

        printf("read/write error, state = %s", snd_pcm_state_name(snd_pcm_status_get_state(status)));
        exit(EXIT_FAILURE);
    }

    bool record(int hz, int chans) {
        frame_size = sample_rate * chans / hz;
        channels = chans;
        snd_pcm_hw_params_t *params;
        int dir;
        int err;

        /// open PCM device for recording (capture)
        if ((err = snd_pcm_open(&handle, dev_name.data, SND_PCM_STREAM_CAPTURE, 0)) < 0) { // SND_PCM_STREAM_CAPTURE
            console.fault("[audio_t] cannot open input device: {0}", { dev_name });
            return false;
        }

        /// allocate a hardware parameters object
        snd_pcm_hw_params_alloca(&params);

        /// fill it in with default values
        snd_pcm_hw_params_any(handle, params);

        if ((err = snd_pcm_hw_params_set_access (handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            console.fault("cannot set access type ({0})", { snd_strerror (err) });
        }

        /// set the desired hardware parameters
        snd_pcm_format_t snd_format;
        snd_pcm_hw_params_get_format(params, &snd_format);
        if (snd_format == SND_PCM_FORMAT_FLOAT_LE) {
            unit_size = 4;
            audio_format = Media::PCMf32;
        } else {
            assert(SND_PCM_FORMAT_S16_LE);
            unit_size = 2;
            audio_format = Media::PCM;
        }
        
        /// mics have a set number of sources, and we dont want to reduce that (use default = all)
        snd_pcm_hw_params_get_channels(params, (u32*)&channels);

        assert(snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, &dir) >= 0);
        assert(sample_rate == 48000);
        int prior = frame_size;
        snd_pcm_hw_params_set_period_size_near(handle, params, &frame_size, &dir);
        assert(prior == frame_size);

        snd_pcm_hw_params_set_buffer_size(handle, params, frame_size * 64);

        /// write the parameters to the driver
        if ((err = snd_pcm_hw_params(handle, params)) < 0) {
            console.fault("[audio_t] cannot set audio parameters: {0}", { snd_strerror(err) });
            return false;
        }

        /// prepare the device.
        snd_pcm_prepare(handle);

        /// use a buffer large enough to hold one period
        buffer = (u8*)calloc(unit_size, 64 * frame_size);  /// 2 bytes / sample for S16_LE format
        return true;
    }

    bool select(const char *search_str) {
        int err;
        snd_ctl_t *handle;
        snd_ctl_card_info_t *info;
        bool found = false;

        sample_rate = 48000;
        snd_ctl_card_info_malloc(&info);
        card_id = -1;
        while (!found && snd_card_next(&card_id) >= 0) {
            char devname[128];
            sprintf(devname, "hw:%d", card_id);
            err = snd_ctl_open(&handle, devname, 0);
            if (err < 0) {
                console.fault("ERROR: snd_ctl_open, card={0}: {1}", { card_id, snd_strerror(err) });
            } else {
                // get card information
                if ((err = snd_ctl_card_info(handle, info)) < 0) {
                    snd_ctl_close(handle);
                    console.fault("[audio_t] cannot get card info for card {0}: {1}", { card_id, snd_strerror(err) });
                }
                // check if the card name or ID contains the desired substring
                const char *card_name_str = snd_ctl_card_info_get_name(info);
                const char *card_id_str   = snd_ctl_card_info_get_id(info);
                if (strstr(card_name_str, search_str) != NULL || strstr(card_id_str, search_str) != NULL) {
                    sub_id   = 0;
                    dev_name = fmt { "hw:{0},{1}", { card_id, sub_id }};
                    found    = true;
                    console.log("[audio_t] found card {0}", { dev_name });
                }
                snd_ctl_close(handle);
            }
        }
        snd_ctl_card_info_free(info);
        assert(found);
        return found;
    }
};


MStream camera(
        Array<StreamType> stream_types, Array<Media> priority,
        str video_alias, str audio_alias, int width, int height) {
    return MStream(stream_types, priority, [stream_types, priority, video_alias, audio_alias, width, height](MStream s) -> void {
        
        /// PnP device (generic USB mic, widely avail OEM) USB 2.1 device
        /// output the packet lengths and total size along with average per second.
        /// it should be 48,000 * 1chan * 4float;
        /// each sample read at 30hz should average at 
        ///     48000S/30HZ*1C*4F = 6400

        camera_t cam;
        cam.io = IO_METHOD_MMAP;//IO_METHOD_USERPTR;//IO_METHOD_MMAP;
        bool sel = cam.select(video_alias.data);
        assert(sel); /// find camera with this name
        open_device(&cam);

        int     selected_v4l;
        Media   video_format;
        bool fmt = camera_select_format(&cam, priority, &selected_v4l, &video_format);
        assert(fmt); /// query formats and priority by Media order
        s.set_video_format(video_format);

        cam.process_fn = [&](camera_t *cam, mx &data) {
            s.push_video(data);
        };

        const int hz = 30;

        s->w  = width;
        s->h  = height;
        init_device(&cam, selected_v4l, width, height, hz);
        start_capturing(&cam);

        audio_t  audio;
        StreamType aud = StreamType::Audio;
        if (stream_types.index_of(aud) >= 0) {
            audio.select(audio_alias.data);
            audio.record(hz, 1);
        }
        
        s.set_info(width, height, hz, audio ? audio.channels : 0);
        s.ready();
		i64 t = millis();

        int ready_value = audio ? 2 : 1;
        int is_ready    = 0;

        /// if we need to read the first frame, then it would be ideal to break up the functions into singular calls from this thread
        /// then we set the origin frame time, then create the threads which should not take more than 1ms

        /// read video frame; initiate as ready once its read the
        async([&](runtime *rt, int index) -> mx {
            is_ready++;
            while (is_ready != ready_value) { usleep(1); }

            bool stop  = s->rt->stop;
            while (!stop) {
                if (read_frame(&cam) < 0)
                    break;
                stop = s->rt->stop;
            }
            return true;
        });

        /// do not clear frames after writing
        /// clear audio yes, but not video; we will reuse the video frame
        if (audio) {
            std::shared_mutex mtx;

            /// feed frames into mx queue
            async([&s, &audio, &mtx, &ready_value, &is_ready](runtime *rt, int index) -> mx {
                is_ready++;
                while (is_ready != ready_value) { usleep(1); }

                while (!s->rt->stop) {
                    /// read audio (short or float based; next layer in pipeline performs conversions for the user)
                    u8 *audio_frame = null;
                    if (!audio.read_frames(&audio_frame)) /// frame size is input size of channel count * samples_sec / rate
                        break;
                    
                    mtx.lock();
                    if (audio.audio_format == Media::PCMf32) {
                        Array<float> frame(audio.frame_size);
                        memcpy(frame.window, audio_frame, frame.reserve() * audio.unit_size);
                        frame.set_size(frame.reserve());
                        s.push_audio(frame);
                    } else {
                        Array<short> frame(audio.frame_size);
                        memcpy(frame.window, audio_frame, frame.reserve() * audio.unit_size);
                        frame.set_size(frame.reserve());
                        s.push_audio(frame);
                    }
                    mtx.unlock();

                }
                return true;
            });
        }

        if (cam || audio)
            while (!s->rt->stop) {
                usleep(10000);
            };
    
        if (cam) {
            stop_capturing(&cam);
            uninit_device(&cam);
            close_device(&cam);
        }

        if (!s->ready)
            s.cancel();
    });
}

}