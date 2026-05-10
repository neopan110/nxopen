/**
 * @file error_handler.h
 * @brief HVAC箱体参数化系统 - 统一错误处理与日志
 * @details 提供分级日志、异常类型、NX Syslog/ListingWindow输出、文件日志。
 *
 * 【设计原则】
 * 1. 所有NX API调用的异常统一在此捕获和记录
 * 2. 日志同时输出到NX ListingWindow(用户可见)和文件(调试用)
 * 3. 错误码体系与NX UF错误码兼容
 *
 * @version 1.0
 * @date 2026-05-10
 */

#ifndef HVAC_ERROR_HANDLER_H
#define HVAC_ERROR_HANDLER_H

#include "hvac_common.h"

/* ============================================================
 * 错误码定义
 * ============================================================ */

enum class HvacErrorCode {
    SUCCESS = 0,

    // 参数错误 1xx
    PARAM_OUT_OF_RANGE = 100,
    PARAM_CONFLICT = 101,
    PARAM_NOT_FOUND = 102,
    PARAM_LOAD_FAILED = 103,

    // 几何构建错误 2xx
    FEATURE_CREATE_FAILED = 200,
    BOOLEAN_FAILED = 201,
    SKETCH_FAILED = 202,
    EXTRUDE_FAILED = 203,
    OFFSET_FAILED = 204,
    DRAFT_FAILED = 205,
    BODY_NULL = 206,

    // NX环境错误 3xx
    NX_NOT_INITIALIZED = 300,
    WORK_PART_NULL = 301,
    UF_API_ERROR = 302,
    NXOPEN_EXCEPTION = 303,
    UNDO_FAILED = 304,

    // 系统错误 4xx
    FILE_NOT_FOUND = 400,
    MEMORY_ERROR = 401,
    INTERNAL_ERROR = 499
};

/* ============================================================
 * 自定义异常类
 * ============================================================ */

/**
 * @class HvacException
 * @brief HVAC系统专用异常基类
 */
class HvacException : public std::runtime_error {
public:
    HvacException(HvacErrorCode code, const std::string& msg,
                  const char* file = "", int line = 0)
        : std::runtime_error(msg)
        , m_code(code)
        , m_file(file ? file : "")
        , m_line(line) {}

    HvacErrorCode code() const { return m_code; }
    const std::string& file() const { return m_file; }
    int line() const { return m_line; }

    std::string fullMessage() const {
        std::ostringstream oss;
        oss << "[HVAC Error " << static_cast<int>(m_code) << "] " << what();
        if (!m_file.empty()) {
            oss << " (" << m_file << ":" << m_line << ")";
        }
        return oss.str();
    }

private:
    HvacErrorCode m_code;
    std::string m_file;
    int m_line;
};

/** 构建异常 */
class HvacBuildException : public HvacException {
public:
    HvacBuildException(const std::string& msg, HvacErrorCode code = HvacErrorCode::FEATURE_CREATE_FAILED,
                       const char* file = "", int line = 0)
        : HvacException(code, msg, file, line) {}
};

/** 参数异常 */
class HvacParamException : public HvacException {
public:
    HvacParamException(const std::string& msg, HvacErrorCode code = HvacErrorCode::PARAM_OUT_OF_RANGE,
                       const char* file = "", int line = 0)
        : HvacException(code, msg, file, line) {}
};

/* ============================================================
 * 日志级别
 * ============================================================ */

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR_LVL = 3,  // 避免与Windows ERROR宏冲突
    FATAL = 4
};

/* ============================================================
 * 错误处理器 (单例)
 * ============================================================ */

/**
 * @class HvacErrorHandler
 * @brief 统一错误处理器
 * @details 线程安全的单例, 管理日志输出目标和级别过滤。
 */
class HvacErrorHandler {
public:
    static HvacErrorHandler& instance() {
        static HvacErrorHandler inst;
        return inst;
    }

    /* ---- 配置 ---- */

    /** 设置最低日志级别 */
    void setLogLevel(LogLevel level) { m_minLevel = level; }

    /** 设置日志文件路径 (空字符串=禁用文件日志) */
    void setLogFile(const std::string& path);

    /** 是否输出到NX ListingWindow */
    void setListingWindowOutput(bool enable) { m_useListingWindow = enable; }

    /* ---- 日志输出 ---- */

    /** 通用日志 */
    void log(LogLevel level, const std::string& message,
             const char* file = nullptr, int line = 0);

    /** 便捷接口 */
    static void logDebug(const std::string& msg) {
        instance().log(LogLevel::DEBUG, msg);
    }
    static void logInfo(const std::string& msg) {
        instance().log(LogLevel::INFO, msg);
    }
    static void logWarn(const std::string& msg) {
        instance().log(LogLevel::WARN, msg);
    }
    static void logError(const std::string& msg, const char* file = nullptr, int line = 0) {
        instance().log(LogLevel::ERROR_LVL, msg, file, line);
    }
    static void logFatal(const std::string& msg, const char* file = nullptr, int line = 0) {
        instance().log(LogLevel::FATAL, msg, file, line);
    }

    /* ---- NX异常处理 ---- */

    /**
     * @brief 处理NXOpen异常并记录
     * @param nxMsg NXException的GetMessage()
     * @param context 出错上下文描述
     */
    static void handleNxException(const std::string& nxMsg, const std::string& context,
                                  const char* file = nullptr, int line = 0);

    /**
     * @brief 处理UF API错误码
     * @param ufRetCode UF函数返回码
     * @param context 出错上下文描述
     * @return true=无错误, false=有错误(已记录)
     */
    static bool checkUfError(int ufRetCode, const std::string& context,
                             const char* file = nullptr, int line = 0);

    /* ---- 获取历史日志 ---- */

    /** 获取最近N条日志 */
    std::vector<std::string> getRecentLogs(int count = 50) const;

    /** 清空日志缓存 */
    void clearLogs();

private:
    HvacErrorHandler();
    ~HvacErrorHandler();
    HvacErrorHandler(const HvacErrorHandler&) = delete;
    HvacErrorHandler& operator=(const HvacErrorHandler&) = delete;

    void writeToFile(const std::string& formatted);
    void writeToListingWindow(const std::string& formatted);
    std::string formatMessage(LogLevel level, const std::string& msg,
                              const char* file, int line) const;
    const char* levelToString(LogLevel level) const;

    LogLevel m_minLevel = LogLevel::INFO;
    bool m_useListingWindow = true;
    std::string m_logFilePath;
    std::ofstream m_logFile;
    std::vector<std::string> m_logBuffer;   // 内存日志缓存
    static constexpr int MAX_LOG_BUFFER = 500;
};

/* ============================================================
 * 便捷宏
 * ============================================================ */

/** 记录错误并抛出异常 */
#define HVAC_THROW(code, msg) \
    do { \
        HvacErrorHandler::logError(msg, __FILE__, __LINE__); \
        throw HvacBuildException(msg, code, __FILE__, __LINE__); \
    } while(0)

/** 检查条件, 失败则抛出 */
#define HVAC_ASSERT(cond, code, msg) \
    do { \
        if (!(cond)) { HVAC_THROW(code, msg); } \
    } while(0)

/** 检查指针非空 */
#define HVAC_CHECK_NULL(ptr, msg) \
    HVAC_ASSERT((ptr) != nullptr, HvacErrorCode::BODY_NULL, msg)

/** 安全执行NX Builder操作(带异常捕获和Builder销毁) */
#define HVAC_SAFE_BUILDER(builderPtr, operations) \
    do { \
        try { \
            operations \
            (builderPtr)->Destroy(); \
            (builderPtr) = nullptr; \
        } catch (...) { \
            if ((builderPtr) != nullptr) { \
                (builderPtr)->Destroy(); \
                (builderPtr) = nullptr; \
            } \
            throw; \
        } \
    } while(0)

#endif /* HVAC_ERROR_HANDLER_H */
