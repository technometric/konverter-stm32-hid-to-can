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
#include "upload.h"
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <combaseapi.h>
#include "json.hpp" // nlohmann json

using json = nlohmann::json;
constexpr int MAX_RETRIES = 10;
constexpr int RETRY_DELAY_MS = 500;

// ==========================================
// Helper
// ==========================================
static bool isValidAck(const char* ack) {
    if (!ack || strlen(ack) == 0) return false;
    std::string a(ack);
    if (a == "no response" || a == "NO RESPONSE" || a == "No Response") return false;
    return true;
}

// Kirim perintah dengan retry dan callback
#ifdef HID_MODE
// ========== HID VERSION ==========
static std::string sendUploadCommandWithRetry(
    const char* commandType,
    const char* frameData,
    size_t frameSize,
    UploadCallbacks* callbacks)
{
    int retryCount = 0;
    std::string ackStr;

    while (retryCount < MAX_RETRIES)
    {
        // Callback: Command Sent
        if (callbacks && callbacks->onCommandSent) {
            std::string hexData = toHex2(std::string(frameData, frameSize));
            callbacks->onCommandSent(commandType, hexData.c_str());
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
            if (callbacks && callbacks->onError) {
                std::ostringstream msg;
                msg << commandType << " - WriteFile failed: " << err;
                callbacks->onError(msg.str().c_str());
            }
            retryCount++;
            if (retryCount < MAX_RETRIES) {
                Sleep(RETRY_DELAY_MS);
            }
            continue;
        }

        logMsgHex(("TX " + std::string(commandType)).c_str(),
            (const uint8_t*)hidReport, 64);

        // Baca ACK
        Sleep(50);
        const char* ack = getResponse();
        if (ack) {
            logMsgHex(("ACK " + std::string(commandType)).c_str(),
                reinterpret_cast<const uint8_t*>(ack),
                strlen(ack));
            ackStr = ack;
        }
        else {
            ackStr.clear();
        }

        // Jika valid ACK
        if (isValidAck(ackStr.c_str())) {
            if (callbacks && callbacks->onResponseReceived)
                callbacks->onResponseReceived(commandType, ackStr.c_str());
            return ackStr;
        }

        // Kalau belum valid, retry
        retryCount++;
        if (retryCount < MAX_RETRIES) {
            if (callbacks && callbacks->onRetry)
                callbacks->onRetry(commandType, retryCount, MAX_RETRIES);
            Sleep(RETRY_DELAY_MS);
        }
        else {
            if (callbacks && callbacks->onError) {
                std::ostringstream err;
                err << commandType << " failed after "
                    << MAX_RETRIES << " retries - no valid ACK";
                callbacks->onError(err.str().c_str());
            }
        }
    }
    return ""; // gagal
}
#else
static std::string sendUploadCommandWithRetry(
    const char* commandType,
    const char* frameData,
    size_t frameSize,
    UploadCallbacks* callbacks)
{
    int retryCount = 0;
    std::string ackStr;

    while (retryCount < MAX_RETRIES)
    {
        // Callback: Command Sent
        if (callbacks && callbacks->onCommandSent) {
            std::string hexData = toHex2(std::string(frameData, frameSize));
            callbacks->onCommandSent(commandType, hexData.c_str());
        }

        // Kirim frame ke serial
        DWORD written;
        WriteFile(hSerial, frameData, (DWORD)frameSize, &written, NULL);
        logMsgHex(("TX " + std::string(commandType)).c_str(),
            (const uint8_t*)frameData, frameSize);

        // Baca ACK
        Sleep(50);
        const char* ack = getResponse();
        if (ack) {
            logMsgHex(("ACK " + std::string(commandType)).c_str(),
                reinterpret_cast<const uint8_t*>(ack),
                strlen(ack));
            ackStr = ack;
        }
        else {
            ackStr.clear();
        }

        // Jika valid ACK
        if (isValidAck(ackStr.c_str())) {
            if (callbacks && callbacks->onResponseReceived)
                callbacks->onResponseReceived(commandType, ackStr.c_str());
            return ackStr;
        }

        // Kalau belum valid, retry
        retryCount++;
        if (retryCount < MAX_RETRIES) {
            if (callbacks && callbacks->onRetry)
                callbacks->onRetry(commandType, retryCount, MAX_RETRIES);
            Sleep(RETRY_DELAY_MS);
        }
        else {
            if (callbacks && callbacks->onError) {
                std::ostringstream err;
                err << commandType << " failed after "
                    << MAX_RETRIES << " retries - no valid ACK";
                callbacks->onError(err.str().c_str());
            }
        }
    }
    return ""; // gagal
}
#endif
static char* dup_json(const std::string& s) {
    char* p = (char*)::CoTaskMemAlloc(s.size() + 1);
    if (!p) return nullptr;
    memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

// ==========================================
// Main Upload Function
// ==========================================
const char* __stdcall handleUploadWithCallback(
    const char* jsonPayload,
    UploadCallbacks* callbacks)
{
    static std::string lastResponse;
    lastResponse.clear();

    if (!jsonPayload) {
        if (callbacks && callbacks->onError)
            callbacks->onError("JSON payload is null");
        return dup_json("{}");
    }

    json root;
    try { root = json::parse(jsonPayload); }
    catch (const std::exception& e) {
        if (callbacks && callbacks->onError)
            callbacks->onError(("JSON parse error: " + std::string(e.what())).c_str());
        return dup_json("{}");
    }

    std::string idStr = root["id"].get<std::string>();
    std::string progId = root["progId"].get<std::string>();
    //int stepCount = root["stepCount"].get<int>();

    json result;
    result["progId"] = progId;
    json steps = json::array();
    
    // ==================================================
    // 1️⃣ Frame RP
    // ==================================================
    std::ostringstream p;
    p << static_cast<char>(0x05)
        << idStr
        << "RP"
        << progId;

    std::string payload = p.str();
    uint8_t cs = calcChecksum(payload);

    std::ostringstream frame;
    frame << payload
        << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << (int)cs
        << static_cast<char>(0x04);
    std::string out = frame.str();

    std::string ackStr = sendUploadCommandWithRetry("RP", out.data(), out.size(), callbacks);
    if (ackStr.empty()) {
        if (callbacks && callbacks->onError){
            callbacks->onError("{\"error\":\"RP failed\"}");
        }
        return dup_json("{\"error\":\"RP failed\"}");
    }

    // 🔹 Parsing langsung hasil ACK
    std::string progName;
    if (ackStr.size() > 10) {
        progName = ackStr.substr(7, 12);
        progName.erase(progName.find_last_not_of(" \0") + 1);
    }
    result["progName"] = progName;
    auto dotPos = ackStr.find('.');   
    int stepCount = 0; 
    // Ambil 2 karakter setelah titik
    if (dotPos != std::string::npos && dotPos + 2 < ackStr.size()) {
        std::string counterStr = ackStr.substr(dotPos + 1, 2);
        try { stepCount = std::stoi(counterStr, nullptr, 10); } catch(...) { stepCount = 0; }
    } else {
        if (callbacks && callbacks->onError) callbacks->onError("{\"error\":\"Bad RP format\"}");
        logMsg("Bad RP format: %s",ackStr.c_str());
        return dup_json("{\"error\":\"Bad RP format\"}");
    }
    
    int totalSteps = stepCount + 1; // RP + RS loop
    int currentStep = 0;
    currentStep++;
    if (callbacks && callbacks->onProgress)
        callbacks->onProgress(currentStep, totalSteps, "Sending RP");
    Sleep(100);
    // ==================================================
    // 2️⃣ Frame RS loop
    // ==================================================
    for (int i = 1; i <= stepCount; ++i) {
        currentStep++;
        if (callbacks && callbacks->onProgress) {
            std::ostringstream s;
            s << "Sending RS " << i << "/" << stepCount;
            callbacks->onProgress(currentStep, totalSteps, s.str().c_str());
        }

        std::ostringstream ps;
        ps << static_cast<char>(0x05)
            << idStr
            << "RS"
            << progId
            << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << i;

        std::string pay = ps.str();
        uint8_t cs2 = calcChecksum(pay);

        std::ostringstream fr;
        fr << pay
            << std::uppercase << std::setw(2) << std::setfill('0')
            << std::hex << (int)cs2
            << static_cast<char>(0x04);
        std::string out2 = fr.str();

        std::ostringstream cmd;
        cmd << "RS " << i;
        std::string ackS = sendUploadCommandWithRetry(cmd.str().c_str(), out2.data(), out2.size(), callbacks);
        if (ackS.empty()) {
            if (callbacks && callbacks->onError){
                callbacks->onError("{\"error\":\"RS failed\"}");
            }
            return dup_json("{\"error\":\"RS failed\"}");
        }
        json step;

        if (ackS.size() >= 21 && ackS.rfind("RS", 0) == 0) {
            std::string stepNumber = ackS.substr(5, 2);
            std::string stepId = ackS.substr(7, 2);
            std::string v1 = ackS.substr(9, 4);
            std::string v2 = ackS.substr(13, 4);
            std::string v3 = ackS.substr(17, 4);
            step["stepNumber"] = std::stoi(stepNumber, nullptr, 16);  // Base 16 (hex)
            step["stepId"] = std::stoi(stepId);
            step["param"] = json::array({
                std::stoi(v1), std::stoi(v2), std::stoi(v3)
                });
        }
        else {
            step["stepNumber"] = i;
            step["stepId"] = 0;
            step["param"] = json::array({ 0, 0, 0 });
        }
        steps.push_back(step);
        Sleep(300);
    }

    result["steps"] = steps;

    if (callbacks && callbacks->onProgress)
        callbacks->onProgress(totalSteps, totalSteps, "Upload Complete");

    // Return hasil JSON
    lastResponse = result.dump();
    OutputDebugStringA(("UPLOAD result: " + lastResponse + "\n").c_str());
    return dup_json(lastResponse.c_str());
}