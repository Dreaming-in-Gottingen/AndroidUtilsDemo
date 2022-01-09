/* Base on Android11 */

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <termios.h>
#include <unistd.h>

#define LOG_TAG "MC_DEMO"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <utils/Errors.h>
#include <utils/SystemClock.h>
#include <utils/Timers.h>
#include <utils/Trace.h>

#include <gui/Surface.h>
#include <media/MediaCodecBuffer.h>
#include <media/openmax/OMX_IVCommon.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/foundation/AMessage.h>
#include <mediadrm/ICrypto.h>

using android::ALooper;
using android::AMessage;
using android::AString;
using android::IBinder;
using android::MediaCodec;
using android::MediaCodecBuffer;
using android::ProcessState;
using android::String8;
using android::Vector;
using android::sp;
using android::status_t;

using android::INVALID_OPERATION;
using android::NAME_NOT_FOUND;
using android::NO_ERROR;
using android::UNKNOWN_ERROR;


static const uint32_t kMinBitRate = 100000;         // 0.1Mbps
static const uint32_t kMaxBitRate = 200 * 1000000;  // 200Mbps
static const uint32_t kFallbackWidth = 1280;        // 720p
static const uint32_t kFallbackHeight = 720;
static const char* kMimeTypeAvc = "video/avc";

// Command-line parameters.
static bool gVerbose = false;           // chatty on stdout
static AString gCodecName = "";         // codec name override
static bool gSizeSpecified = false;     // was size explicitly requested?
static uint32_t gVideoWidth = 0;        // default width+height
static uint32_t gVideoHeight = 0;
static uint32_t gBitRate = 4000000;     // 4Mbps
static uint32_t gBframes = 0;
static char* gInFileName;               // yuv
static char* gOutFileName;              // bitstream

// Set by signal handler to stop recording.
static volatile bool gStopRequested = false;


// Previous signal handler state, restored after first hit.
static struct sigaction gOrigSigactionINT;
static struct sigaction gOrigSigactionHUP;


/*
 * Catch keyboard interrupt signals.  On receipt, the "stop requested"
 * flag is raised, and the original handler is restored (so that, if
 * we get stuck finishing, a second Ctrl-C will kill the process).
 */
static void signalCatcher(int signum)
{
    gStopRequested = true;
    switch (signum) {
    case SIGINT:
    case SIGHUP:
        sigaction(SIGINT, &gOrigSigactionINT, NULL);
        sigaction(SIGHUP, &gOrigSigactionHUP, NULL);
        break;
    default:
        abort();
        break;
    }
}

/*
 * Configures signal handlers.  The previous handlers are saved.
 *
 * If the command is run from an interactive adb shell, we get SIGINT
 * when Ctrl-C is hit.  If we're run from the host, the local adb process
 * gets the signal, and we get a SIGHUP when the terminal disconnects.
 */
static status_t configureSignals() {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signalCatcher;
    if (sigaction(SIGINT, &act, &gOrigSigactionINT) != 0) {
        status_t err = -errno;
        fprintf(stderr, "Unable to configure SIGINT handler: %s\n",
                strerror(errno));
        return err;
    }
    if (sigaction(SIGHUP, &act, &gOrigSigactionHUP) != 0) {
        status_t err = -errno;
        fprintf(stderr, "Unable to configure SIGHUP handler: %s\n",
                strerror(errno));
        return err;
    }
    signal(SIGPIPE, SIG_IGN);
    return NO_ERROR;
}

/*
 * Configures and starts the MediaCodec encoder.
 */
static status_t prepareEncoder(sp<MediaCodec>* pCodec)
{
    status_t err;

    if (gVerbose) {
        printf("Configuring recorder for %dx%d %s at %.2fMbps\n",
                gVideoWidth, gVideoHeight, kMimeTypeAvc, gBitRate / 1000000.0);
        fflush(stdout);
    }

    sp<AMessage> format = new AMessage;
    format->setInt32(KEY_WIDTH, gVideoWidth);
    format->setInt32(KEY_HEIGHT, gVideoHeight);
    format->setString(KEY_MIME, kMimeTypeAvc);
    format->setInt32(KEY_COLOR_FORMAT, OMX_COLOR_FormatYUV420Planar);
    format->setInt32(KEY_BIT_RATE, gBitRate);
    format->setFloat(KEY_FRAME_RATE, 30);
    format->setInt32(KEY_I_FRAME_INTERVAL, 30);
    format->setInt32(KEY_MAX_B_FRAMES, gBframes);
    if (gBframes > 0) {
        format->setInt32(KEY_PROFILE, AVCProfileMain);
        format->setInt32(KEY_LEVEL, AVCLevel41);
    }

    sp<android::ALooper> looper = new android::ALooper;
    looper->setName("mediacodec_looper");
    looper->start();
    ALOGV("Creating codec");
    sp<MediaCodec> codec;
    if (gCodecName.empty()) {
        codec = MediaCodec::CreateByType(looper, kMimeTypeAvc, true);
        if (codec == NULL) {
            fprintf(stderr, "ERROR: unable to create %s codec instance\n",
                    kMimeTypeAvc);
            return UNKNOWN_ERROR;
        }
        ALOGD("MediaCodec::CreateByType(%s) => comp_name=%s", kMimeTypeAvc, gCodecName.c_str());
    } else {
        codec = MediaCodec::CreateByComponentName(looper, gCodecName);
        if (codec == NULL) {
            fprintf(stderr, "ERROR: unable to create %s codec instance\n",
                    gCodecName.c_str());
            return UNKNOWN_ERROR;
        }
        ALOGD("MediaCodec::CreateByComponentName(%s)", gCodecName.c_str());
    }
    codec->getName(&gCodecName);

    err = codec->configure(format, NULL, NULL,
            MediaCodec::CONFIGURE_FLAG_ENCODE);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to configure %s codec at %dx%d (err=%d)\n",
                kMimeTypeAvc, gVideoWidth, gVideoHeight, err);
        codec->release();
        return err;
    }

    ALOGD("Starting codec");
    err = codec->start();
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to start codec (err=%d)\n", err);
        codec->release();
        return err;
    }

    ALOGV("Codec prepared");
    *pCodec = codec;
    return 0;
}

/*
 * Runs the MediaCodec encoder, sending the output to the rawOutfp.
 * The input frames are coming from local file.
 */
static status_t runEncoder(const sp<MediaCodec>& encoder, FILE* rawInFp, FILE* rawOutFp)
{
    static int kTimeout = 250000;   // be responsive on signal
    status_t err;
    uint32_t debugNumFrames = 0;
    int64_t startWhenNsec = systemTime(CLOCK_MONOTONIC);
    bool firstFrame = true;

    assert(rawOutFp == NULL);

    Vector<sp<MediaCodecBuffer> > in_buffers;
    err = encoder->getInputBuffers(&in_buffers);
    if (err != NO_ERROR) {
        fprintf(stderr, "Unable to get input buffers (err=%d)\n", err);
        return err;
    }

    Vector<sp<MediaCodecBuffer> > out_buffers;
    err = encoder->getOutputBuffers(&out_buffers);
    if (err != NO_ERROR) {
        fprintf(stderr, "Unable to get output buffers (err=%d)\n", err);
        return err;
    }
    if (gVerbose)
        fprintf(stderr, "MediaCodecBuffer in_buf_cnt=%zu, out_buf_cnt=%zu\n",
                        in_buffers.size(), out_buffers.size());

    int yuv_sz = gVideoWidth*gVideoHeight*3/2;

    // Run until we're signaled.
    while (!gStopRequested) {
        size_t bufIndex, offset, size;
        int64_t ptsUsec;
        uint32_t flags;

        if (firstFrame) {
            ATRACE_NAME("first_frame");
            firstFrame = false;
        }

        err = encoder->dequeueInputBuffer(&bufIndex);
        if (err == NO_ERROR) {
            if (bufIndex >=0) {
                err = fread(in_buffers[bufIndex]->data(), 1, yuv_sz, rawInFp);
                if (err != yuv_sz) {
                    fprintf(stderr, "rawInFp EOF found!\n");
                    //gStopRequested = true;
                    encoder->queueInputBuffer(bufIndex, 0, 0, 0, MediaCodec::BUFFER_FLAG_EOS);
                } else {
                    encoder->queueInputBuffer(bufIndex, 0, yuv_sz, 0, 0);
                }
            } else {
                ALOGD("no input buf found, try to get output buffer!");
            }
        } else {
            ALOGV("dequeueInputBuffer failed with err=%#x, may no enough InBuffer!", err);
        }

        ALOGV("Calling dequeueOutputBuffer");
        err = encoder->dequeueOutputBuffer(&bufIndex, &offset, &size, &ptsUsec, &flags, kTimeout);
        switch (err) {
        case NO_ERROR:
            // got a buffer
            if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) != 0) {
                ALOGV("Got codec config buffer (%zu bytes)", size);
            }
            if (size != 0) {
                ALOGD("Got data in buffer[%zu], size=%zu, flag=%#x, pts=%" PRId64,
                        bufIndex, size, flags, ptsUsec);

                fwrite(out_buffers[bufIndex]->data(), 1, size, rawOutFp);
                if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) == 0) {
                    fflush(rawOutFp);
                }
                debugNumFrames++;
            }
            err = encoder->releaseOutputBuffer(bufIndex);
            if (err != NO_ERROR) {
                fprintf(stderr, "Unable to release output buffer (err=%d)\n", err);
                return err;
            }
            if ((flags & MediaCodec::BUFFER_FLAG_EOS) != 0) {
                ALOGI("Received end-of-stream");
                gStopRequested = true;
            }
            break;
        case -EAGAIN:                       // INFO_TRY_AGAIN_LATER
            ALOGD("Got -EAGAIN, looping");
            break;
        case android::INFO_FORMAT_CHANGED:    // INFO_OUTPUT_FORMAT_CHANGED
            ALOGI("INFO_FORMAT_CHANGED");
            break;
        case android::INFO_OUTPUT_BUFFERS_CHANGED:   // INFO_OUTPUT_BUFFERS_CHANGED
            // Not expected for an encoder; handle it anyway.
            fprintf(stderr, "Encoder buffers changed\n");
            err = encoder->getOutputBuffers(&out_buffers);
            if (err != NO_ERROR) {
                fprintf(stderr, "Unable to get new output buffers (err=%d)\n", err);
                return err;
            }
            break;
        case INVALID_OPERATION:
            fprintf(stderr, "dequeueOutputBuffer returned INVALID_OPERATION\n");
            return err;
        default:
            fprintf(stderr, "Got weird result %d from dequeueOutputBuffer\n", err);
            return err;
        }
    }

    ALOGD("Encoder stopping (req=%d)", gStopRequested);
    if (gVerbose) {
        float dura_sec = 1.0f*(systemTime(CLOCK_MONOTONIC) - startWhenNsec)/(1000*1000*1000L);
        float fps = debugNumFrames / dura_sec;
        printf("Encoder stopping; encoded %u frames in %0.1f seconds, speed=%0.1f fps\n",
                debugNumFrames, dura_sec, fps);
        fflush(stdout);
    }
    return NO_ERROR;
}

/*
 * Raw H.264 byte stream output requested.  Send the output to stdout
 * if desired.  If the output is a tty, reconfigure it to avoid the
 * CRLF line termination that we see with "adb shell" commands.
 */
static FILE* prepareRawOutput(const char* fileName) {
    FILE* rawOutFp = NULL;

    if (strcmp(fileName, "-") == 0) {
        if (gVerbose) {
            fprintf(stderr, "ERROR: verbose output and '-' not compatible");
            return NULL;
        }
        rawOutFp = stdout;
    } else {
        rawOutFp = fopen(fileName, "w");
        if (rawOutFp == NULL) {
            fprintf(stderr, "fopen raw failed: %s\n", strerror(errno));
            return NULL;
        }
    }

    int fd = fileno(rawOutFp);
    if (isatty(fd)) {
        // best effort -- reconfigure tty for "raw"
        ALOGD("raw video output to tty (fd=%d)", fd);
        struct termios term;
        if (tcgetattr(fd, &term) == 0) {
            cfmakeraw(&term);
            if (tcsetattr(fd, TCSANOW, &term) == 0) {
                ALOGD("tty successfully configured for raw");
            }
        }
    }

    return rawOutFp;
}

static inline uint32_t floorToEven(uint32_t num) {
    return num & ~1;
}

/*
 * Main "do work" start point.
 *
 * Configure codec, then starts moving bits around.
 */
static status_t videoEncode()
{
    status_t err;

    // Configure signal handler.
    err = configureSignals();
    if (err != NO_ERROR) return err;

    // Start Binder thread pool.  MediaCodec needs to be able to receive
    // messages from mediaserver.
    sp<ProcessState> self = ProcessState::self();
    self->startThreadPool();

    // Encoder can't take odd number as config
    if (gVideoWidth == 0) {
        gVideoWidth = 1280;
    }
    if (gVideoHeight == 0) {
        gVideoHeight = 720;
    }

    // Configure and start the encoder.
    sp<MediaCodec> encoder;
    err = prepareEncoder(&encoder);
    if (err != NO_ERROR){
        fprintf(stderr, "WARNING: prepareEncoder failed at %dx%d\n",
                gVideoWidth, gVideoHeight);
        return err;
    }

    FILE* rawInFp = fopen(gInFileName, "rb");
    if (rawInFp == NULL) {
        fprintf(stderr, "fopen gInFileName(%s) failed: %s\n", gInFileName, strerror(errno));
        if (encoder != NULL) encoder->release();
        return -1;
    }
    struct stat tmp;
    fstat(fileno(rawInFp), &tmp);
    int frm_cnt = tmp.st_size / (gVideoWidth * gVideoHeight * 3 >> 1);
    if (gVerbose)
        printf("inYuvFile(%s) total frm_cnt=%d\n", gInFileName, frm_cnt);

    FILE* rawOutFp = NULL;
    rawOutFp = fopen(gOutFileName, "wb");
    if (rawOutFp == NULL) {
        fprintf(stderr, "fopen gOutFileName(%s) failed: %s\n", gOutFileName, strerror(errno));
        if (encoder != NULL) encoder->release();
        return -1;
    }

    // Main encoder loop.
    err = runEncoder(encoder, rawInFp, rawOutFp);
    if (err != NO_ERROR) {
        fprintf(stderr, "Encoder failed (err=%d)\n", err);
        // fall through to cleanup
    }

    if (gVerbose) {
        printf("Stopping encoder!\n");
        fflush(stdout);
    }

    // Shut everything down.
    fclose(rawInFp);
    if (rawOutFp != stdout)
        fclose(rawOutFp);
    if (encoder != NULL)
        encoder->release();

    return err;
}

/*
 * Parses a string of the form "1280x720".
 *
 * Returns true on success.
 */
static bool parseWidthHeight(const char* widthHeight, uint32_t* pWidth,
        uint32_t* pHeight) {
    long width, height;
    char* end;

    // Must specify base 10, or "0x0" gets parsed differently.
    width = strtol(widthHeight, &end, 10);
    if (end == widthHeight || *end != 'x' || *(end+1) == '\0') {
        // invalid chars in width, or missing 'x', or missing height
        return false;
    }
    height = strtol(end + 1, &end, 10);
    if (*end != '\0') {
        // invalid chars in height
        return false;
    }

    *pWidth = width;
    *pHeight = height;
    return true;
}

/*
 * Accepts a string with a bare number ("4000000") or with a single-character
 * unit ("4m").
 *
 * Returns an error if parsing fails.
 */
static status_t parseValueWithUnit(const char* str, uint32_t* pValue) {
    long value;
    char* endptr;

    value = strtol(str, &endptr, 10);
    if (*endptr == '\0') {
        // bare number
        *pValue = value;
        return NO_ERROR;
    } else if (toupper(*endptr) == 'M' && *(endptr+1) == '\0') {
        *pValue = value * 1000000;  // check for overflow?
        return NO_ERROR;
    } else {
        fprintf(stderr, "Unrecognized value: %s\n", str);
        return UNKNOWN_ERROR;
    }
}

/*
 * Dumps usage on stderr.
 */
static void usage() {
    fprintf(stderr,
        "Usage: screenrecord [options] <filename>\n"
        "\n"
        "Android screenrecord.  Records the device's display to a .mp4 file.\n"
        "\n"
        "Options:\n"
        "--in-yuv input yuv path\n"
        "    Read from the in_yuv local file as input of MediaCodec.\n"
        "--out-bs output bitstream path\n"
        "    Get bitstream from MediaCodec, then write to out_bs local file.\n"
        "--size WIDTHxHEIGHT\n"
        "    Set the video size, e.g. \"1280x720\".  Default is the device's main\n"
        "    display resolution (if supported), 1280x720 if not.  For best results,\n"
        "    use a size supported by the AVC encoder.\n"
        "--bit-rate RATE\n"
        "    Set the video bit rate, in bits per second.  Value may be specified as\n"
        "    bits or megabits, e.g. '4000000' is equivalent to '4M'.  Default %dMbps.\n"
        "--bframes\n"
        "    Need b frames from MediaCodec, default is false.\n"
        "--verbose\n"
        "    Display interesting information on stdout.\n"
        "--help\n"
        "    Show this message.\n"
        "\n"
        "Recording continues until Ctrl-C is hit or EOF is found from loacal file.\n"
        "\n",
        gBitRate / 1000000
        );
}

/*
 * Parses args and kicks things off.
 */
int main(int argc, char* const argv[]) {
    static const struct option longOptions[] = {
        { "help",               no_argument,        NULL, 'h' },
        { "verbose",            no_argument,        NULL, 'v' },
        { "size",               required_argument,  NULL, 's' },
        { "bit-rate",           required_argument,  NULL, 'b' },
        { "in-yuv",             required_argument,  NULL, 'i' },
        { "out-bs",             required_argument,  NULL, 'o' },
        { "codec-name",         required_argument,  NULL, 'n' },
        { "bframes",            required_argument,  NULL, 'B' },
        { NULL,                 0,                  NULL, 0 }
    };

    while (true) {
        int optionIndex = 0;
        int ic = getopt_long(argc, argv, "", longOptions, &optionIndex);
        if (ic == -1) {
            break;
        }

        switch (ic) {
        case 'h':
            usage();
            return 0;
        case 'v':
            gVerbose = true;
            break;
        case 's':
            if (!parseWidthHeight(optarg, &gVideoWidth, &gVideoHeight)) {
                fprintf(stderr, "Invalid size '%s', must be width x height\n",
                        optarg);
                return 2;
            }
            if (gVideoWidth == 0 || gVideoHeight == 0) {
                fprintf(stderr,
                    "Invalid size %ux%u, width and height may not be zero\n",
                    gVideoWidth, gVideoHeight);
                return 2;
            }
            gSizeSpecified = true;
            break;
        case 'b':
            if (parseValueWithUnit(optarg, &gBitRate) != NO_ERROR) {
                return 2;
            }
            if (gBitRate < kMinBitRate || gBitRate > kMaxBitRate) {
                fprintf(stderr, "Bit rate %dbps outside acceptable range [%d,%d]\n",
                        gBitRate, kMinBitRate, kMaxBitRate);
                return 2;
            }
            break;
        case 'n':
            gCodecName = optarg;
            break;
        case 'i':
            gInFileName = optarg;
            break;
        case 'o':
            gOutFileName = optarg;
            break;
        case 'B':
            if (parseValueWithUnit(optarg, &gBframes) != NO_ERROR) {
                return 2;
            }
            break;
        default:
            if (ic != '?') {
                fprintf(stderr, "getopt_long returned unexpected value 0x%x\n", ic);
            }
            return 2;
        }
    }

    if (gVerbose) {
        fprintf(stderr, "dumpInfo: in_yuv=%s, out_bs=%s,"
                        "wxh=%dx%d, bit_rate=%u, bframs=%d, codec_name=%s\n",
                        gInFileName, gOutFileName,
                        gVideoWidth, gVideoHeight, gBitRate, gBframes, gCodecName.c_str());
    }

    status_t err = videoEncode();
    ALOGD(err == NO_ERROR ? "success" : "failed");
    return (int) err;
}
