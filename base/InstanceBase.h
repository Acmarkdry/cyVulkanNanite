#pragma once

template<typename T>
class Singleton
{
public:
	static T* getManager()
	{
		if (instance == nullptr)
			instance = new T();
		return instance;
	}
	static void destroy()
	{
		
	}

	Singleton(Singleton const&) = delete;
	Singleton& operator=(Singleton const&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;
	Singleton() = default;

private:
	static T* instance;
};
