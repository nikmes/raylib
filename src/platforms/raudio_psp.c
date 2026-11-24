/**********************************************************************************************
*
*   raudio_psp - PSP miniaudio backend
*
*   Included by src/raudio.c under PLATFORM_PSP.
*   P9-S1 installed the silent sink; P9-S2 drives one regular 1024-frame channel with two
*   alternating aligned PCM buffers and an explicit cache writeback before each output call.
*
**********************************************************************************************/

#include <limits.h>
#include <pspaudio.h>
#include <pspthreadman.h>
#include <psputils.h>
#include <stdint.h>
#include <string.h>

#define RAUDIO_PSP_FRAMES 1024
#define RAUDIO_PSP_CHANNEL_FRAMES PSP_AUDIO_SAMPLE_ALIGN(RAUDIO_PSP_FRAMES)

static int16_t raudioPspPcm[2][RAUDIO_PSP_FRAMES*2] __attribute__((aligned(64)));
static int raudioPspPcmIndex = 0;
static int raudioPspChannel = -1;

static unsigned int raudioPspCallbacks = 0;
static unsigned int raudioPspErrors = 0;
static SceInt64 raudioPspFirstWrite = 0;
static SceInt64 raudioPspLastWrite = 0;
static SceUID raudioPspThreadId = -1;
static int raudioPspRestMin = INT_MAX;
static int raudioPspRestMax = 0;
static unsigned int raudioPspUnderruns = 0;
static int raudioPspHighPriority = 0;
static int raudioPspSlowGaps = 0;
static SceInt64 raudioPspPrevWrite = 0;
static SceInt64 raudioPspMaxGap = 0;

static ma_result OnPspContextInit(ma_context *context, const ma_context_config *config, ma_backend_callbacks *callbacks)
{
    (void)context;
    (void)config;
    (void)callbacks;
    return MA_SUCCESS;
}

static ma_result OnPspContextUninit(ma_context *context)
{
    (void)context;
    return MA_SUCCESS;
}

static ma_result OnPspDeviceInit(ma_device *device, const ma_device_config *config, ma_device_descriptor *playback, ma_device_descriptor *capture)
{
    (void)device;
    (void)config;
    (void)capture;

    playback->format = ma_format_s16;
    playback->channels = 2;
    playback->sampleRate = 44100;
    playback->periodSizeInFrames = RAUDIO_PSP_FRAMES;
    playback->periodCount = 2;

    return MA_SUCCESS;
}

static ma_result OnPspDeviceStart(ma_device *device)
{
    (void)device;

    if (raudioPspChannel < 0)
    {
        int channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, RAUDIO_PSP_CHANNEL_FRAMES, PSP_AUDIO_FORMAT_STEREO);
        if (channel < 0)
        {
            TRACELOG(LOG_ERROR, "AUDIO: sceAudioChReserve failed 0x%08X", (unsigned int)channel);
            return MA_ERROR;
        }

        raudioPspChannel = channel;
        raudioPspCallbacks = 0;
        raudioPspErrors = 0;
        raudioPspFirstWrite = 0;
        raudioPspLastWrite = 0;
        raudioPspRestMin = INT_MAX;
        raudioPspRestMax = 0;
        raudioPspUnderruns = 0;
        raudioPspHighPriority = 0;
        raudioPspSlowGaps = 0;
        raudioPspPrevWrite = 0;
        raudioPspMaxGap = 0;
        TRACELOG(LOG_INFO, "AUDIO: PSP channel=%d frames=%d", raudioPspChannel, RAUDIO_PSP_FRAMES);
    }

    return MA_SUCCESS;
}

static ma_result OnPspDeviceStop(ma_device *device)
{
    (void)device;
    return MA_SUCCESS;
}

static ma_result OnPspDeviceUninit(ma_device *device)
{
    (void)device;

    if (raudioPspChannel >= 0)
    {
        int result = sceAudioChRelease(raudioPspChannel);
        if (result < 0) TRACELOG(LOG_WARNING, "AUDIO: sceAudioChRelease failed 0x%08X", (unsigned int)result);
        raudioPspChannel = -1;
    }

    return MA_SUCCESS;
}

static ma_result OnPspDeviceWrite(ma_device *device, const void *frames, ma_uint32 frameCount, ma_uint32 *framesWritten)
{
    (void)device;

    if (framesWritten != NULL) *framesWritten = 0;
    if ((frames == NULL) || (frameCount == 0)) return MA_SUCCESS;
    if (raudioPspChannel < 0) return MA_ERROR;

    SceInt64 now = sceKernelGetSystemTimeWide();
    SceInt64 writeGap = (raudioPspPrevWrite != 0)? (now - raudioPspPrevWrite) : 0;
    if (writeGap > raudioPspMaxGap) raudioPspMaxGap = writeGap;
    raudioPspPrevWrite = now;

    if (raudioPspCallbacks == 0)
    {
        raudioPspFirstWrite = now;
        raudioPspThreadId = sceKernelGetThreadId();
    }

    const int16_t *source = (const int16_t *)frames;
    ma_uint32 remaining = frameCount;
    ma_uint32 written = 0;

    while (remaining > 0)
    {
        ma_uint32 blockFrames = (remaining > RAUDIO_PSP_FRAMES)? RAUDIO_PSP_FRAMES : remaining;
        int16_t *destination = raudioPspPcm[raudioPspPcmIndex];

        memcpy(destination, source, blockFrames*2*sizeof(int16_t));

        // The channel is reserved for a full block; zero-fill a short tail so stale
        // samples are never played back.
        if (blockFrames < RAUDIO_PSP_FRAMES)
        {
            memset(destination + blockFrames*2, 0, (RAUDIO_PSP_FRAMES - blockFrames)*2*sizeof(int16_t));
        }

        sceKernelDcacheWritebackRange(destination, RAUDIO_PSP_FRAMES*2*sizeof(int16_t));

        if (raudioPspCallbacks > 0)
        {
            int preRest = sceAudioGetChannelRestLength(raudioPspChannel);
            if (preRest == 0) raudioPspUnderruns++;

            // miniaudio creates this worker at PSP priority 60, below the 0x20 game thread.
            // At 60 fps the block pacing is ~23 ms and the frame gaps are enough. A frame
            // longer than one block pushes the gap to ~33 ms and starves the worker; raise
            // it above the frame loop only after two consecutive slow gaps, so a single
            // load hitch does not raise a 60 fps workload. One-way: once raised it stays, so
            // 60 fps and slow scenes each keep their correct policy.
            if (writeGap > 28000) raudioPspSlowGaps++;
            else raudioPspSlowGaps = 0;

            if (!raudioPspHighPriority && (raudioPspSlowGaps >= 2))
            {
                sceKernelChangeThreadPriority(raudioPspThreadId, 0x1E);
                raudioPspHighPriority = 1;
                raudioPspSlowGaps = 0;
                TRACELOG(LOG_INFO, "AUDIO: PSP worker raised to 0x1E (gap=%lld us)", (long long)writeGap);
            }
        }

        int result = sceAudioOutputPannedBlocking(raudioPspChannel, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, destination);
        if (result < 0)
        {
            raudioPspErrors++;
            if (raudioPspErrors == 1) TRACELOG(LOG_ERROR, "AUDIO: sceAudioOutputPannedBlocking failed 0x%08X", (unsigned int)result);
            if (framesWritten != NULL) *framesWritten = written;
            return MA_ERROR;
        }

        int rest = sceAudioGetChannelRestLength(raudioPspChannel);
        if (rest >= 0)
        {
            if (rest < raudioPspRestMin) raudioPspRestMin = rest;
            if (rest > raudioPspRestMax) raudioPspRestMax = rest;
        }

        raudioPspCallbacks++;
        raudioPspLastWrite = sceKernelGetSystemTimeWide();
        raudioPspPcmIndex ^= 1;

        source += blockFrames*2;
        remaining -= blockFrames;
        written += blockFrames;
    }

    if (framesWritten != NULL) *framesWritten = written;
    return MA_SUCCESS;
}

static ma_backend_callbacks raudioPspBackend = {
    OnPspContextInit,
    OnPspContextUninit,
    NULL,
    NULL,
    OnPspDeviceInit,
    OnPspDeviceUninit,
    OnPspDeviceStart,
    OnPspDeviceStop,
    NULL,
    OnPspDeviceWrite,
    NULL,
    NULL,
    NULL
};

const ma_backend_callbacks *raudio_psp_get_backend_callbacks(void)
{
    return &raudioPspBackend;
}

void raudio_psp_log_stats(void)
{
    if (raudioPspCallbacks == 0)
    {
        TRACELOG(LOG_WARNING, "AUDIO: PSP output saw no writes (errors=%u)", raudioPspErrors);
        return;
    }

    SceKernelThreadRunStatus status;
    memset(&status, 0, sizeof(status));
    status.size = sizeof(status);

    unsigned long long runClocks = 0;
    if (raudioPspThreadId >= 0 && sceKernelReferThreadRunStatus(raudioPspThreadId, &status) == 0)
    {
        runClocks = ((unsigned long long)status.runClocks.hi << 32) | status.runClocks.low;
    }

    SceInt64 elapsed = raudioPspLastWrite - raudioPspFirstWrite;
    unsigned int callbacksPerSec = (elapsed > 0)? (unsigned int)(((unsigned long long)raudioPspCallbacks * 1000000ULL) / (unsigned long long)elapsed) : 0;

    if (raudioPspRestMin == INT_MAX) raudioPspRestMin = 0;

    TRACELOG(LOG_INFO, "AUDIO: PSP callbacks=%u callbacks_per_sec=%u rest_min=%d rest_max=%d underruns=%u max_gap_us=%lld errors=%u runClocks=%llu elapsed_us=%lld",
             raudioPspCallbacks, callbacksPerSec, raudioPspRestMin, raudioPspRestMax, raudioPspUnderruns,
             (long long)raudioPspMaxGap, raudioPspErrors, runClocks, (long long)elapsed);
}
