#pragma once
#include <mutex>
#include <memory>

template<typename T>
class Singleton {
public:
	// 获取单例实例
	static T& getInstance() {
		static T instance;  // C++11 保证线程安全
		return instance;
	}

	// 禁止拷贝
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	// 禁止移动
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;

protected:
	Singleton() = default;
	virtual ~Singleton() = default;
};