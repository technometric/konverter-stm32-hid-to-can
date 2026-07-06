#define BUILDING_CBS32_DLL
#include <windows.h>
#include "mode.h"
#ifdef HID_MODE
    #define WINVER 0x0501
    #define _WIN32_WINNT 0x0501    
    #include <setupapi.h>
    #include <cfgmgr32.h>
    
    // INITGUID harus sebelum include GUID headers
    #define INITGUID
    #include <initguid.h>      // Pindah ke sini
    #include <devguid.h>
    #include <hidclass.h>
    
    extern "C"
    {
        #include <hidsdi.h>
    }
#endif

#include "download.h"
#include <string>
#include <sstream>
#include <iomanip>
#include "json.hpp" // nlohmann::json header-only
using json = nlohmann::json;

constexpr int MAX_RETRIES = 10;
constexpr int RETRY_DELAY_MS = 100;

// === Utility: safe debug print (agar aman di XP tanpa OutputDebugStringA) ===
static void debugPrint(const std::string& msg) {
#ifdef _WIN32
    OutputDebugStringA((msg + "\n").c_str());
#else
    printf("%s\n", msg.c_str());
#endif
}

// === Cek validitas ACK ===
static bool isValidAck(const char* ack) {
    if (!ack || !*ack) return false;
    std::string s(ack);
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return !(s == "" || s == "NO RESPONSE");
}

// === Kirim command dengan retry ===
#ifdef HID_MODE
static bool sendCommandWithRetry(
    const char* commandType,
    const char* frameData,
    size_t frameSize,
    DownloadCallbacks* cb)
{
    int retry = 0;
    bool ok = false;

    while (retry < MAX_RETRIES && !ok) {
        // Callback kirim command
        if (cb && cb->onCommandSent) {
            std::string hexData = toHex2(std::string(frameData, frameSize));
            cb->onCommandSent(commandType, hexData.c_str());
        }

        // Siapkan HID Report (64 bytes)
        int reportOffset = 1;
        unsigned char hidReport[65];
        memset(hidReport, 0, sizeof(hidReport));
        
        // Copy frame data ke report
        size_t copySize = (frameSize > 65) ? 65 : frameSize;
        memcpy(&hidReport[reportOffset], frameData, copySize);

        // Kirim HID Report
        BOOL writeOk = HidD_SetOutputReport(hHidDevice, hidReport, sizeof(hidReport));
    
        if (!writeOk) {
            DWORD err = GetLastError();
            if (cb && cb->onError) {
                std::ostringstream msg;
                msg << commandType << " - WriteFile failed: " << err;
                cb->onError(msg.str().c_str());
            }
            debugPrint(std::string(commandType) + " - Write error!");
            Sleep(RETRY_DELAY_MS);
            retry++;
            continue;
        }

        logMsgHex(("TX " + std::string(commandType)).c_str(),
                  (const uint8_t*)hidReport, 64);

        // Tunggu response dengan timeout
        const char* ack = getResponse();
        if (ack && strlen(ack) > 0)
            logMsgHex("ACK", reinterpret_cast<const uint8_t*>(ack), strlen(ack));

        if (isValidAck(ack)) {
            ok = true;
            if (cb && cb->onResponseReceived)
                cb->onResponseReceived(commandType, ack);
        } else {
            retry++;
            if (retry < MAX_RETRIES) {
                if (cb && cb->onRetry)
                    cb->onRetry(commandType, retry, MAX_RETRIES);

                std::ostringstream msg;
                msg << commandType << " - No ACK, retry " << retry << "/" << MAX_RETRIES;
                debugPrint(msg.str());
                Sleep(RETRY_DELAY_MS);
            } else {
                if (cb && cb->onError) {
                    std::ostringstream msg;
                    msg << commandType << " failed after " << MAX_RETRIES << " retries";
                    cb->onError(msg.str().c_str());
                }
                debugPrint(std::string(commandType) + " - Max retries reached!");
            }
        }
    }

    return ok;
}
#else
static bool sendCommandWithRetry(
    const char* commandType,
    const char* frameData,
    size_t frameSize,
    DownloadCallbacks* cb)
{
    int retry = 0;
    bool ok = false;

    while (retry < MAX_RETRIES && !ok) {
        // Callback kirim command
        if (cb && cb->onCommandSent) {
            std::string hexData = toHex2(std::string(frameData, frameSize));
            cb->onCommandSent(commandType, hexData.c_str());
        }

        DWORD written = 0;
        WriteFile(hSerial, frameData, (DWORD)frameSize, &written, NULL);
        logMsgHex(("TX " + std::string(commandType)).c_str(),
                  (const uint8_t*)frameData, frameSize);

        const char* ack = getResponse();
        if (ack && strlen(ack) > 0)
            logMsgHex("ACK", reinterpret_cast<const uint8_t*>(ack), strlen(ack));

        if (isValidAck(ack)) {
            ok = true;
            if (cb && cb->onResponseReceived)
                cb->onResponseReceived(commandType, ack);
        } else {
            retry++;
            if (retry < MAX_RETRIES) {
                if (cb && cb->onRetry)
                    cb->onRetry(commandType, retry, MAX_RETRIES);

                std::ostringstream msg;
                msg << commandType << " - No ACK, retry " << retry << "/" << MAX_RETRIES;
                debugPrint(msg.str());
                Sleep(RETRY_DELAY_MS);
            } else {
                if (cb && cb->onError) {
                    std::ostringstream msg;
                    msg << commandType << " failed after " << MAX_RETRIES << " retries";
                    cb->onError(msg.str().c_str());
                }
                debugPrint(std::string(commandType) + " - Max retries reached!");
            }
        }
    }

    return ok;
}
#endif
// === Fungsi utama Download ===
int __stdcall handleDownloadWithCallback(const char* jsonPayload, DownloadCallbacks* cb, bool test)
{
    if (!jsonPayload) {
        if (cb && cb->onError) cb->onError("JSON payload is null");
        return -1;
    }

    json root;
    try {
        root = json::parse(jsonPayload);
    } catch (const std::exception& e) {
        std::string err = std::string("JSON parse error: ") + e.what();
        debugPrint(err);
        if (cb && cb->onError) cb->onError(err.c_str());
        return -1;
    }

    std::string idStr   = root["id"].get<std::string>();
    std::string progId  = root["progId"].get<std::string>();
    std::string progName= root["progName"].get<std::string>();
    int stepCount       = (int)root["data"].size();
    int totalSteps      = stepCount + 2;
    int currentStep     = 0;

    // Pad atau trim nama program jadi 11 char
    if (progName.size() < 11) progName.append(11 - progName.size(), ' ');
    else if (progName.size() > 11) progName = progName.substr(0, 11);

    // --- WP OPEN ---
    {
        currentStep++;
        if (cb && cb->onProgress) cb->onProgress(currentStep, totalSteps, "Sending WP OPEN");

        std::ostringstream p;
        p << (char)0x05 << idStr << "WP" << progId
          << progName << std::setw(2) << std::setfill('0') << stepCount << "1";
        std::string payload = p.str();

        uint8_t cs = calcChecksum(payload);
        std::ostringstream frame;
        frame << payload << std::uppercase << std::setw(2) << std::setfill('0')
              << std::hex << (int)cs << (char)0x04;
        if(test){
            logMsgHex("TX WP OPEN ",
                  (const uint8_t*)frame.str().data(), frame.str().size());
            return 0;
        }
        if (!sendCommandWithRetry("WP OPEN", frame.str().data(), frame.str().size(), cb))
            return -2;
        Sleep(350);
    }

    // --- WS STEPS ---
    for (size_t i = 0; i < root["data"].size(); ++i) {
        currentStep++;
        const auto& s = root["data"][i];
        int sn = s["stepNumber"].get<int>();
        int sid = s["stepId"].get<int>();
        auto vals = s["stepValue"];

        if (cb && cb->onProgress) {
            std::ostringstream msg;
            msg << "Sending WS Step " << sn << "/" << stepCount;
            cb->onProgress(currentStep, totalSteps, msg.str().c_str());
        }

        std::ostringstream p;
        p << (char)0x05 << idStr << "WS" << progId
          << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << sn
            // stepId: convert ke DECIMAL format (12 -> "12")  
          << std::dec << std::setw(2) << std::setfill('0') << sid
          << std::setw(4) << std::setfill('0') << vals[0].get<int>()
          << std::setw(4) << std::setfill('0') << vals[1].get<int>()
          << std::setw(4) << std::setfill('0') << vals[2].get<int>();

        std::string payload = p.str();
        uint8_t cs = calcChecksum(payload);
        std::ostringstream frame;
        frame << payload << std::uppercase << std::setw(2) << std::setfill('0')
              << std::hex << (int)cs << (char)0x04;

        std::ostringstream cmd; cmd << "WS Step " << sn;

        if(test){
            logMsgHex("TX ",
                  (const uint8_t*)frame.str().data(), frame.str().size());
            return 0;
        }

        if (!sendCommandWithRetry(cmd.str().c_str(), frame.str().data(), frame.str().size(), cb))
            return -3;
        Sleep(300);
    }

    // --- WP CLOSE ---
    {
        currentStep++;
        if (cb && cb->onProgress) cb->onProgress(currentStep, totalSteps, "Sending WP CLOSE");

        std::ostringstream p;
        p << (char)0x05 << idStr << "WP" << progId << progName << "0" << "1" << "2";
        std::string payload = p.str();

        uint8_t cs = calcChecksum(payload);
        std::ostringstream frame;
        frame << payload << std::uppercase << std::setw(2) << std::setfill('0')
              << std::hex << (int)cs << (char)0x04;

        if(test){
            logMsgHex("TX CLOSE ",
                  (const uint8_t*)frame.str().data(), frame.str().size());
            return 0;
        }

        if (!sendCommandWithRetry("WP CLOSE", frame.str().data(), frame.str().size(), cb))
            return -4;

        if (cb && cb->onProgress)
            cb->onProgress(totalSteps, totalSteps, "Download Complete");
    }
    return 0;
}