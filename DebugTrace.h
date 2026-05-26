#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include "MessageLog.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace DebugTrace
{
    inline std::mutex& LogMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline std::string LogFilePath();

    inline std::ofstream& LogStream()
    {
        static std::ofstream stream(LogFilePath(), std::ios::app);
        return stream;
    }

    inline std::string LogFilePath()
    {
#ifdef _WIN32
        char modulePath[MAX_PATH]{};
        DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            std::string path(modulePath, length);
            size_t slash = path.find_last_of("\\/");
            if (slash != std::string::npos)
            {
                return path.substr(0, slash + 1) + "ai_trace.log";
            }
        }
#endif
        return "ai_trace.log";
    }

    inline bool ShouldMirrorToMessageLog(const std::string& text)
    {
        if (text.find("REGISTER") != std::string::npos) return false;
        if (text.find("MAP_ADD_UNIT") != std::string::npos) return false;
        if (text.find("DROP_") != std::string::npos) return false;
        if (text.find("MINIMAP_INVALID") != std::string::npos) return false;
        if (text.find("ENEMY_RECOG") != std::string::npos) return false;
        return true;
    }

    template <typename... Args>
    inline void Log(Args&&... args)
    {
        std::ostringstream line;
        (line << ... << std::forward<Args>(args));
        line << '\n';

        const std::string text = line.str();
        {
            std::lock_guard<std::mutex> lock(LogMutex());
            LogStream() << text;
            LogStream().flush();
        }

#ifdef _WIN32
        OutputDebugStringA(text.c_str());
#endif
        if (ShouldMirrorToMessageLog(text))
        {
            MessageLog::Instance().AddMessage(text);
        }
    }
}
