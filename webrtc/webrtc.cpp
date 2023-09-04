#include <webrtc/webrtc.hpp>
#include <media/media.hpp>

using namespace ion;

#ifdef _WIN32
int gettimeofday(struct timeval *tv, struct timezone *tz) {
	if (tv) {
		FILETIME filetime; /* 64bit: 100-nanosecond intervals since January 1, 1601 00:00 UTC */
		ULARGE_INTEGER x;
		ULONGLONG usec;
		static const ULONGLONG epoch_offset_us =
		    11644473600000000ULL; /* microseconds betweeen Jan 1,1601 and Jan 1,1970 */

#if _WIN32_WINNT >= _WIN32_WINNT_WIN8
		GetSystemTimePreciseAsFileTime(&filetime);
#else
		GetSystemTimeAsFileTime(&filetime);
#endif
		x.LowPart = filetime.dwLowDateTime;
		x.HighPart = filetime.dwHighDateTime;
		usec = x.QuadPart / 10 - epoch_offset_us;
		tv->tv_sec = time_t(usec / 1000000ULL);
		tv->tv_usec = long(usec % 1000000ULL);
	}
	if (tz) {
		TIME_ZONE_INFORMATION timezone;
		GetTimeZoneInformation(&timezone);
		tz->tz_minuteswest = timezone.Bias;
		tz->tz_dsttime = 0;
	}
	return 0;
}
#endif

void Service::mounted() {
	state->service = webrtc::service(state->url, state->on_message);
}

/// contextual class instances are safely allocated until the data is not;
/// we dont mount and then free the context leaving the data, thats possible but we leave the context open with data
void VideoStream::mounted() {
	VideoSink *video_sink = context<VideoSink>("video_sink"); /// send to nearest video sink, we think.
	assert(video_sink);

	state->service = async {1, [this, video_sink](runtime *proc, int i) -> mx {
		ion::image img { path("test_image.png") };
		doubly<mx> cache;
		int frames = 0;
		h264e {
			/// one might imagine that when connected to a vulkan device you would just lock a mutex, grab a frame and place
			/// we need to encode the audio in this function too; its complex to manage two streams separately
			[&](i64 frame) -> yuv420 {
				return frames++ == 512 ? null : img;
			},
			[&](mx nalu) -> bool {
				(*video_sink)(nalu);
				return true;
			}
		};
		return frames;
	}};
}

