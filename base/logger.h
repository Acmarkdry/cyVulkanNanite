#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <memory>
#include <cstdarg>
#ifdef _WIN32
	#include <windows.h>
#endif

namespace Log
{
	// ============================================================================
	// 日志级别
	// ============================================================================
	enum class Level
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warn = 3,
		Error = 4,
		Fatal = 5,
		Off = 6
	};

	// ============================================================================
	// 控制台颜色（跨平台）
	// ============================================================================
	namespace Color
	{
#ifdef _WIN32
		inline void SetColor(int color)
		{
			static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, color);
		}

		constexpr int Reset = 7;
		constexpr int White = 15;
		constexpr int Gray = 8;
		constexpr int Cyan = 11;
		constexpr int Green = 10;
		constexpr int Yellow = 14;
		constexpr int Red = 12;
		constexpr int Magenta = 13;
#else
		inline void SetColor(int)
		{
		} // ANSI 用字符串实现
		constexpr int Reset = 0;
		constexpr int White = 1;
		constexpr int Gray = 2;
		constexpr int Cyan = 3;
		constexpr int Green = 4;
		constexpr int Yellow = 5;
		constexpr int Red = 6;
		constexpr int Magenta = 7;
#endif
	}

	// ============================================================================
	// Logger 核心类
	// ============================================================================
	class Logger
	{
	public:
		static Logger& Instance()
		{
			static Logger instance;
			return instance;
		}

		// 设置最小日志级别
		void SetLevel(Level level) { m_minLevel = level; }
		Level GetLevel() const { return m_minLevel; }

		// 启用/禁用控制台输出
		void EnableConsole(bool enable) { m_consoleEnabled = enable; }

		// 启用/禁用颜色
		void EnableColor(bool enable) { m_colorEnabled = enable; }

		// 设置日志文件
		bool SetLogFile(const std::string& filename)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_fileStream.is_open())
			{
				m_fileStream.close();
			}
			m_fileStream.open(filename, std::ios::out | std::ios::app);
			return m_fileStream.is_open();
		}

		void CloseLogFile()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_fileStream.is_open())
			{
				m_fileStream.close();
			}
		}

		// 核心日志函数
		void Log(Level level, const char* file, int line, const char* func, const std::string& message)
		{
			if (level < m_minLevel) return;

			std::lock_guard<std::mutex> lock(m_mutex);

			// 获取时间戳
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				now.time_since_epoch()) % 1000;

			std::tm tm_buf;
#ifdef _WIN32
			localtime_s(&tm_buf, &time);
#else
			localtime_r(&time, &tm_buf);
#endif

			// 提取文件名（去掉路径）
			std::string filename = file;
			size_t pos = filename.find_last_of("/\\");
			if (pos != std::string::npos)
			{
				filename = filename.substr(pos + 1);
			}

			// 格式化日志
			std::ostringstream oss;
			oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
				<< '.' << std::setfill('0') << std::setw(3) << ms.count()
				<< " [" << LevelToString(level) << "] "
				<< "[" << filename << ":" << line << "] "
				<< message;

			std::string logLine = oss.str();

			// 输出到控制台
			if (m_consoleEnabled)
			{
				PrintToConsole(level, logLine);
			}

			// 输出到文件
			if (m_fileStream.is_open())
			{
				m_fileStream << logLine << std::endl;
				m_fileStream.flush();
			}
		}

		// printf 风格的格式化
		template <typename... Args>
		void LogFormat(Level level, const char* file, int line, const char* func,
		               const char* fmt, Args... args)
		{
			if (level < m_minLevel) return;

			char buffer[4096];
			snprintf(buffer, sizeof(buffer), fmt, args...);
			Log(level, file, line, func, std::string(buffer));
		}

	private:
		Logger() = default;
		~Logger() { CloseLogFile(); }
		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;

		const char* LevelToString(Level level)
		{
			switch (level)
			{
			case Level::Trace: return "TRACE";
			case Level::Debug: return "DEBUG";
			case Level::Info: return "INFO ";
			case Level::Warn: return "WARN ";
			case Level::Error: return "ERROR";
			case Level::Fatal: return "FATAL";
			default: return "?????";
			}
		}

		void PrintToConsole(Level level, const std::string& message)
		{
#ifdef _WIN32
			if (m_colorEnabled)
			{
				Color::SetColor(GetColorCode(level));
			}
			std::cout << message << std::endl;
			if (m_colorEnabled)
			{
				Color::SetColor(Color::Reset);
			}
#else
			if (m_colorEnabled)
			{
				std::cout << GetAnsiColor(level) << message << "\033[0m" << std::endl;
			}
			else
			{
				std::cout << message << std::endl;
			}
#endif
		}

#ifdef _WIN32
		int GetColorCode(Level level)
		{
			switch (level)
			{
			case Level::Trace: return Color::Gray;
			case Level::Debug: return Color::Cyan;
			case Level::Info: return Color::Green;
			case Level::Warn: return Color::Yellow;
			case Level::Error: return Color::Red;
			case Level::Fatal: return Color::Magenta;
			default: return Color::White;
			}
		}
#else
		const char* GetAnsiColor(Level level)
		{
			switch (level)
			{
			case Level::Trace: return "\033[90m"; // 灰色
			case Level::Debug: return "\033[36m"; // 青色
			case Level::Info: return "\033[32m"; // 绿色
			case Level::Warn: return "\033[33m"; // 黄色
			case Level::Error: return "\033[31m"; // 红色
			case Level::Fatal: return "\033[35m"; // 紫色
			default: return "\033[0m";
			}
		}
#endif

		Level m_minLevel = Level::Trace;
		bool m_consoleEnabled = true;
		bool m_colorEnabled = true;
		std::mutex m_mutex;
		std::ofstream m_fileStream;
	};

	// ============================================================================
	// 流式日志辅助类
	// ============================================================================
	class LogStream
	{
	public:
		LogStream(Level level, const char* file, int line, const char* func)
			: m_level(level), m_file(file), m_line(line), m_func(func)
		{
		}

		~LogStream()
		{
			Logger::Instance().Log(m_level, m_file, m_line, m_func, m_stream.str());
		}

		template <typename T>
		LogStream& operator<<(const T& value)
		{
			m_stream << value;
			return *this;
		}
		
		void Example();

	private:
		Level m_level;
		const char* m_file;
		int m_line;
		const char* m_func;
		std::ostringstream m_stream;
	};
} // namespace Log

// ============================================================================
// 便捷宏定义
// ============================================================================

// 流式日志（推荐）
#define LOG_TRACE Log::LogStream(Log::Level::Trace, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG Log::LogStream(Log::Level::Debug, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO  Log::LogStream(Log::Level::Info,  __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN  Log::LogStream(Log::Level::Warn,  __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR Log::LogStream(Log::Level::Error, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL Log::LogStream(Log::Level::Fatal, __FILE__, __LINE__, __FUNCTION__)

// printf 风格日志
#define LOGF_TRACE(fmt, ...) Log::Logger::Instance().LogFormat(Log::Level::Trace, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOGF_DEBUG(fmt, ...) Log::Logger::Instance().LogFormat(Log::Level::Debug, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOGF_INFO(fmt, ...)  Log::Logger::Instance().LogFormat(Log::Level::Info,  __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOGF_WARN(fmt, ...)  Log::Logger::Instance().LogFormat(Log::Level::Warn,  __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOGF_ERROR(fmt, ...) Log::Logger::Instance().LogFormat(Log::Level::Error, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOGF_FATAL(fmt, ...) Log::Logger::Instance().LogFormat(Log::Level::Fatal, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

// 条件日志
#define LOG_IF(condition, level) if(condition) LOG_##level
#define LOG_ASSERT(condition, msg) if(!(condition)) { LOG_FATAL << "Assertion failed: " << #condition << " - " << msg; std::abort(); }
