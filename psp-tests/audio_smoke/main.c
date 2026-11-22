/**********************************************************************************************
*
*   audio_smoke - P9-S3 Sound/Wave API and mixing harness
*
*   Modes: tone/sound (S2), api, mix, pitch, levels, heap, music, musicmix,
*   stream, lifecycle, trackchange (P9-S5).
*   The game path uses raylib.h only; PSP headers appear only in this test harness.
*
**********************************************************************************************/

#include <malloc.h>
#include <math.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <raylib.h>

PSP_MODULE_INFO("audio_smoke", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef AUDIO_COUNT
    #define AUDIO_COUNT 16
#endif
#ifndef AUDIO_FRAMES
    #define AUDIO_FRAMES 600
#endif
#ifndef AUDIO_PITCH
    #define AUDIO_PITCH 1.0f
#endif
#ifndef AUDIO_WAVE_MS
    #define AUDIO_WAVE_MS 1000
#endif
#ifndef AUDIO_STREAM_BUFFER
    #define AUDIO_STREAM_BUFFER 1024
#endif
#ifndef MUSIC_PATH
    #define MUSIC_PATH "/p9s4_target.ogg"
#endif
#ifndef MUSIC_WAV_PATH
    #define MUSIC_WAV_PATH "/p9s4_sound.wav"
#endif

#define MAX_MEASURE 18000
static float measureMs[MAX_MEASURE];
static float sortedMs[MAX_MEASURE];
static int CompareFloat(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}
static float StreamPhase = 0.0f;
static short StreamPcm[44100*2];
static int StreamPcmOffset;

#if !defined(MODE_TONE) && !defined(MODE_SOUND) && !defined(MODE_API) && \
    !defined(MODE_MIX) && !defined(MODE_PITCH) && !defined(MODE_LEVELS) && !defined(MODE_HEAP) && \
    !defined(MODE_MUSIC) && !defined(MODE_MUSICMIX) && !defined(MODE_STREAM) && \
    !defined(MODE_LIFECYCLE) && !defined(MODE_TRACKCHANGE)
    #define MODE_TONE
#endif

static Wave MakeWave(int milliseconds, float frequency)
{
    int sampleRate = 44100;
    int frameCount = sampleRate*milliseconds/1000;
    short *samples = (short *)MemAlloc(frameCount*2*sizeof(short));

    for (int i = 0; i < frameCount; i++)
    {
        float value = sinf(2.0f*3.14159265f*frequency*((float)i/(float)sampleRate))*0.20f;
        short sample = (short)(value*32767.0f);
        samples[i*2 + 0] = sample;
        samples[i*2 + 1] = sample;
    }

    Wave wave = { 0 };
    wave.frameCount = frameCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 2;
    wave.data = samples;
    return wave;
}

static void FreeWaveData(Wave *wave)
{
    MemFree(wave->data);
    wave->data = NULL;
}

static void DrawFrame(int frame, const char *mode, const char *status)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText(TextFormat("%s %d  audio=%s  %s", mode, frame,
                        IsAudioDeviceReady()? "ready" : "NOT READY", status),
             10, 10, 20, BLACK);
    DrawFPS(10, 40);
    EndDrawing();
}

static void RunFrames(int frames, const char *mode, const char *status)
{
    for (int frame = 0; frame < frames; frame++)
    {
        DrawFrame(frame, mode, status);
        if ((frame > 0) && ((frame%60) == 0))
        {
            TraceLog(LOG_INFO, "%s FRAME %d  fps %d", mode, frame, GetFPS());
        }
    }
}

#if defined(MODE_API)
static void ApiMatrix(void)
{
    Wave wave = MakeWave(AUDIO_WAVE_MS, 1000.0f);
    Sound source = LoadSoundFromWave(wave);
    TraceLog(LOG_INFO, "API LoadSoundFromWave valid=%d", IsSoundValid(source));

    Wave fileWave = LoadWave("host0:" MUSIC_WAV_PATH);
    TraceLog(LOG_INFO, "API LoadWave valid=%d", IsWaveValid(fileWave));
    if (IsWaveValid(fileWave)) UnloadWave(fileWave);

    int memSize = 0;
    unsigned char *memData = LoadFileData("host0:" MUSIC_WAV_PATH, &memSize);
    if (memData != NULL)
    {
        Wave memWave = LoadWaveFromMemory(".wav", memData, memSize);
        TraceLog(LOG_INFO, "API LoadWaveFromMemory valid=%d", IsWaveValid(memWave));
        UnloadFileData(memData);
        if (IsWaveValid(memWave)) UnloadWave(memWave);
    }
    else TraceLog(LOG_WARNING, "API LoadFileData failed for %s", MUSIC_WAV_PATH);

    Wave invalidWave = { 0 };
    TraceLog(LOG_INFO, "API IsWaveValid(invalid)=%d", IsWaveValid(invalidWave));

    Sound invalid = { 0 };
    TraceLog(LOG_INFO, "API IsSoundValid(invalid)=%d", IsSoundValid(invalid));

    int exported = ExportWave(wave, "host0:/audio_smoke_tone.wav");
    TraceLog(LOG_INFO, "API ExportWave=%d", exported);

    bool exportedCode = ExportWaveAsCode(wave, "host0:/audio_smoke_wave.h");
    TraceLog(LOG_INFO, "API ExportWaveAsCode=%d", exportedCode);
    if (exportedCode) remove("host0:/audio_smoke_wave.h");
    if (exported)
    {
        Sound fileSound = LoadSound("host0:/audio_smoke_tone.wav");
        TraceLog(LOG_INFO, "API LoadSound valid=%d", IsSoundValid(fileSound));
        PlaySound(fileSound);
        StopSound(fileSound);
        UnloadSound(fileSound);
        remove("host0:/audio_smoke_tone.wav");
    }

    Wave copy = WaveCopy(wave);
    if (copy.data != NULL)
    {
        WaveCrop(&copy, 0, copy.frameCount/2);
        WaveFormat(&copy, 22050, 16, 2);
        float *waveSamples = LoadWaveSamples(copy);
        TraceLog(LOG_INFO, "API WaveCopy/Crop/Format/LoadWaveSamples data=%d", waveSamples != NULL);
        if (waveSamples != NULL) UnloadWaveSamples(waveSamples);
        UnloadWave(copy);
    }

    SetMasterVolume(0.5f);
    TraceLog(LOG_INFO, "API master=%.2f", GetMasterVolume());

    SetSoundVolume(source, 0.5f);
    SetSoundPan(source, -0.5f);
    SetSoundPitch(source, 1.25f);

    PlaySound(source);
    TraceLog(LOG_INFO, "API PlaySound playing=%d", IsSoundPlaying(source));
    PauseSound(source);
    ResumeSound(source);

    float *update = (float *)MemAlloc(wave.frameCount*2*sizeof(float));
    for (int i = 0; i < wave.frameCount; i++)
    {
        float value = sinf(2.0f*3.14159265f*440.0f*((float)i/(float)wave.sampleRate))*0.10f;
        update[i*2 + 0] = value;
        update[i*2 + 1] = value;
    }
    UpdateSound(source, update, wave.frameCount);
    MemFree(update);
    TraceLog(LOG_INFO, "API UpdateSound done");

    StopSound(source);
    TraceLog(LOG_INFO, "API StopSound stopped=%d", !IsSoundPlaying(source));

    Sound alias = LoadSoundAlias(source);
    TraceLog(LOG_INFO, "API LoadSoundAlias valid=%d", IsSoundValid(alias));
    UnloadSoundAlias(alias);
    TraceLog(LOG_INFO, "API source still valid=%d", IsSoundValid(source));

    UnloadSound(source);
    FreeWaveData(&wave);

    RunFrames(120, "api", "matrix done");
}
#endif

#if defined(MODE_PITCH)
static void PitchMode(void)
{
    Wave wave = MakeWave(AUDIO_WAVE_MS, 1000.0f);
    Sound sound = LoadSoundFromWave(wave);
    FreeWaveData(&wave);

    SetSoundPitch(sound, AUDIO_PITCH);
    PlaySound(sound);

    int stopFrame = -1;
    for (int frame = 0; frame < AUDIO_FRAMES; frame++)
    {
        DrawFrame(frame, "pitch", TextFormat("pitch=%.2f", (double)AUDIO_PITCH));
        if ((stopFrame < 0) && !IsSoundPlaying(sound)) stopFrame = frame;
    }

    TraceLog(LOG_INFO, "PITCH value=%.2f stop_frame=%d", (double)AUDIO_PITCH, stopFrame);
    UnloadSound(sound);
}
#endif

#if defined(MODE_LEVELS)
static void LevelsMode(void)
{
    Wave wave = MakeWave(AUDIO_WAVE_MS, 1000.0f);
    Sound sound = LoadSoundFromWave(wave);
    FreeWaveData(&wave);

    SetMasterVolume(1.0f);
    SetSoundVolume(sound, 1.0f);
    SetSoundPan(sound, -1.0f);
    PlaySound(sound);
    RunFrames(30, "levels", "loud-left");
    StopSound(sound);

    SetSoundVolume(sound, 0.20f);
    SetSoundPan(sound, 1.0f);
    PlaySound(sound);
    RunFrames(30, "levels", "quiet-right");
    StopSound(sound);

    TraceLog(LOG_INFO, "LEVELS loud-left then quiet-right");
    UnloadSound(sound);
}
#endif

#if defined(MODE_MIX)
static void MixMode(void)
{
    Sound *sounds = (Sound *)MemAlloc(AUDIO_COUNT*sizeof(Sound));
    int valid = 0;

    for (int i = 0; i < AUDIO_COUNT; i++)
    {
        Wave wave = MakeWave(AUDIO_WAVE_MS, 300.0f + (float)i*20.0f);
        sounds[i] = LoadSoundFromWave(wave);
        if (IsSoundValid(sounds[i])) valid++;
        FreeWaveData(&wave);
        SetSoundVolume(sounds[i], 0.15f);
        PlaySound(sounds[i]);
    }

    TraceLog(LOG_INFO, "MIX count=%d valid=%d wave_ms=%d playing0=%d",
             AUDIO_COUNT, valid, AUDIO_WAVE_MS, IsSoundPlaying(sounds[0]));
    RunFrames(AUDIO_FRAMES, "mix", TextFormat("count=%d", AUDIO_COUNT));

    for (int i = 0; i < AUDIO_COUNT; i++) UnloadSound(sounds[i]);
    MemFree(sounds);
}
#endif

#if defined(MODE_HEAP)
static void HeapMode(void)
{
    int before = sceKernelTotalFreeMemSize();
    int maxBefore = sceKernelMaxFreeMemSize();
    struct mallinfo miBefore = mallinfo();
    char *brkBefore = (char *)sbrk(0);

    Sound *sounds = (Sound *)MemAlloc(AUDIO_COUNT*sizeof(Sound));
    int valid = 0;
    for (int i = 0; i < AUDIO_COUNT; i++)
    {
        Wave wave = MakeWave(AUDIO_WAVE_MS, 300.0f + (float)i*10.0f);
        sounds[i] = LoadSoundFromWave(wave);
        if (IsSoundValid(sounds[i])) valid++;
        FreeWaveData(&wave);
    }

    int after = sceKernelTotalFreeMemSize();
    int maxAfter = sceKernelMaxFreeMemSize();
    struct mallinfo miAfter = mallinfo();
    char *brkAfter = (char *)sbrk(0);
    TraceLog(LOG_INFO, "HEAP count=%d valid=%d sbrk_delta=%d ma_arena=%u ma_uordblks=%u ma_fordblks=%u partition_before=%d max_before=%d partition_after=%d max_after=%d",
             AUDIO_COUNT, valid, (int)(brkAfter - brkBefore),
             (unsigned)miAfter.arena, (unsigned)miAfter.uordblks, (unsigned)miAfter.fordblks,
             before, maxBefore, after, maxAfter);

    for (int i = 0; i < AUDIO_COUNT; i++) { if (IsSoundValid(sounds[i])) UnloadSound(sounds[i]); }
    MemFree(sounds);

    RunFrames(60, "heap", TextFormat("count=%d", AUDIO_COUNT));
}
#endif

#if defined(MODE_MUSIC) || defined(MODE_MUSICMIX)
static void MusicBaseline(const char *tag, int withSounds)
{
    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_BUFFER);

    Music music = LoadMusicStream(TextFormat("host0:%s", MUSIC_PATH));
    if (!IsMusicValid(music))
    {
        TraceLog(LOG_ERROR, "MUSIC %s invalid path=%s", tag, MUSIC_PATH);
        return;
    }

    TraceLog(LOG_INFO, "MUSIC %s valid=1 length=%.3f", tag, GetMusicTimeLength(music));
    SetMusicVolume(music, 0.5f);
    PlayMusicStream(music);

    Sound *sounds = NULL;
    if (withSounds)
    {
        sounds = (Sound *)MemAlloc(AUDIO_COUNT*sizeof(Sound));
        for (int i = 0; i < AUDIO_COUNT; i++)
        {
            Wave wave = MakeWave(AUDIO_WAVE_MS, 300.0f + (float)i*20.0f);
            sounds[i] = LoadSoundFromWave(wave);
            FreeWaveData(&wave);
            SetSoundVolume(sounds[i], 0.15f);
            PlaySound(sounds[i]);
        }
        TraceLog(LOG_INFO, "MUSIC %s sounds=%d wave_ms=%d", tag, AUDIO_COUNT, AUDIO_WAVE_MS);
    }

    int measured = 0;
    int totalFrames = 60 + AUDIO_FRAMES;

    for (int frame = 0; frame < totalFrames; frame++)
    {
        double start = GetTime();
        UpdateMusicStream(music);
        double elapsedMs = (GetTime() - start)*1000.0;

        if ((frame >= 60) && (measured < MAX_MEASURE))
        {
            measureMs[measured++] = (float)elapsedMs;
        }

        DrawFrame(frame, tag, TextFormat("t=%.1f up=%.2f", GetMusicTimePlayed(music), elapsedMs));
        if ((frame > 0) && ((frame%60) == 0))
        {
            TraceLog(LOG_INFO, "MUSIC %s FRAME %d  fps %d  up=%.2f", tag, frame, GetFPS(), elapsedMs);
        }
    }

    int n = measured;
    for (int i = 0; i < n; i++) sortedMs[i] = measureMs[i];
    qsort(sortedMs, n, sizeof(float), CompareFloat);

    float p95 = (n > 0)? sortedMs[(int)(0.95f*(float)(n - 1))] : 0.0f;
    float max = (n > 0)? sortedMs[n - 1] : 0.0f;
    int spikes = 0;
    for (int i = 0; i < n; i++) { if (measureMs[i] > 2.0f) spikes++; }

    TraceLog(LOG_INFO, "MUSIC %s measured=%d p95=%.3fms max=%.3fms spikes>2ms=%d time=%.2f length=%.2f",
             tag, n, p95, max, spikes, GetMusicTimePlayed(music), GetMusicTimeLength(music));

    StopMusicStream(music);
    UnloadMusicStream(music);

    if (sounds != NULL)
    {
        for (int i = 0; i < AUDIO_COUNT; i++) UnloadSound(sounds[i]);
        MemFree(sounds);
    }
}

static void MusicApiMode(void)
{
    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_BUFFER);

    Music ogg = LoadMusicStream(TextFormat("host0:%s", MUSIC_PATH));
    Music wav = LoadMusicStream(TextFormat("host0:%s", MUSIC_WAV_PATH));
    Music invalid = { 0 };

    TraceLog(LOG_INFO, "MUSIC API ogg=%d wav=%d invalid=%d", IsMusicValid(ogg), IsMusicValid(wav), IsMusicValid(invalid));

    int memMusicSize = 0;
    unsigned char *memMusic = LoadFileData(TextFormat("host0:%s", MUSIC_PATH), &memMusicSize);
    if (memMusic != NULL)
    {
        const char *memType = (strstr(MUSIC_PATH, ".wav") != NULL)? ".wav" : ".ogg";
        Music fromMemory = LoadMusicStreamFromMemory(memType, memMusic, memMusicSize);
        TraceLog(LOG_INFO, "MUSIC API frommemory valid=%d", IsMusicValid(fromMemory));
        if (IsMusicValid(fromMemory)) UnloadMusicStream(fromMemory);
        UnloadFileData(memMusic);
    }
    else TraceLog(LOG_WARNING, "MUSIC API LoadFileData failed for %s", MUSIC_PATH);

    if (IsMusicValid(ogg))
    {
        TraceLog(LOG_INFO, "MUSIC API length ogg=%.3f", GetMusicTimeLength(ogg));
        SetMusicVolume(ogg, 0.5f);
        SetMusicPan(ogg, -0.5f);
        SetMusicPitch(ogg, 1.25f);
        PlayMusicStream(ogg);
        UpdateMusicStream(ogg);
        TraceLog(LOG_INFO, "MUSIC API playing=%d", IsMusicStreamPlaying(ogg));

        PauseMusicStream(ogg);
        ResumeMusicStream(ogg);
        SeekMusicStream(ogg, 0.2f);
        UpdateMusicStream(ogg);
        TraceLog(LOG_INFO, "MUSIC API played=%.3f", GetMusicTimePlayed(ogg));
        StopMusicStream(ogg);
    }

    if (IsMusicValid(wav))
    {
        TraceLog(LOG_INFO, "MUSIC API length wav=%.3f", GetMusicTimeLength(wav));
        PlayMusicStream(wav);
        UpdateMusicStream(wav);
        StopMusicStream(wav);
    }

    if (IsMusicValid(ogg)) UnloadMusicStream(ogg);
    if (IsMusicValid(wav)) UnloadMusicStream(wav);

    RunFrames(60, "musicapi", "matrix done");
}
#endif

#if defined(MODE_MUSIC)
static void MusicMode(void)
{
    MusicApiMode();
    MusicBaseline("music", 0);
}
#endif

#if defined(MODE_MUSICMIX)
static void MusicMixMode(void)
{
    MusicBaseline("musicmix", 1);
}
#endif

#if defined(MODE_LIFECYCLE)
static void LifecycleMode(void)
{
    // main() already initialized the device: close it and cycle from a clean state
    CloseAudioDevice();

    for (int i = 0; i < 10; i++)
    {
        int freeBefore = (int)sceKernelTotalFreeMemSize();
        int maxBefore = (int)sceKernelMaxFreeMemSize();

        InitAudioDevice();
        bool ready = IsAudioDeviceReady();
        CloseAudioDevice();

        int freeAfter = (int)sceKernelTotalFreeMemSize();
        int maxAfter = (int)sceKernelMaxFreeMemSize();
        TraceLog(LOG_INFO, "LIFECYCLE %d ready=%d free=%d maxfree=%d after=%d maxafter=%d",
                 i, ready, freeBefore, maxBefore, freeAfter, maxAfter);
    }

    RunFrames(30, "lifecycle", "10 cycles done");
    TraceLog(LOG_INFO, "LIFECYCLE DONE cycles=10 ready=%d", IsAudioDeviceReady());
}
#endif

#if defined(MODE_TRACKCHANGE)
static void TrackChangeMode(void)
{
    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_BUFFER);

    Music a = LoadMusicStream(TextFormat("host0:%s", MUSIC_PATH));
    Music b = LoadMusicStream(TextFormat("host0:%s", MUSIC_WAV_PATH));
    TraceLog(LOG_INFO, "TRACKCHANGE valid a=%d b=%d", IsMusicValid(a), IsMusicValid(b));

    if (IsMusicValid(a))
    {
        SetMusicVolume(a, 0.5f);
        PlayMusicStream(a);
        RunFrames(240, "trackchange", "A playing");
        TraceLog(LOG_INFO, "TRACKCHANGE A time=%.2f playing=%d", GetMusicTimePlayed(a), IsMusicStreamPlaying(a));
        StopMusicStream(a);
        UnloadMusicStream(a);
        TraceLog(LOG_INFO, "TRACKCHANGE A unloaded");
    }

    if (IsMusicValid(b))
    {
        SetMusicVolume(b, 0.5f);
        PlayMusicStream(b);
        RunFrames(240, "trackchange", "B playing");
        TraceLog(LOG_INFO, "TRACKCHANGE B time=%.2f playing=%d", GetMusicTimePlayed(b), IsMusicStreamPlaying(b));
        StopMusicStream(b);
        UnloadMusicStream(b);
        TraceLog(LOG_INFO, "TRACKCHANGE B unloaded");
    }

    // Reload the first track to prove the decoder can start again after a swap
    Music c = LoadMusicStream(TextFormat("host0:%s", MUSIC_PATH));
    TraceLog(LOG_INFO, "TRACKCHANGE reload valid=%d", IsMusicValid(c));
    if (IsMusicValid(c))
    {
        PlayMusicStream(c);
        RunFrames(60, "trackchange", "C playing");
        StopMusicStream(c);
        UnloadMusicStream(c);
    }

    TraceLog(LOG_INFO, "TRACKCHANGE DONE");
}
#endif

#if defined(MODE_STREAM)
static void StreamCallback(void *bufferData, unsigned int frames)
{
    // The callback arm uses a 32-bit float stream, so the callback buffer is f32.
    // Copy precomputed PCM; do not synthesize on the audio thread.
    float *out = (float *)bufferData;
    for (unsigned int i = 0; i < frames; i++)
    {
        int j = (StreamPcmOffset + (int)i)%44100;
        out[i*2 + 0] = (float)StreamPcm[j*2 + 0]/32768.0f;
        out[i*2 + 1] = (float)StreamPcm[j*2 + 1]/32768.0f;
    }
    StreamPcmOffset = (StreamPcmOffset + (int)frames)%44100;
}

static void StreamProcessor(void *bufferData, unsigned int frames)
{
    float *out = (float *)bufferData;
    for (unsigned int i = 0; i < frames*2; i++) out[i] *= 0.5f;
}

static void MixedProcessor(void *bufferData, unsigned int frames)
{
    float *out = (float *)bufferData;
    for (unsigned int i = 0; i < frames*2; i++) out[i] *= 0.9f;
}

static void AudioStreamMode(void)
{
    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_BUFFER);

#if defined(STREAM_CALLBACK)
    AudioStream stream = LoadAudioStream(44100, 32, 2);
#else
    AudioStream stream = LoadAudioStream(44100, 16, 2);
#endif
    if (!IsAudioStreamValid(stream))
    {
        TraceLog(LOG_ERROR, "STREAM invalid AudioStream");
        return;
    }

    SetAudioStreamVolume(stream, 1.0f);
    SetAudioStreamPan(stream, 0.0f);
    SetAudioStreamPitch(stream, 1.0f);

#if defined(STREAM_CALLBACK)
    SetAudioStreamCallback(stream, StreamCallback);
#elif defined(STREAM_PROCESSOR)
    AttachAudioStreamProcessor(stream, StreamProcessor);
#endif

    short pcm[AUDIO_STREAM_BUFFER*2];
    for (int i = 0; i < 44100; i++)
    {
        float value = sinf(2.0f*3.14159265f*440.0f*((float)i/44100.0f))*0.20f;
        short sample = (short)(value*32767.0f);
        StreamPcm[i*2 + 0] = sample;
        StreamPcm[i*2 + 1] = sample;
    }
    int pcmOffset = 0;
    StreamPcmOffset = 0;

    PlayAudioStream(stream);
    AttachAudioMixedProcessor(MixedProcessor);
    TraceLog(LOG_INFO, "STREAM mixed processor attached");

    PauseAudioStream(stream);
    RunFrames(2, "stream", "paused");
    ResumeAudioStream(stream);
    TraceLog(LOG_INFO, "STREAM pause/resume done");

    for (int frame = 0; frame < AUDIO_FRAMES; frame++)
    {
#if !defined(STREAM_CALLBACK)
        if (IsAudioStreamProcessed(stream))
        {
            for (int i = 0; i < AUDIO_STREAM_BUFFER; i++)
            {
                int j = (pcmOffset + i)%44100;
                pcm[i*2 + 0] = StreamPcm[j*2 + 0];
                pcm[i*2 + 1] = StreamPcm[j*2 + 1];
            }
            pcmOffset = (pcmOffset + AUDIO_STREAM_BUFFER)%44100;
            UpdateAudioStream(stream, pcm, AUDIO_STREAM_BUFFER);
        }
#endif
        DrawFrame(frame, "stream", TextFormat("playing=%d", IsAudioStreamPlaying(stream)));
        if ((frame > 0) && ((frame%60) == 0)) TraceLog(LOG_INFO, "STREAM FRAME %d  fps %d", frame, GetFPS());
    }

    StopAudioStream(stream);
    DetachAudioMixedProcessor(MixedProcessor);
    TraceLog(LOG_INFO, "STREAM mixed processor detached");
#if defined(STREAM_PROCESSOR)
    DetachAudioStreamProcessor(stream, StreamProcessor);
#endif
    UnloadAudioStream(stream);
}
#endif

#if defined(MODE_TONE) || defined(MODE_SOUND)
static void ToneMode(void)
{
    Wave wave = MakeWave(AUDIO_WAVE_MS, 1000.0f);
    Sound tone = LoadSoundFromWave(wave);
    FreeWaveData(&wave);

    TraceLog(LOG_INFO, "AUDIO: tone valid=%d", IsSoundValid(tone));
    PlaySound(tone);
    RunFrames(AUDIO_FRAMES, "tone", "sound");
    TraceLog(LOG_INFO, "TONE playing=%d", IsSoundPlaying(tone));
    UnloadSound(tone);
}
#endif

int main(void)
{
#ifdef LOG_FILE
    // Long runs exceed PSPLink's SIO buffer; write the log to host0 instead.
    if (freopen(LOG_FILE, "w", stdout) == NULL) { }
    setvbuf(stdout, NULL, _IOLBF, 0);
#endif

    InitWindow(480, 272, "audio smoke (P9-S5)");
    SetTargetFPS(60);

    TraceLog(LOG_INFO, "AUDIO build b5");
    InitAudioDevice();
    if (!IsAudioDeviceReady()) TraceLog(LOG_ERROR, "AUDIO: device is not ready");

#if defined(MODE_LIFECYCLE)
    LifecycleMode();
#elif defined(MODE_TRACKCHANGE)
    TrackChangeMode();
#elif defined(MODE_API)
    ApiMatrix();
#elif defined(MODE_MUSIC)
    MusicMode();
#elif defined(MODE_MUSICMIX)
    MusicMixMode();
#elif defined(MODE_STREAM)
    AudioStreamMode();
#elif defined(MODE_PITCH)
    PitchMode();
#elif defined(MODE_LEVELS)
    LevelsMode();
#elif defined(MODE_MIX)
    MixMode();
#elif defined(MODE_HEAP)
    HeapMode();
#else
    ToneMode();
#endif

    TraceLog(LOG_INFO, "DONE audio=%d", IsAudioDeviceReady());
    CloseAudioDevice();

    // Never CloseWindow()/sceKernelExitGame() here: both end the PSPLink session.
    sceKernelSleepThread();
    return 0;
}
