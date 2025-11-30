#pragma once

namespace cyRenderGraph
{
	template<typename HandleInfo, typename Factory>
	class RAIIHandle
	{
	public:
		RAIIHandle(const HandleInfo &info, bool isAttached = true) = delete;
		
		RAIIHandle()
		{
			isAttached = true;
		}
		RAIIHandle<HandleInfo, Factory> &operator=(const RAIIHandle<HandleInfo, Factory>&other) = delete;
		RAIIHandle<HandleInfo, Factory> &operator=(RAIIHandle<HandleInfo, Factory>&other) = delete;
		RAIIHandle<HandleInfo, Factory> &operator = (RAIIHandle<HandleInfo, Factory> &&other)
		{
			if (isAttached)
				this->info.Reset();
			
			this->isAttached = other.isAttached;
			other.isAttached = false;
			this->info = other.info;
			return *this;
		}
		RAIIHandle(RAIIHandle<HandleInfo, Factory>&& other)
		{
			isAttached = other.isAttached;
			other.isAttached = false;
			this->info = other.info;
		}
		~RAIIHandle()
		{
			if (isAttached)
			this->info.Reset();
		}
		void Detach()
		{
			isAttached = false;
		}
		void Reset()
		{
			Detach();
			info.Reset();
		}
		bool IsAttached()
		{
			return isAttached;
		}
		const HandleInfo &Get() const
		{
			return info;
		}
		const HandleInfo *operator ->() const
		{
			return &info;
		}
		
	private:
		friend Factory;
		bool isAttached;
		HandleInfo info;
	};
}