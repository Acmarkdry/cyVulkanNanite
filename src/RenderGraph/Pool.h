#pragma once
#include <vector>

namespace cyRenderGraph
{
	template <typename T>
	class Pool
	{
	public:
		struct ID
		{
			ID(size_t val) :asInt(val) {}
			ID() : asInt(size_t(-1)) {}
			bool operator == (const ID &other) const
			{
				return this->asInt == other.asInt;
			}
			size_t asInt;
		};
		
		struct Iterator
		{
		public:
			Iterator(ID id, Pool<T> &pool):id(id), pool(pool)
			{
				Skip();
			}
			
			T &operator*() {
				return pool.Get(id);
			}
			void operator++() {
				id.asInt++;
				Skip();
			}
			bool operator != (const Iterator &other) const
			{
				return this->id.asInt != other.id.asInt;
			}
		
		private:	
			void Skip()
			{
				for (; id.asInt < pool.GetSize() && !pool.isPresent(id); id.asInt++);
			}
			
			ID id;
			Pool<T> &pool;
		};
		
		Iterator begin();
		Iterator end();
		
		ID Add(T &&elem);
		void Release(const ID &id);
		const T&Get(const ID& id) const;
		T& Get(const ID& id);
		size_t GetSize();
		bool IsPresent(ID id);
	
	private:
		std::vector<T> data;
		std::vector<bool> isPresent;
		std::vector<ID> freeIDs;
	};
}
