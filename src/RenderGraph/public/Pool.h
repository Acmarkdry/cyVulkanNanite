#pragma once
#include <vector>
#include <cassert>

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
				for (; id.asInt < pool.GetSize() && !pool.IsPresent(id); id.asInt++);
			}
			
			ID id;
			Pool<T> &pool;
		};
		
		Iterator begin()
		{
			return Iterator({0}, *this);
		}

		Iterator end()
		{
			return Iterator({GetSize()}, *this);
		}

		ID Add(T &&elem)
		{
			if (freeIDs.size() == 0)
			{
				ID id = {data.size()};
				data.emplace_back(std::move(elem));
				isPresent.emplace_back(true);
				return id;
			}
			else
			{
				ID id = freeIDs.back();
				freeIDs.pop_back();
				data[id.asInt] = std::move(elem);
				isPresent[id.asInt] = true;
				return id;
			}
		}

		void Release(const ID &id)
		{
			assert(isPresent[id.asInt]);
			isPresent[id.asInt] = false;
			freeIDs.push_back(id);
		}

		const T& Get(const ID& id) const
		{
			assert(id.asInt != size_t(-1));
			assert(id.asInt < data.size());
			assert(isPresent[id.asInt]);
			return data[id.asInt];
		}

		T& Get(const ID& id)
		{
			assert(id.asInt != size_t(-1));
			assert(id.asInt < data.size());
			assert(isPresent[id.asInt]);
			return data[id.asInt];
		}

		size_t GetSize()
		{
			return data.size();
		}

		bool IsPresent(ID id)
		{
			return isPresent[id.asInt];
		}
	
	private:
		std::vector<T> data;
		std::vector<bool> isPresent;
		std::vector<ID> freeIDs;
	};
}
