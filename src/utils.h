#pragma once
#include <source_location>
#include <iostream>
#include <format>  // C++20

namespace NaniteMesh {

	// 断言函数
	inline void NaniteAssert(
		bool condition,
		std::string_view message,
		const std::source_location& loc = std::source_location::current()
	) {
		if (!condition) {
			std::cerr << std::format(
				"Assertion failed: {}\n"
				"File: {}({}:{})\n"
				"Function: {}\n"
				"Message: {}\n",
				condition, 
				loc.file_name(), 
				loc.line(), 
				loc.column(),
				loc.function_name(),
				message
			);
			std::abort();
		}
	}

	// 日志函数模板（支持任意参数）
	template<typename... Args>
	void log_impl(
		std::format_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current()
	) {
		std::cout << std::format(
			"[{}:{}] ",
			loc.file_name(),
			loc.line()
		) << std::format(fmt, std::forward<Args>(args)...) << '\n';
	}
	
}
