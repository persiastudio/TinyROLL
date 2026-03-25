// ============================================================================
// TinyROLL.cpp — Video + Audio player DLL for GameMaker
// Media Foundation + WASAPI | Decoding Thread + Audio Master Clock (v5)
// ============================================================================
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
// AUDIO RING BUFFER
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
// GLOBAL STATE — Video
// ============================================================================
static IMFSourceReader* g_pReader = nullptr;
static UINT32               g_width = 0;
static UINT32               g_height = 0;
static double               g_duration = 0.0;
static double               g_position = 0.0;
static int                  g_status = 0;
static bool                 g_loop = false;
static double               g_volume = 1.0;
static std::vector<BYTE>    g_frameBuf;
static void* g_targetBuffer = nullptr;
static bool                 g_hasVideo = false;
static bool                 g_hasAudio = false;

static LARGE_INTEGER        g_perfFreq;
static LARGE_INTEGER        g_playStart;
static double               g_timeOffset = 0.0;
static bool                 g_firstFrame = true;
static LONGLONG             g_lastPTS = 0;

// Audio
static AudioRingBuffer      g_audioRing;
static UINT32               g_audioSampleRate = 0;
static UINT32               g_audioChannels = 0;
static UINT32               g_audioBitsPerSample = 0;

// WASAPI
static IMMDeviceEnumerator* g_pEnumerator = nullptr;
static IMMDevice* g_pDevice = nullptr;
static IAudioClient* g_pAudioClient = nullptr;
static IAudioRenderClient* g_pRenderClient = nullptr;
static HANDLE               g_hAudioThread = nullptr;
static volatile bool        g_audioRunning = false;
static UINT32               g_wasapiBufSize = 0;
static WAVEFORMATEX* g_pWasapiFmt = nullptr;

// Decoding Thread
static std::thread* g_decoderThread = nullptr;
static volatile bool        g_decoderRunning = false;
static std::mutex           g_frameMutex;
static std::mutex           g_readerMutex;
static std::vector<BYTE>    g_currentFrame;
static LONGLONG             g_currentFramePTS = 0;
static bool                 g_frameReady = false;
static double               g_audioLatency = 0.080;
static double               g_videoFrameRate = 30.0;
static int                  g_debugFrameCount = 0;
static DWORD                g_lastDebugTime = 0;

// ============================================================================
// HELPERS (mantidos iguais)
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
// WASAPI (mantido original)
// ============================================================================
static void StopWASAPI()
{
    g_audioRunning = false;
    if (g_hAudioThread)
    {
        WaitForSingleObject(g_hAudioThread, 2000);
        CloseHandle(g_hAudioThread);
        g_hAudioThread = nullptr;
    }
    if (g_pAudioClient) { g_pAudioClient->Stop(); g_pAudioClient->Release(); g_pAudioClient = nullptr; }
    if (g_pRenderClient) { g_pRenderClient->Release(); g_pRenderClient = nullptr; }
    if (g_pWasapiFmt) { CoTaskMemFree(g_pWasapiFmt); g_pWasapiFmt = nullptr; }
    if (g_pDevice) { g_pDevice->Release(); g_pDevice = nullptr; }
    if (g_pEnumerator) { g_pEnumerator->Release(); g_pEnumerator = nullptr; }
}

static DWORD WINAPI AudioThreadProc(LPVOID) { /* seu código original completo mantido */
    DWORD taskIndex = 0;
    HANDLE hTask = nullptr;
    HMODULE hAvrt = LoadLibraryA("avrt.dll");
    if (hAvrt)
    {
        typedef HANDLE(WINAPI* PFN)(LPCWSTR, LPDWORD);
        PFN pfn = (PFN)GetProcAddress(hAvrt, "AvSetMmThreadCharacteristicsW");
        if (pfn) hTask = pfn(L"Pro Audio", &taskIndex);
    }

    while (g_audioRunning)
    {
        if (!g_pAudioClient || !g_pRenderClient) { Sleep(1); continue; }
        UINT32 padding = 0;
        if (FAILED(g_pAudioClient->GetCurrentPadding(&padding))) { Sleep(1); continue; }
        UINT32 available = g_wasapiBufSize - padding;
        if (available == 0) { Sleep(1); continue; }

        BYTE* pData = nullptr;
        if (FAILED(g_pRenderClient->GetBuffer(available, &pData))) { Sleep(1); continue; }

        UINT32 bytesPerFrame = g_pWasapiFmt->nBlockAlign;
        UINT32 totalBytes = available * bytesPerFrame;
        size_t got = g_audioRing.Read(pData, totalBytes);

        if (got > 0 && g_volume < 0.999)
        {
            if (g_pWasapiFmt->wBitsPerSample == 32)
            {
                float* fData = (float*)pData;
                size_t count = got / 4;
                float vol = (float)g_volume;
                for (size_t i = 0; i < count; i++)
                    fData[i] *= vol;
            }
        }

        if (got < totalBytes)
            memset(pData + got, 0, totalBytes - got);

        g_pRenderClient->ReleaseBuffer(available, 0);
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
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&g_pEnumerator);
    if (FAILED(hr)) return false;
    hr = g_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &g_pDevice);
    if (FAILED(hr)) return false;
    hr = g_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&g_pAudioClient);
    if (FAILED(hr)) return false;
    hr = g_pAudioClient->GetMixFormat(&g_pWasapiFmt);
    if (FAILED(hr)) return false;
    REFERENCE_TIME bufDur = 500000;
    hr = g_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufDur, 0, g_pWasapiFmt, nullptr);
    if (FAILED(hr)) return false;
    hr = g_pAudioClient->GetBufferSize(&g_wasapiBufSize);
    if (FAILED(hr)) return false;
    hr = g_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&g_pRenderClient);
    if (FAILED(hr)) return false;

    g_audioRunning = true;
    g_hAudioThread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);
    hr = g_pAudioClient->Start();
    return SUCCEEDED(hr);
}

// ============================================================================
// CLOCK
// ============================================================================
static double GetPlaybackClock()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return g_timeOffset + (double)(now.QuadPart - g_playStart.QuadPart) / (double)g_perfFreq.QuadPart;
}

static double GetAudioMasterTime()
{
    return GetPlaybackClock() - g_audioLatency;
}

// ============================================================================
// DECODING THREAD (PACING MAIS RÍGIDO)
// ============================================================================
static void DecoderThreadProc()
{
    while (g_decoderRunning && g_pReader)
    {
        if (g_status != 1) { Sleep(5); continue; }

        double currentTime = GetAudioMasterTime();

        if (g_currentFramePTS > 0)
        {
            double frameTime = (double)g_currentFramePTS / 10000000.0;
            if (frameTime > currentTime + 0.055)
            {
                Sleep(4);
                continue;
            }
        }

        std::lock_guard<std::mutex> readerLock(g_readerMutex);

        DWORD flags = 0, streamIndex = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;

        HRESULT hr = g_pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            if (pSample) pSample->Release();
            if (g_loop)
            {
                PROPVARIANT var; PropVariantInit(&var);
                var.vt = VT_I8; var.hVal.QuadPart = 0;
                g_pReader->SetCurrentPosition(GUID_NULL, var);
                PropVariantClear(&var);
                g_audioRing.Clear();
                g_currentFramePTS = 0;
                continue;
            }
            g_status = 3;
            break;
        }

        if (pSample)
        {
            g_lastPTS = timestamp;
            g_position = (double)timestamp / 10000000.0;
            g_currentFramePTS = timestamp;

            IMFMediaBuffer* pBuf = nullptr;
            if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuf)))
            {
                BYTE* pData = nullptr;
                DWORD curLen = 0;
                if (SUCCEEDED(pBuf->Lock(&pData, nullptr, &curLen)))
                {
                    std::lock_guard<std::mutex> lock(g_frameMutex);
                    size_t pixels = (std::min)((size_t)curLen / 4, g_currentFrame.size() / 4);
                    for (size_t i = 0; i < pixels; i++)
                    {
                        size_t off = i * 4;
                        g_currentFrame[off + 0] = pData[off + 2];
                        g_currentFrame[off + 1] = pData[off + 1];
                        g_currentFrame[off + 2] = pData[off + 0];
                        g_currentFrame[off + 3] = 255;
                    }
                    g_frameReady = true;
                }
                pBuf->Unlock();
                pBuf->Release();
            }
            pSample->Release();
        }
    }
}

static void StartDecoderThread()
{
    if (g_decoderThread) return;
    g_decoderRunning = true;
    g_decoderThread = new std::thread(DecoderThreadProc);
}

static void StopDecoderThread()
{
    g_decoderRunning = false;
    if (g_decoderThread)
    {
        g_decoderThread->join();
        delete g_decoderThread;
        g_decoderThread = nullptr;
    }
}

// ============================================================================
// DECODE AUDIO
// ============================================================================
static bool DecodeAudioIntoRing(int maxSamples)
{
    if (!g_pReader || !g_hasAudio) return false;

    std::lock_guard<std::mutex> lock(g_readerMutex);

    for (int i = 0; i < maxSamples; i++)
    {
        if (g_audioRing.Count() > 1048576) break;

        DWORD flags = 0, streamIndex = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;

        HRESULT hr = g_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, &streamIndex, &flags, &timestamp, &pSample);

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            if (pSample) pSample->Release();
            return false;
        }

        if (pSample)
        {
            IMFMediaBuffer* pBuf = nullptr;
            if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuf)))
            {
                BYTE* pData = nullptr;
                DWORD maxLen, curLen;
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
    return true;
}

// ============================================================================
// CLOSE / CLEANUP
// ============================================================================
static void CloseReader()
{
    StopDecoderThread();
    StopWASAPI();

    if (g_pReader) { g_pReader->Release(); g_pReader = nullptr; }

    g_width = g_height = 0;
    g_duration = g_position = 0.0;
    g_status = 0;
    g_frameBuf.clear();
    g_currentFrame.clear();
    g_audioRing.Clear();
    g_hasVideo = false;
    g_hasAudio = false;
    g_firstFrame = true;
    g_timeOffset = 0.0;
    g_lastPTS = 0;
    g_currentFramePTS = 0;
    g_frameReady = false;
}

// ============================================================================
// CONFIGURE READER
// ============================================================================
static bool ConfigureReader(IMFSourceReader* pReader)
{
    HRESULT hr;
    g_videoFrameRate = 30.0;
    g_currentFramePTS = 0;
    g_frameReady = false;
    g_audioLatency = 0.080;
    g_debugFrameCount = 0;
    g_lastDebugTime = GetTickCount();

    // Vídeo
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
                UINT32 num = 0, den = 0;
                if (SUCCEEDED(MFGetAttributeRatio(pOut, MF_MT_FRAME_RATE, &num, &den)) && den != 0)
                    g_videoFrameRate = (double)num / (double)den;

                char dbg[256];
                sprintf_s(dbg, "[TinyROLL] Video: %ux%u @ %.3f fps\n", g_width, g_height, g_videoFrameRate);
                OutputDebugStringA(dbg);
                pOut->Release();
            }
            g_frameBuf.resize((size_t)g_width * g_height * 4, 0);
            g_currentFrame.resize((size_t)g_width * g_height * 4, 0);
        }
        else return false;
    }

    // Áudio
    {
        IMFMediaType* pType = nullptr;
        if (SUCCEEDED(MFCreateMediaType(&pType)))
        {
            pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
            hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pType);
            pType->Release();

            if (SUCCEEDED(hr))
            {
                g_hasAudio = true;
                IMFMediaType* pCurrent = nullptr;
                if (SUCCEEDED(pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pCurrent)))
                {
                    pCurrent->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &g_audioSampleRate);
                    pCurrent->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &g_audioChannels);
                    pCurrent->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &g_audioBitsPerSample);
                    pCurrent->Release();
                }
                size_t ringSize = (size_t)g_audioSampleRate * g_audioChannels * 8;
                g_audioRing.Init(ringSize);
            }
        }
    }

    PROPVARIANT var; PropVariantInit(&var);
    if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)) && var.vt == VT_UI8)
        g_duration = (double)var.uhVal.QuadPart / 10000000.0;
    PropVariantClear(&var);

    OutputDebugStringA("[TinyROLL] ConfigureReader OK - Strict Pacing v5\n");
    return true;
}

// ============================================================================
// VIDEO API (mantido o máximo possível do original)
// ============================================================================
TINYROLL_API double DLL_Video_Open(const char* path_utf8)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    CloseReader();

    std::wstring path = NormalizePath(Utf8ToWString(path_utf8));
    OutputDebugStringW((L"[TinyROLL] Opening: " + path + L"\n").c_str());

    IMFAttributes* pAttribs = nullptr;
    MFCreateAttributes(&pAttribs, 2);
    pAttribs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    pAttribs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    IMFSourceReader* pReader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), pAttribs, &pReader);
    if (pAttribs) { pAttribs->Release(); pAttribs = nullptr; }

    char dbg[128];
    sprintf_s(dbg, "[TinyROLL] HRESULT: 0x%08X\n", hr);
    OutputDebugStringA(dbg);

    if (FAILED(hr)) return 0.0;

    if (!ConfigureReader(pReader))
    {
        pReader->Release();
        return 0.0;
    }

    g_pReader = pReader;
    QueryPerformanceFrequency(&g_perfFreq);
    g_firstFrame = true;
    g_timeOffset = 0.0;
    g_lastPTS = 0;

    g_status = 2;
    return 1.0;
}

TINYROLL_API double DLL_Video_SetTargetBuffer(const char* ptr)
{
    g_targetBuffer = (void*)ptr;
    return 1.0;
}

TINYROLL_API double DLL_Video_CopyFrame(double size)
{
    if (!g_targetBuffer || g_frameBuf.empty()) return 0.0;
    size_t copy = (std::min)((size_t)size, g_frameBuf.size());
    memcpy(g_targetBuffer, g_frameBuf.data(), copy);
    return 1.0;
}

TINYROLL_API double DLL_Video_FrameReady()
{
    return (g_pReader && !g_frameBuf.empty()) ? 1.0 : 0.0;
}

TINYROLL_API double DLL_Video_Close()
{
    CloseReader();
    MFShutdown();
    CoUninitialize();
    return 1.0;
}

TINYROLL_API double DLL_Video_Play()
{
    OutputDebugStringA("[TinyROLL] DLL_Video_Play CALLED\n");

    if (!g_pReader) return 0.0;

    if (g_status == 3)
    {
        PROPVARIANT var; PropVariantInit(&var);
        var.vt = VT_I8; var.hVal.QuadPart = 0;
        g_pReader->SetCurrentPosition(GUID_NULL, var);
        PropVariantClear(&var);
        g_position = 0.0;
        g_timeOffset = 0.0;
        g_lastPTS = 0;
        g_audioRing.Clear();
    }

    g_timeOffset = g_position;
    QueryPerformanceCounter(&g_playStart);
    g_firstFrame = false;
    g_status = 1;
    g_currentFramePTS = 0;

    StartDecoderThread();
    if (g_hasAudio) StartWASAPI();

    OutputDebugStringA("[TinyROLL] Status set to PLAYING - Decoder Thread active\n");
    return 1.0;
}

TINYROLL_API double DLL_Video_Pause()
{
    OutputDebugStringA("[TinyROLL] DLL_Video_Pause CALLED\n");
    if (!g_pReader) return 0.0;
    if (g_status == 1) g_timeOffset = GetPlaybackClock();
    g_status = 2;
    return 1.0;
}

TINYROLL_API double DLL_Video_SetLoop(double enabled) { g_loop = (enabled != 0.0); return 1.0; }
TINYROLL_API double DLL_Video_SetVolume(double vol) { g_volume = (std::max)(0.0, (std::min)(1.0, vol)); return 1.0; }
TINYROLL_API double DLL_Video_GetVolume() { return g_volume; }

TINYROLL_API double DLL_Video_Seek(double seconds)
{
    if (!g_pReader) return 0.0;
    seconds = (std::max)(0.0, (std::min)(seconds, g_duration));
    PROPVARIANT var; PropVariantInit(&var);
    var.vt = VT_I8; var.hVal.QuadPart = (LONGLONG)(seconds * 10000000.0);
    HRESULT hr = g_pReader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    if (SUCCEEDED(hr))
    {
        g_position = seconds;
        g_timeOffset = seconds;
        g_lastPTS = (LONGLONG)(seconds * 10000000.0);
        g_currentFramePTS = g_lastPTS;
        g_audioRing.Clear();
        QueryPerformanceCounter(&g_playStart);
        if (g_status == 3) g_status = 2;
    }
    return SUCCEEDED(hr) ? 1.0 : 0.0;
}

TINYROLL_API double DLL_Video_Tick()
{
    if (!g_pReader || g_status != 1) return 0.0;

    if (g_hasAudio)
        DecodeAudioIntoRing(3);

    if (g_frameReady)
    {
        std::lock_guard<std::mutex> lock(g_frameMutex);
        if (!g_currentFrame.empty())
            memcpy(g_frameBuf.data(), g_currentFrame.data(), g_frameBuf.size());
    }

    return 1.0;
}

TINYROLL_API double DLL_Video_GetFramePtr()
{
    if (g_frameBuf.empty()) return 0.0;
    return (double)(int64_t)(uintptr_t)g_frameBuf.data();
}

TINYROLL_API double DLL_Video_GetWidth() { return (double)g_width; }
TINYROLL_API double DLL_Video_GetHeight() { return (double)g_height; }
TINYROLL_API double DLL_Video_GetDuration() { return g_duration; }
TINYROLL_API double DLL_Video_GetPosition() { return g_position; }
TINYROLL_API double DLL_Video_GetStatus() { return (double)g_status; }
TINYROLL_API double DLL_Video_HasAudio() { return g_hasAudio ? 1.0 : 0.0; }
TINYROLL_API double DLL_Video_HasVideo() { return g_hasVideo ? 1.0 : 0.0; }

// ============================================================================
// THUMB PARSER (COMPLETO)
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
    if (pFrame) pFrame->Release();
    if (pEncoder) pEncoder->Release();
    if (pStream) pStream->Release();
    if (pWICBitmap) pWICBitmap->Release();
    if (pFactory) pFactory->Release();
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
    if (hBitmap) DeleteObject(hBitmap);
    if (pFactory) pFactory->Release();
    if (pItem) pItem->Release();
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

        PROPVARIANT durVar; PropVariantInit(&durVar);
        if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durVar)) && durVar.vt == VT_UI8)
        {
            LONGLONG seekPos = durVar.uhVal.QuadPart / 10;
            PROPVARIANT seekVar; PropVariantInit(&seekVar);
            seekVar.vt = VT_I8; seekVar.hVal.QuadPart = seekPos;
            pReader->SetCurrentPosition(GUID_NULL, seekVar);
            PropVariantClear(&seekVar);
        }
        PropVariantClear(&durVar);

        DWORD si = 0, flags = 0; LONGLONG ts = 0; IMFSample* pSample = nullptr;
        if (FAILED(pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &si, &flags, &ts, &pSample)) || !pSample) break;

        IMFMediaBuffer* pBuf = nullptr;
        if (FAILED(pSample->ConvertToContiguousBuffer(&pBuf))) { pSample->Release(); break; }

        UINT32 w = 256, h = 144;
        IMFMediaType* pCur = nullptr;
        if (SUCCEEDED(pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCur)))
        {
            MFGetAttributeSize(pCur, MF_MT_FRAME_SIZE, &w, &h);
            pCur->Release();
        }

        BYTE* pData = nullptr; DWORD maxLen, curLen;
        if (SUCCEEDED(pBuf->Lock(&pData, &maxLen, &curLen)))
        {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = (LONG)w;
            bmi.bmiHeader.biHeight = -(LONG)h;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            HDC hdc = GetDC(nullptr); void* pvBits = nullptr;
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
        pBuf->Release(); pSample->Release();
    } while (false);
    if (pType) pType->Release();
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
                count++; continue;
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
        PROPVARIANT var; PropVariantInit(&var);
        if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)) && var.vt == VT_UI8)
            duration = (double)var.uhVal.QuadPart / 10000000.0;
        PropVariantClear(&var);
        pReader->Release();
    }
    MFShutdown();
    CoUninitialize();
    return duration;
}