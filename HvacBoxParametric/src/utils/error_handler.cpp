/**
 * @file error_handler.cpp
 * @brief 统一错误处理器实现
 * @version 1.0
 * @date 2026-05-10
 */

#include "utils/error_handler.h"
#include <ctime>
#include <iomanip>

/* ============================================================
 * 构造/析构
 * ============================================================ */

HvacErrorHandler::HvacErrorHandler()
{
    // 默认日志文件路径: 用户临时目录
#if HVAC_PLATFORM_WINDOWS
    const char* tmp = std::getenv("APPDATA");
    if (tmp) {
        m_logFilePath = std::string(tmp) + "\\hvac_parametric\\debug.log";
    }
#else
    const char* tmp = std::getenv("HOME");
    if (tmp) {
        m_logFilePath = std::string(tmp) + "/.hvac_parametric/debug.log";
    }
#endif
}

HvacErrorHandler::~HvacErrorHandler()
{
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

/* ============================================================
 * 配置
 * ============================================================ */

void HvacErrorHandler::setLogFile(const std::string& path)
{
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_logFilePath = path;
    if (!path.empty()) {
        m_logFile.open(path, std::ios::app);
    }
}

/* ============================================================
 * 日志输出
 * ============================================================ */

void HvacErrorHandler::log(LogLevel level, const std::string& message,
                           const char* file, int line)
{
    if (level < m_minLevel) return;

    std::string formatted = formatMessage(level, message, file, line);

    // 写入内存缓存
    m_logBuffer.push_back(formatted);
    if (static_cast<int>(m_logBuffer.size()) > MAX_LOG_BUFFER) {
        m_logBuffer.erase(m_logBuffer.begin());
    }

    // 写入文件
    writeToFile(formatted);

    // 写入NX ListingWindow
    if (m_useListingWindow && level >= LogLevel::INFO) {
        writeToListingWindow(formatted);
    }
}

/* ============================================================
 * NX异常处理
 * ============================================================ */

void HvacErrorHandler::handleNxException(const std::string& nxMsg,
                                         const std::string& context,
                                         const char* file, int line)
{
    std::string fullMsg = "NX Exception in [" + context + "]: " + nxMsg;
    instance().log(LogLevel::ERROR_LVL, fullMsg, file, line);
}

bool HvacErrorHandler::checkUfError(int ufRetCode, const std::string& context,
                                    const char* file, int line)
{
    if (ufRetCode == 0) return true;

    char errMsg[256] = {0};
    UF_get_fail_message(ufRetCode, errMsg);

    std::string fullMsg = "UF Error " + std::to_string(ufRetCode)
                        + " in [" + context + "]: " + errMsg;
    instance().log(LogLevel::ERROR_LVL, fullMsg, file, line);
    return false;
}

/* ============================================================
 * 历史查询
 * ============================================================ */

std::vector<std::string> HvacErrorHandler::getRecentLogs(int count) const
{
    int total = static_cast<int>(m_logBuffer.size());
    int start = (total > count) ? (total - count) : 0;
    return std::vector<std::string>(m_logBuffer.begin() + start, m_logBuffer.end());
}

void HvacErrorHandler::clearLogs()
{
    m_logBuffer.clear();
}

/* ============================================================
 * 内部格式化
 * ============================================================ */

std::string HvacErrorHandler::formatMessage(LogLevel level, const std::string& msg,
                                            const char* file, int line) const
{
    // 时间戳
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);

    std::ostringstream oss;
    oss << "[" << std::put_time(tm, "%H:%M:%S") << "]"
        << "[" << levelToString(level) << "] "
        << msg;

    if (file != nullptr && file[0] != '\0') {
        // 只取文件名(去掉路径)
        std::string fileName(file);
        auto pos = fileName.find_last_of("/\\");
        if (pos != std::string::npos) {
            fileName = fileName.substr(pos + 1);
        }
        oss << " (" << fileName << ":" << line << ")";
    }

    return oss.str();
}

const char* HvacErrorHandler::levelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::DEBUG:     return "DEBUG";
        case LogLevel::INFO:      return "INFO ";
        case LogLevel::WARN:      return "WARN ";
        case LogLevel::ERROR_LVL: return "ERROR";
        case LogLevel::FATAL:     return "FATAL";
        default:                  return "?????";
    }
}

void HvacErrorHandler::writeToFile(const std::string& formatted)
{
    if (!m_logFile.is_open() && !m_logFilePath.empty()) {
        m_logFile.open(m_logFilePath, std::ios::app);
    }
    if (m_logFile.is_open()) {
        m_logFile << formatted << "\n";
        m_logFile.flush();
    }
}

void HvacErrorHandler::writeToListingWindow(const std::string& formatted)
{
#if HVAC_USE_NXOPEN_CPP
    try {
        NXOpen::Session* session = NXOpen::Session::GetSession();
        if (session != nullptr) {
            NXOpen::ListingWindow* lw = session->ListingWindow();
            if (lw != nullptr) {
                if (!lw->IsOpen()) lw->Open();
                lw->WriteLine(formatted.c_str());
            }
        }
    } catch (...) {
        // ListingWindow写入失败不应中断主流程
    }
#else
    // NX12: 使用UF_UI_write_listing_window
    UF_UI_open_listing_window();
    UF_UI_write_listing_window(formatted.c_str());
    UF_UI_write_listing_window("\n");
#endif
}
