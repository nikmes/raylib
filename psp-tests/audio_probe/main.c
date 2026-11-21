/*
 * audio_probe - can software recover volume on a console whose volume-up button is broken?
 *
 * Two questions, one run:
 *   1. What can a user-mode app actually READ about the system volume? Answer: nothing.
 *      sceImposeGetParam(PSP_IMPOSE_MAIN_VOLUME) is declared for the kernel impose driver,
 *      and this SDK ships no libpspimpose_driver import stub, so it cannot even be linked
 *      from user mode. The user-mode pspimpose.h has no volume call at all.
 *   2. What can it SET? sceAudioChangeChannelVolume(ch, 0x8000, 0x8000) is the maximum the
 *      application layer offers. Whether that is loud depends on the system volume, which is
 *      applied by the hardware after us - so the only honest test is to play a full-scale tone
 *      and listen.
 *
 * Plays three 2-second bursts of a 1 kHz tone at full channel volume and full sample scale,
 * then a fourth burst at half sample scale so the two can be compared by ear.
 */

#include <pspkernel.h>
#include <pspaudio.h>
#include <pspimpose.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("audio_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SAMPLES   1024
#define RATE      44100
#define VOL_MAX   0x8000

static short buffer[SAMPLES*2];

static void fillTone(float scale)
{
    for (int i = 0; i < SAMPLES; i++)
    {
        short v = (short)(32767.0f * scale * sinf(2.0f*3.14159265f*1000.0f*(float)i/(float)RATE));
        buffer[i*2 + 0] = v;
        buffer[i*2 + 1] = v;
    }
}

static void burst(int channel, const char *tag, float scale, int buffers)
{
    fillTone(scale);
    printf("[PROBE] %s: %d buffers at channel volume 0x%04X\n", tag, buffers, VOL_MAX);
    fflush(stdout);

    for (int i = 0; i < buffers; i++) sceAudioOutputBlocking(channel, VOL_MAX, buffer);
}

int main(int argc, char **argv)
{
    printf("[PROBE] start\n"); fflush(stdout);

    // What user mode can read. None of these is the volume, and there is no call that is.
    int lang = 0, button = 0;
    int r = sceImposeGetLanguageMode(&lang, &button);
    printf("[PROBE] sceImposeGetLanguageMode -> %d (lang %d, button %d)\n", r, lang, button);
    printf("[PROBE] sceImposeGetBacklightOffTime -> %d\n", sceImposeGetBacklightOffTime());
    printf("[PROBE] note: no libpspaudiorouting stub either; routing mode is unreadable here\n");
    printf("[PROBE] note: sceImposeGetParam(MAIN_VOLUME) is kernel-only; this SDK has no\n");
    printf("[PROBE]       libpspimpose_driver stub, so it cannot be linked from user mode\n");
    fflush(stdout);

    int channel = sceAudioChReserve(-1, SAMPLES, PSP_AUDIO_FORMAT_STEREO);
    printf("[PROBE] sceAudioChReserve -> %d\n", channel);
    if (channel < 0) { printf("[PROBE] DONE (no channel)\n"); fflush(stdout); sceKernelSleepThread(); return 0; }

    int v = sceAudioChangeChannelVolume(channel, VOL_MAX, VOL_MAX);
    printf("[PROBE] sceAudioChangeChannelVolume(0x%04X, 0x%04X) -> %d\n", VOL_MAX, VOL_MAX, v);
    fflush(stdout);

    // ~2 s per burst at 1024 samples/buffer, 44.1 kHz
    burst(channel, "full scale  #1", 1.0f, 86);
    sceKernelDelayThread(500000);
    burst(channel, "full scale  #2", 1.0f, 86);
    sceKernelDelayThread(500000);
    burst(channel, "full scale  #3", 1.0f, 86);
    sceKernelDelayThread(500000);
    burst(channel, "half scale  (for comparison)", 0.5f, 86);

    sceAudioChRelease(channel);
    printf("[PROBE] DONE\n"); fflush(stdout);
    sceKernelSleepThread();

    return 0;
}
