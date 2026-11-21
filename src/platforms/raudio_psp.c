/**********************************************************************************************
*
*   raudio_psp - silent miniaudio backend for P9-S1
*
*   This file is included by src/raudio.c under PLATFORM_PSP. It provides the PSP backend
*   callbacks only; the real channel/output path belongs to P9-S2.
*
**********************************************************************************************/

#include <pspthreadman.h>
#include <stdint.h>
#include <string.h>

static unsigned int raudioPspCallbacks = 0;
static SceInt64 raudioPspFirstWrite = 0;
static SceInt64 raudioPspLastWrite = 0;
static SceUID raudioPspThreadId = -1;

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
    playback->periodSizeInFrames = 1024;
    playback->periodCount = 2;

    return MA_SUCCESS;
}

static ma_result OnPspDeviceStart(ma_device *device)
{
    (void)device;
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
    return MA_SUCCESS;
}

static ma_result OnPspDeviceWrite(ma_device *device, const void *frames, ma_uint32 frameCount, ma_uint32 *framesWritten)
{
    (void)device;
    (void)frames;

    if (framesWritten != NULL) *framesWritten = frameCount;

    if (raudioPspCallbacks == 0)
    {
        raudioPspFirstWrite = sceKernelGetSystemTimeWide();
        raudioPspThreadId = sceKernelGetThreadId();
    }

    raudioPspCallbacks++;
    raudioPspLastWrite = sceKernelGetSystemTimeWide();

    if (frameCount > 0)
    {
        SceUInt usec = (SceUInt)(((unsigned long long)frameCount * 1000000ULL) / 44100ULL);
        if (usec == 0) usec = 1;
        sceKernelDelayThread(usec);
    }

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
        TRACELOG(LOG_WARNING, "AUDIO: silent backend saw no writes");
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

    TRACELOG(LOG_INFO, "AUDIO: silent callbacks=%u callbacks_per_sec=%u runClocks=%llu elapsed_us=%lld",
             raudioPspCallbacks, callbacksPerSec, runClocks, (long long)elapsed);
}
