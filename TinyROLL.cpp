// ===============================================================================
// TinyROLL — Video + Audio player DLL for TinyBox OS (GameMaker 2024.14)
// Media Foundation + WASAPI | Fully autonomous threads
// Architecture:
//   - Audio decode thread: reads MF audio samples → ring buffer (self-throttled)
//   - WASAPI render thread: ring buffer → speakers (master clock)
//   - Video decode thread:  reads MF video samples, paces to audio clock
//   - GM only calls: Open/Play/Pause/Seek/Close + reads frame buffer
// ===============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mfobjects.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <cmath>
#include <thread>
#include <atomic>
#include <condition_variable>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "propsys.lib")
#pragma warning(disable: 6031)
#define TINYROLL_API extern "C" __declspec(dllexport)
// ============================================================================
// AUDIO RING BUFFER (lock-free-ish, single producer single consumer safe)
// ============================================================================
class AudioRingBuffer
{
public:
    void Init(size_t sizeInBytes)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_buf.resize(sizeInBytes, 0);
        m_head = m_tail = m_count = 0;
    }
    void Clear()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_head = m_tail = m_count = 0;
    }
    size_t Write(const BYTE* data, size_t len)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_buf.empty()) return 0;
        size_t cap = m_buf.size();
        size_t avail = cap - m_count;
        size_t toWrite = (std::min)(len, avail);
        for (size_t i = 0; i < toWrite; i++)
        {
            m_buf[m_head] = data[i];
            m_head = (m_head + 1) % cap;
        }
        m_count += toWrite;
        return toWrite;
    }
    size_t Read(BYTE* out, size_t len)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_buf.empty()) return 0;
        size_t toRead = (std::min)(len, m_count);
        for (size_t i = 0; i < toRead; i++)
        {
            out[i] = m_buf[m_tail];
            m_tail = (m_tail + 1) % m_buf.size();
        }
        m_count -= toRead;
        return toRead;
    }
    size_t Count()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_count;
    }
private:
    std::vector<BYTE> m_buf;
    size_t m_head = 0, m_tail = 0, m_count = 0;
    std::mutex m_mtx;
};
// ============================================================================
// GLOBAL STATE
// ============================================================================
// Media Foundation reader (shared between audio + video decode threads)
static IMFSourceReader* g_pReader = nullptr;
static std::mutex        g_readerMutex;          // protects g_pReader calls
// Video info
static UINT32  g_width = 0;
static UINT32  g_height = 0;
static double  g_duration = 0.0;
static bool    g_hasVideo = false;
static bool    g_hasAudio = false;
// Playback control (atomic for cross-thread safety)
static std::atomic<int>    g_status{ 0 };     // 0=idle 1=playing 2=paused 3=ended
static std::atomic<bool>   g_loop{ false };
static std::atomic<double> g_volume{ 1.0 };
static std::atomic<double> g_position{ 0.0 };
// Frame buffer
static std::vector<BYTE>   g_frontFrame;         // GM reads from here
static std::mutex          g_frameMutex;
static void* g_targetBuffer = nullptr;
// Master clock — driven by total audio bytes consumed by WASAPI
static std::atomic<int64_t> g_audioBytesSent{ 0 }; // bytes pushed to WASAPI
static UINT32  g_wasapiSampleRate = 0;
static UINT32  g_wasapiChannels = 0;
static UINT32  g_wasapiBytesPerFrame = 0;         // nBlockAlign
// Seek support
static std::atomic<bool>   g_seekRequested{ false };
static std::atomic<double> g_seekTarget{ 0.0 };
static std::mutex          g_seekMutex;
static std::condition_variable g_seekCV;
// Playback start offset (for seek + pause resume)
static std::atomic<double> g_clockOffset{ 0.0 };    // seconds offset after seek
static std::atomic<int64_t> g_audioBytesSentAtResume{ 0 };
// Audio ring buffer
static AudioRingBuffer    g_audioRing;
static UINT32  g_audioSampleRate = 0;
static UINT32  g_audioChannels = 0;
// WASAPI
static IMMDeviceEnumerator* g_pEnumerator = nullptr;
static IMMDevice* g_pDevice = nullptr;
static IAudioClient* g_pAudioClient = nullptr;
static IAudioRenderClient* g_pRenderClient = nullptr;
static UINT32               g_wasapiBufSize = 0;
static WAVEFORMATEX* g_pWasapiFmt = nullptr;
// Threads
static std::thread* g_audioDecodeThread = nullptr;
static std::thread* g_videoDecodeThread = nullptr;
static HANDLE       g_hWasapiThread = nullptr;
static std::atomic<bool> g_threadsRunning{ false };
// ============================================================================
// HELPERS
// ============================================================================
static std::wstring Utf8ToWString(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &r[0], n);
    return r;
}
static std::wstring NormalizePath(const std::wstring& p)
{
    std::wstring r = p;
    std::replace(r.begin(), r.end(), L'/', L'\\');
    std::wstring c; c.reserve(r.size());
    for (size_t i = 0; i < r.size(); i++)
    {
        if (r[i] == L'\\' && i > 0 && r[i - 1] == L'\\') continue;
        c += r[i];
    }
    return c;
}
// ============================================================================
// MASTER CLOCK — based on audio bytes consumed
// ============================================================================
static double GetMasterClock()
{
    if (!g_hasAudio || g_wasapiBytesPerFrame == 0 || g_wasapiSampleRate == 0)
    {
        // No audio: fall back to wall clock (set elsewhere)
        return g_position.load();
    }
    int64_t bytesSent = g_audioBytesSent.load() - g_audioBytesSentAtResume.load();
    double secondsOfAudio = (double)bytesSent / (double)(g_wasapiSampleRate * g_wasapiBytesPerFrame);
    return g_clockOffset.load() + secondsOfAudio;
}
// ============================================================================
// WASAPI
// ============================================================================
static void StopWASAPI()
{
    // thread is stopped via g_threadsRunning flag externally
    if (g_hWasapiThread)
    {
        WaitForSingleObject(g_hWasapiThread, 3000);
        CloseHandle(g_hWasapiThread);
        g_hWasapiThread = nullptr;
    }
    if (g_pAudioClient) { g_pAudioClient->Stop();  g_pAudioClient->Release();  g_pAudioClient = nullptr; }
    if (g_pRenderClient) { g_pRenderClient->Release(); g_pRenderClient = nullptr; }
    if (g_pWasapiFmt) { CoTaskMemFree(g_pWasapiFmt); g_pWasapiFmt = nullptr; }
    if (g_pDevice) { g_pDevice->Release();       g_pDevice = nullptr; }
    if (g_pEnumerator) { g_pEnumerator->Release();   g_pEnumerator = nullptr; }
    g_wasapiSampleRate = 0;
    g_wasapiChannels = 0;
    g_wasapiBytesPerFrame = 0;
}
static DWORD WINAPI WasapiThreadProc(LPVOID)
{
    // Boost thread priority for glitch-free audio
    DWORD taskIndex = 0;
    HANDLE hTask = nullptr;
    HMODULE hAvrt = LoadLibraryA("avrt.dll");
    if (hAvrt)
    {
        typedef HANDLE(WINAPI* PFN)(LPCWSTR, LPDWORD);
        PFN pfn = (PFN)GetProcAddress(hAvrt, "AvSetMmThreadCharacteristicsW");
        if (pfn) hTask = pfn(L"Pro Audio", &taskIndex);
    }
    while (g_threadsRunning.load())
    {
        if (!g_pAudioClient || !g_pRenderClient || g_status.load() != 1)
        {
            Sleep(5);
            continue;
        }
        UINT32 padding = 0;
        if (FAILED(g_pAudioClient->GetCurrentPadding(&padding))) { Sleep(1); continue; }
        UINT32 available = g_wasapiBufSize - padding;
        if (available == 0) { Sleep(1); continue; }
        BYTE* pData = nullptr;
        if (FAILED(g_pRenderClient->GetBuffer(available, &pData))) { Sleep(1); continue; }
        UINT32 bytesPerFrame = g_pWasapiFmt->nBlockAlign;
        UINT32 totalBytes = available * bytesPerFrame;
        size_t got = g_audioRing.Read(pData, totalBytes);
        // Apply volume
        if (got > 0)
        {
            double vol = g_volume.load();
            if (vol < 0.999 && g_pWasapiFmt->wBitsPerSample == 32)
            {
                float* fData = (float*)pData;
                size_t count = got / 4;
                float fvol = (float)vol;
                for (size_t i = 0; i < count; i++)
                    fData[i] *= fvol;
            }
        }
        // Silence-fill remainder
        if (got < totalBytes)
            memset(pData + got, 0, totalBytes - got);
        g_pRenderClient->ReleaseBuffer(available, 0);
        // Track how many real bytes of audio we sent
        g_audioBytesSent.fetch_add((int64_t)got);
        // Update position from audio clock
        g_position.store(GetMasterClock());
        Sleep(2);
    }
    if (hTask && hAvrt)
    {
        typedef BOOL(WINAPI* PFN2)(HANDLE);
        PFN2 pfn2 = (PFN2)GetProcAddress(hAvrt, "AvRevertMmThreadCharacteristics");
        if (pfn2) pfn2(hTask);
    }
    if (hAvrt) FreeLibrary(hAvrt);
    return 0;
}
static bool StartWASAPI()
{
    if (g_pAudioClient) return true;
    HRESULT hr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&g_pEnumerator);
    if (FAILED(hr)) return false;
    hr = g_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &g_pDevice);
    if (FAILED(hr)) return false;
    hr = g_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&g_pAudioClient);
    if (FAILED(hr)) return false;
    hr = g_pAudioClient->GetMixFormat(&g_pWasapiFmt);
    if (FAILED(hr)) return false;
    // Store WASAPI format info for clock calculations
    g_wasapiSampleRate = g_pWasapiFmt->nSamplesPerSec;
    g_wasapiChannels = g_pWasapiFmt->nChannels;
    g_wasapiBytesPerFrame = g_pWasapiFmt->nBlockAlign;
    REFERENCE_TIME bufDur = 500000; // 50ms buffer
    hr = g_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufDur, 0, g_pWasapiFmt, nullptr);
    if (FAILED(hr)) return false;
    hr = g_pAudioClient->GetBufferSize(&g_wasapiBufSize);
    if (FAILED(hr)) return false;
    hr = g_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&g_pRenderClient);
    if (FAILED(hr)) return false;
    char dbg[256];
    sprintf_s(dbg, "[TinyROLL] WASAPI: %u Hz, %u ch, %u block align\n",
        g_wasapiSampleRate, g_wasapiChannels, g_wasapiBytesPerFrame);
    OutputDebugStringA(dbg);
    return true;
}
// ============================================================================
// AUDIO DECODE THREAD — self-throttled, feeds ring buffer
// ============================================================================
static void AudioDecodeThreadProc()
{
    while (g_threadsRunning.load())
    {
        // Only decode while playing
        if (g_status.load() != 1)
        {
            Sleep(10);
            continue;
        }
        // Handle seek
        if (g_seekRequested.load())
        {
            Sleep(5);
            continue; // seek is handled in video thread which resets both streams
        }
        // Throttle: keep ~300ms of audio buffered, no more
        size_t bytesPerSecond = (size_t)g_audioSampleRate * g_audioChannels * 4; // float32
        size_t maxBuffered = bytesPerSecond * 3 / 10; // 300ms
        if (g_audioRing.Count() >= maxBuffered)
        {
            Sleep(5);
            continue;
        }
        // Read one audio sample from MF
        std::lock_guard<std::mutex> readerLock(g_readerMutex);
        if (!g_pReader) break;
        DWORD flags = 0, streamIndex = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;
        HRESULT hr = g_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, &streamIndex, &flags, &timestamp, &pSample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            if (pSample) pSample->Release();
            // Audio ended — let video thread handle looping/end
            Sleep(20);
            continue;
        }
        if (pSample)
        {
            IMFMediaBuffer* pBuf = nullptr;
            if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuf)))
            {
                BYTE* pData = nullptr;
                DWORD maxLen = 0, curLen = 0;
                if (SUCCEEDED(pBuf->Lock(&pData, &maxLen, &curLen)))
                {
                    g_audioRing.Write(pData, curLen);
                    pBuf->Unlock();
                }
                pBuf->Release();
            }
            pSample->Release();
        }
    }
}
// ============================================================================
// VIDEO DECODE THREAD — paces frames to master clock
// ============================================================================
static void VideoDecodeThreadProc()
{
    LARGE_INTEGER perfFreq, perfNow;
    QueryPerformanceFrequency(&perfFreq);
    LARGE_INTEGER wallClockStart;
    QueryPerformanceCounter(&wallClockStart);
    double wallClockOffset = 0.0;

    while (g_threadsRunning.load())
    {
        if (g_status.load() != 1)
        {
            Sleep(5);
            if (g_status.load() == 1)
            {
                QueryPerformanceCounter(&wallClockStart);
                wallClockOffset = g_position.load();
            }
            continue;
        }

        if (g_seekRequested.load())
        {
            double seekTo = g_seekTarget.load();
            seekTo = (std::max)(0.0, (std::min)(seekTo, g_duration));
            std::lock_guard<std::mutex> readerLock(g_readerMutex);
            PROPVARIANT var;
            PropVariantInit(&var);
            var.vt = VT_I8;
            var.hVal.QuadPart = (LONGLONG)(seekTo * 10000000.0);
            HRESULT hr = g_pReader->SetCurrentPosition(GUID_NULL, var);
            PropVariantClear(&var);
            if (SUCCEEDED(hr))
            {
                g_audioRing.Clear();
                g_clockOffset.store(seekTo);
                g_audioBytesSentAtResume.store(g_audioBytesSent.load());
                g_position.store(seekTo);
                wallClockOffset = seekTo;
                QueryPerformanceCounter(&wallClockStart);
            }
            g_seekRequested.store(false);
            g_seekCV.notify_all();
            continue;
        }

        double currentTime;
        if (g_hasAudio)
        {
            currentTime = GetMasterClock();
        }
        else
        {
            QueryPerformanceCounter(&perfNow);
            currentTime = wallClockOffset +
                (double)(perfNow.QuadPart - wallClockStart.QuadPart) / (double)perfFreq.QuadPart;
            g_position.store(currentTime);
        }

        DWORD flags = 0, streamIndex = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;
        {
            std::lock_guard<std::mutex> readerLock(g_readerMutex);
            if (!g_pReader) break;
            HRESULT hr = g_pReader->ReadSample(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0, &streamIndex, &flags, &timestamp, &pSample);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
            {
                if (pSample) pSample->Release();
                if (g_loop.load())
                {
                    PROPVARIANT var;
                    PropVariantInit(&var);
                    var.vt = VT_I8;
                    var.hVal.QuadPart = 0;
                    g_pReader->SetCurrentPosition(GUID_NULL, var);
                    PropVariantClear(&var);
                    g_audioRing.Clear();
                    g_clockOffset.store(0.0);
                    g_audioBytesSentAtResume.store(g_audioBytesSent.load());
                    g_position.store(0.0);
                    wallClockOffset = 0.0;
                    QueryPerformanceCounter(&wallClockStart);
                    continue;
                }
                g_status.store(3);
                break;
            }
        }

        if (pSample)
        {
            double frameTime = (double)timestamp / 10000000.0;

            while (g_threadsRunning.load() && g_status.load() == 1 && !g_seekRequested.load())
            {
                double now;
                if (g_hasAudio)
                {
                    now = GetMasterClock();
                }
                else
                {
                    QueryPerformanceCounter(&perfNow);
                    now = wallClockOffset +
                        (double)(perfNow.QuadPart - wallClockStart.QuadPart) / (double)perfFreq.QuadPart;
                    g_position.store(now);
                }
                double diff = frameTime - now;
                if (diff <= 0.002) break;
                if (diff > 0.015)      Sleep(5);
                else if (diff > 0.005) Sleep(1);
                else                   Sleep(0);
            }

            if (!g_threadsRunning.load() || g_seekRequested.load())
            {
                pSample->Release();
                continue;
            }

            IMFMediaBuffer* pBuf = nullptr;
            if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuf)))
            {
                BYTE* pData = nullptr;
                DWORD curLen = 0;
                if (SUCCEEDED(pBuf->Lock(&pData, nullptr, &curLen)))
                {
                    // Escreve direto no frontFrame — sem double-buffer, sem Tick externo
                    std::lock_guard<std::mutex> lock(g_frameMutex);
                    size_t pixels = (std::min)((size_t)curLen / 4, g_frontFrame.size() / 4);
                    for (size_t i = 0; i < pixels; i++)
                    {
                        size_t off = i * 4;
                        g_frontFrame[off + 0] = pData[off + 2]; // R
                        g_frontFrame[off + 1] = pData[off + 1]; // G
                        g_frontFrame[off + 2] = pData[off + 0]; // B
                        g_frontFrame[off + 3] = 255;             // A
                    }
                    pBuf->Unlock();
                }
                pBuf->Release();
            }
            pSample->Release();
            g_position.store(frameTime);
        }
    }
}

// ============================================================================
// THREAD MANAGEMENT
// ============================================================================
static void StartAllThreads()
{
    if (g_threadsRunning.load()) return;
    g_threadsRunning.store(true);
    // WASAPI render thread
    if (g_hasAudio && g_pAudioClient)
    {
        g_hWasapiThread = CreateThread(nullptr, 0, WasapiThreadProc, nullptr, 0, nullptr);
        g_pAudioClient->Start();
    }
    // Audio decode thread
    if (g_hasAudio)
        g_audioDecodeThread = new std::thread(AudioDecodeThreadProc);
    // Video decode thread
    if (g_hasVideo)
        g_videoDecodeThread = new std::thread(VideoDecodeThreadProc);
}
static void StopAllThreads()
{
    g_threadsRunning.store(false);
    // Wake up anything waiting on seek
    g_seekCV.notify_all();
    if (g_audioDecodeThread)
    {
        g_audioDecodeThread->join();
        delete g_audioDecodeThread;
        g_audioDecodeThread = nullptr;
    }
    if (g_videoDecodeThread)
    {
        g_videoDecodeThread->join();
        delete g_videoDecodeThread;
        g_videoDecodeThread = nullptr;
    }
    StopWASAPI();
}
// ============================================================================
// CLOSE / CLEANUP
// ============================================================================
static void CloseReader()
{
    StopAllThreads();
    if (g_pReader) { g_pReader->Release(); g_pReader = nullptr; }
    g_width = g_height = 0;
    g_duration = 0.0;
    g_position.store(0.0);
    g_status.store(0);
    g_frontFrame.clear();
    g_audioRing.Clear();
    g_hasVideo = false;
    g_hasAudio = false;
    g_seekRequested.store(false);
    g_clockOffset.store(0.0);
    g_audioBytesSent.store(0);
    g_audioBytesSentAtResume.store(0);
    g_audioSampleRate = 0;
    g_audioChannels = 0;
}
// ============================================================================
// CONFIGURE READER — matches MF audio output to WASAPI format
// ============================================================================
static bool ConfigureReader(IMFSourceReader* pReader)
{
    HRESULT hr;
    // ---- Video ----
    {
        IMFMediaType* pType = nullptr;
        if (FAILED(MFCreateMediaType(&pType))) return false;
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);
        pType->Release();
        if (SUCCEEDED(hr))
        {
            g_hasVideo = true;
            IMFMediaType* pOut = nullptr;
            if (SUCCEEDED(pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pOut)))
            {
                MFGetAttributeSize(pOut, MF_MT_FRAME_SIZE, &g_width, &g_height);
                char dbg[256];
                sprintf_s(dbg, "[TinyROLL] Video: %ux%u\n", g_width, g_height);
                OutputDebugStringA(dbg);
                pOut->Release();
            }
            size_t frameBytes = (size_t)g_width * g_height * 4;
            g_frontFrame.resize(frameBytes, 0);
        }
        else
        {
            // No video stream — that's OK for audio-only files
            g_hasVideo = false;
        }
    }
    // ---- Audio — match WASAPI format exactly ----
    {
        IMFMediaType* pType = nullptr;
        if (g_pWasapiFmt && SUCCEEDED(MFCreateMediaType(&pType)))
        {
            pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
            pType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, g_pWasapiFmt->nSamplesPerSec);
            pType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, g_pWasapiFmt->nChannels);
            pType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
            pType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, g_pWasapiFmt->nChannels * 4);
            pType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                g_pWasapiFmt->nSamplesPerSec * g_pWasapiFmt->nChannels * 4);
            hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pType);
            pType->Release();
            if (SUCCEEDED(hr))
            {
                g_hasAudio = true;
                g_audioSampleRate = g_pWasapiFmt->nSamplesPerSec;
                g_audioChannels = g_pWasapiFmt->nChannels;
                // Ring buffer: ~2 seconds of audio
                size_t ringSize = (size_t)g_audioSampleRate * g_audioChannels * 4 * 2;
                g_audioRing.Init(ringSize);
                char dbg[256];
                sprintf_s(dbg, "[TinyROLL] Audio: %u Hz, %u ch (matched to WASAPI)\n",
                    g_audioSampleRate, g_audioChannels);
                OutputDebugStringA(dbg);
            }
        }
        if (!g_hasAudio)
        {
            // Try fallback: let MF pick its own float format
            IMFMediaType* pFallback = nullptr;
            if (SUCCEEDED(MFCreateMediaType(&pFallback)))
            {
                pFallback->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                pFallback->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
                hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pFallback);
                pFallback->Release();
                if (SUCCEEDED(hr))
                {
                    g_hasAudio = true;
                    IMFMediaType* pCurrent = nullptr;
                    if (SUCCEEDED(pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pCurrent)))
                    {
                        pCurrent->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &g_audioSampleRate);
                        pCurrent->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &g_audioChannels);
                        pCurrent->Release();
                    }
                    size_t ringSize = (size_t)g_audioSampleRate * g_audioChannels * 4 * 2;
                    g_audioRing.Init(ringSize);
                    OutputDebugStringA("[TinyROLL] Audio: fallback format (may mismatch WASAPI!)\n");
                }
            }
        }
    }
    if (!g_hasVideo && !g_hasAudio)
        return false;
    // Duration
    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var))
        && var.vt == VT_UI8)
    {
        g_duration = (double)var.uhVal.QuadPart / 10000000.0;
    }
    PropVariantClear(&var);
    OutputDebugStringA("[TinyROLL] ConfigureReader OK — autonomous v6\n");
    return true;
}
// ============================================================================
// PUBLIC API
// ============================================================================
TINYROLL_API double DLL_Video_Open(const char* path_utf8)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    CloseReader();
    // Start WASAPI FIRST so g_pWasapiFmt is available for ConfigureReader
    StartWASAPI();
    std::wstring path = NormalizePath(Utf8ToWString(path_utf8));
    OutputDebugStringW((L"[TinyROLL] Opening: " + path + L"\n").c_str());
    IMFAttributes* pAttribs = nullptr;
    MFCreateAttributes(&pAttribs, 2);
    pAttribs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    pAttribs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    IMFSourceReader* pReader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), pAttribs, &pReader);
    if (pAttribs) { pAttribs->Release(); pAttribs = nullptr; }
    if (FAILED(hr))
    {
        char dbg[128];
        sprintf_s(dbg, "[TinyROLL] Open FAILED: 0x%08X\n", hr);
        OutputDebugStringA(dbg);
        return 0.0;
    }
    if (!ConfigureReader(pReader))
    {
        pReader->Release();
        return 0.0;
    }
    g_pReader = pReader;
    g_status.store(2); // paused, ready to play
    return 1.0;
}
TINYROLL_API double DLL_Video_Play()
{
    if (!g_pReader) return 0.0;
    // If ended, seek to start
    if (g_status.load() == 3)
    {
        std::lock_guard<std::mutex> readerLock(g_readerMutex);
        PROPVARIANT var;
        PropVariantInit(&var);
        var.vt = VT_I8;
        var.hVal.QuadPart = 0;
        g_pReader->SetCurrentPosition(GUID_NULL, var);
        PropVariantClear(&var);
        g_audioRing.Clear();
        g_position.store(0.0);
        g_clockOffset.store(0.0);
        g_audioBytesSentAtResume.store(g_audioBytesSent.load());
    }
    // If paused, resume clock from current position
    if (g_status.load() == 2)
    {
        g_clockOffset.store(g_position.load());
        g_audioBytesSentAtResume.store(g_audioBytesSent.load());
    }
    g_status.store(1); // playing
    // Start threads if not already running
    StartAllThreads();
    OutputDebugStringA("[TinyROLL] PLAYING — autonomous threads active\n");
    return 1.0;
}
TINYROLL_API double DLL_Video_Pause()
{
    if (!g_pReader) return 0.0;
    if (g_status.load() == 1)
    {
        // Snapshot current time before pausing
        g_position.store(GetMasterClock());
    }
    g_status.store(2);
    OutputDebugStringA("[TinyROLL] PAUSED\n");
    return 1.0;
}
TINYROLL_API double DLL_Video_Seek(double seconds)
{
    if (!g_pReader) return 0.0;
    g_seekTarget.store(seconds);
    g_seekRequested.store(true);
    // Wait for video thread to process the seek (with timeout)
    if (g_threadsRunning.load())
    {
        std::unique_lock<std::mutex> lk(g_seekMutex);
        g_seekCV.wait_for(lk, std::chrono::milliseconds(500), [] {
            return !g_seekRequested.load();
            });
    }
    else
    {
        // Threads not running, do seek directly
        seconds = (std::max)(0.0, (std::min)(seconds, g_duration));
        std::lock_guard<std::mutex> readerLock(g_readerMutex);
        PROPVARIANT var;
        PropVariantInit(&var);
        var.vt = VT_I8;
        var.hVal.QuadPart = (LONGLONG)(seconds * 10000000.0);
        g_pReader->SetCurrentPosition(GUID_NULL, var);
        PropVariantClear(&var);
        g_audioRing.Clear();
        g_position.store(seconds);
        g_clockOffset.store(seconds);
        g_audioBytesSentAtResume.store(g_audioBytesSent.load());
        g_seekRequested.store(false);
    }
    return 1.0;
}
TINYROLL_API double DLL_Video_SetLoop(double enabled)
{
    g_loop.store(enabled != 0.0);
    return 1.0;
}
TINYROLL_API double DLL_Video_SetVolume(double vol)
{
    g_volume.store((std::max)(0.0, (std::min)(1.0, vol)));
    return 1.0;
}
TINYROLL_API double DLL_Video_GetVolume()
{
    return g_volume.load();
}
TINYROLL_API double DLL_Video_Close()
{
    CloseReader();
    MFShutdown();
    CoUninitialize();
    return 1.0;
}
TINYROLL_API double DLL_Video_SetTargetBuffer(const char* ptr)
{
    g_targetBuffer = (void*)ptr;
    return 1.0;
}
TINYROLL_API double DLL_Video_CopyFrame(double size)
{
    if (!g_targetBuffer || g_frontFrame.empty()) return 0.0;
    size_t copy = (std::min)((size_t)size, g_frontFrame.size());
    memcpy(g_targetBuffer, g_frontFrame.data(), copy);
    return 1.0;
}
TINYROLL_API double DLL_Video_FrameReady()
{
    return (g_pReader && !g_frontFrame.empty()) ? 1.0 : 0.0;
}
TINYROLL_API double DLL_Video_GetFramePtr()
{
    if (g_frontFrame.empty()) return 0.0;
    return (double)(int64_t)(uintptr_t)g_frontFrame.data();
}
TINYROLL_API double DLL_Video_GetWidth() { return (double)g_width; }
TINYROLL_API double DLL_Video_GetHeight() { return (double)g_height; }
TINYROLL_API double DLL_Video_GetDuration() { return g_duration; }
TINYROLL_API double DLL_Video_GetPosition() { return g_position.load(); }
TINYROLL_API double DLL_Video_GetStatus() { return (double)g_status.load(); }
TINYROLL_API double DLL_Video_HasAudio() { return g_hasAudio ? 1.0 : 0.0; }
TINYROLL_API double DLL_Video_HasVideo() { return g_hasVideo ? 1.0 : 0.0; }
// ============================================================================
// THUMBNAIL / UTILITY FUNCTIONS (unchanged)
// ============================================================================
static bool SaveBitmapAsPNG(HBITMAP hBitmap, const std::wstring& outputPath)
{
    IWICImagingFactory* pFactory = nullptr;
    IWICStream* pStream = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    IWICBitmap* pWICBitmap = nullptr;
    bool success = false;
    do
    {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory)))) break;
        if (FAILED(pFactory->CreateBitmapFromHBITMAP(hBitmap, nullptr, WICBitmapIgnoreAlpha, &pWICBitmap))) break;
        if (FAILED(pFactory->CreateStream(&pStream))) break;
        if (FAILED(pStream->InitializeFromFilename(outputPath.c_str(), GENERIC_WRITE))) break;
        if (FAILED(pFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &pEncoder))) break;
        if (FAILED(pEncoder->Initialize(pStream, WICBitmapEncoderNoCache))) break;
        if (FAILED(pEncoder->CreateNewFrame(&pFrame, nullptr))) break;
        if (FAILED(pFrame->Initialize(nullptr))) break;
        if (FAILED(pFrame->WriteSource(pWICBitmap, nullptr))) break;
        if (FAILED(pFrame->Commit())) break;
        success = SUCCEEDED(pEncoder->Commit());
    } while (false);
    if (pFrame)    pFrame->Release();
    if (pEncoder)  pEncoder->Release();
    if (pStream)   pStream->Release();
    if (pWICBitmap) pWICBitmap->Release();
    if (pFactory)  pFactory->Release();
    return success;
}
static bool TryShellThumbnail(const std::wstring& videoPath, const std::wstring& outputPath)
{
    IShellItem* pItem = nullptr;
    IShellItemImageFactory* pFactory = nullptr;
    HBITMAP hBitmap = nullptr;
    bool success = false;
    do
    {
        if (FAILED(SHCreateItemFromParsingName(videoPath.c_str(), nullptr, IID_PPV_ARGS(&pItem)))) break;
        if (FAILED(pItem->QueryInterface(IID_PPV_ARGS(&pFactory)))) break;
        SIZE size = { 256, 144 };
        if (FAILED(pFactory->GetImage(size, SIIGBF_BIGGERSIZEOK | SIIGBF_THUMBNAILONLY, &hBitmap))) break;
        success = SaveBitmapAsPNG(hBitmap, outputPath);
    } while (false);
    if (hBitmap)  DeleteObject(hBitmap);
    if (pFactory) pFactory->Release();
    if (pItem)    pItem->Release();
    return success;
}
static bool TryMFThumbnail(const std::wstring& videoPath, const std::wstring& outputPath)
{
    IMFSourceReader* pReader = nullptr;
    IMFMediaType* pType = nullptr;
    bool success = false;
    do
    {
        if (FAILED(MFCreateSourceReaderFromURL(videoPath.c_str(), nullptr, &pReader))) break;
        if (FAILED(MFCreateMediaType(&pType))) break;
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (FAILED(pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType))) break;
        PROPVARIANT durVar;
        PropVariantInit(&durVar);
        if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durVar))
            && durVar.vt == VT_UI8)
        {
            LONGLONG seekPos = durVar.uhVal.QuadPart / 10;
            PROPVARIANT seekVar;
            PropVariantInit(&seekVar);
            seekVar.vt = VT_I8;
            seekVar.hVal.QuadPart = seekPos;
            pReader->SetCurrentPosition(GUID_NULL, seekVar);
            PropVariantClear(&seekVar);
        }
        PropVariantClear(&durVar);
        DWORD si = 0, flags = 0;
        LONGLONG ts = 0;
        IMFSample* pSample = nullptr;
        if (FAILED(pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &si, &flags, &ts, &pSample))
            || !pSample) break;
        IMFMediaBuffer* pBuf = nullptr;
        if (FAILED(pSample->ConvertToContiguousBuffer(&pBuf))) { pSample->Release(); break; }
        UINT32 w = 256, h = 144;
        IMFMediaType* pCur = nullptr;
        if (SUCCEEDED(pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCur)))
        {
            MFGetAttributeSize(pCur, MF_MT_FRAME_SIZE, &w, &h);
            pCur->Release();
        }
        BYTE* pData = nullptr;
        DWORD maxLen, curLen;
        if (SUCCEEDED(pBuf->Lock(&pData, &maxLen, &curLen)))
        {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = (LONG)w;
            bmi.bmiHeader.biHeight = -(LONG)h;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            HDC hdc = GetDC(nullptr);
            void* pvBits = nullptr;
            HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
            ReleaseDC(nullptr, hdc);
            if (hBmp && pvBits)
            {
                memcpy(pvBits, pData, (std::min)((size_t)curLen, (size_t)(w * h * 4)));
                success = SaveBitmapAsPNG(hBmp, outputPath);
                DeleteObject(hBmp);
            }
            pBuf->Unlock();
        }
        pBuf->Release();
        pSample->Release();
    } while (false);
    if (pType)   pType->Release();
    if (pReader) pReader->Release();
    return success;
}
TINYROLL_API double DLL_GetFileSize(const char* filepath_utf8)
{
    std::wstring filepath = NormalizePath(Utf8ToWString(filepath_utf8));
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &attrs))
        return -1.0;
    ULARGE_INTEGER size;
    size.HighPart = attrs.nFileSizeHigh;
    size.LowPart = attrs.nFileSizeLow;
    return (double)size.QuadPart;
}
TINYROLL_API double DLL_ParseThumbs(const char* folder_utf8)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    std::wstring folder = NormalizePath(Utf8ToWString(folder_utf8));
    std::wstring thumbsFolder = folder + L"\\Thumbs";
    CreateDirectoryW(thumbsFolder.c_str(), nullptr);
    SetFileAttributesW(thumbsFolder.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    std::vector<std::wstring> exts = { L"*.mp4", L"*.avi", L"*.mkv", L"*.mov", L"*.wmv", L"*.flv" };
    int count = 0;
    for (auto& ext : exts)
    {
        std::wstring searchPath = folder + L"\\" + ext;
        WIN32_FIND_DATAW fd = {};
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do
        {
            std::wstring filename = fd.cFileName;
            std::wstring videoPath = folder + L"\\" + filename;
            size_t dot = filename.find_last_of(L".");
            std::wstring base = (dot != std::wstring::npos) ? filename.substr(0, dot) : filename;
            std::wstring thumbPath = thumbsFolder + L"\\" + base + L".png";
            if (GetFileAttributesW(thumbPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                count++;
                continue;
            }
            if (!TryShellThumbnail(videoPath, thumbPath))
                TryMFThumbnail(videoPath, thumbPath);
            count++;
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    MFShutdown();
    CoUninitialize();
    return (double)count;
}
TINYROLL_API double DLL_GetVideoDuration(const char* filepath_utf8)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    std::wstring filepath = NormalizePath(Utf8ToWString(filepath_utf8));
    double duration = 0.0;
    IMFSourceReader* pReader = nullptr;
    if (SUCCEEDED(MFCreateSourceReaderFromURL(filepath.c_str(), nullptr, &pReader)))
    {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var))
            && var.vt == VT_UI8)
        {
            duration = (double)var.uhVal.QuadPart / 10000000.0;
        }
        PropVariantClear(&var);
        pReader->Release();
    }
    MFShutdown();
    CoUninitialize();
    return duration;
}
