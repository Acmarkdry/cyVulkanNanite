#include "logger.h"

namespace Log
{
	void LogStream::Example()
	{
		auto &logger = Logger::Instance();
		logger.SetLevel(Level::Trace);
		logger.EnableColor(true);
		logger.SetLogFile("cyVulkanNanite.log");
		
		// 流式日志
		LOG_INFO << "Application started";
		LOG_DEBUG << "Application value" << 42 << "name" << "test";
		
		// printf风格日志
		int count = 180;
		float fps = 60.f;
		LOGF_INFO("Render %d frame", count);
		
		// 条件日志
		bool enableDebug = true;
		LOG_IF(enableDebug, DEBUG) << "this only logs if enable debug";
		
		// 断言
		int* ptr = new int(42);
		LOG_ASSERT(ptr != nullptr, "memory allocation failed");
		
		delete ptr;
		return;
	}
}

