#include "public/Pool.h"

#include <cassert>

namespace cyRenderGraph
{
	template <typename T>
	typename Pool<T>::Iterator Pool<T>::begin()
	{
		return Iterator({0}, *this);
	}

	template <typename T>
	typename Pool<T>::Iterator Pool<T>::end()
	{
		return Iterator({GetSize()}, *this);
	}

	template <typename T>
	typename Pool<T>::ID Pool<T>::Add(T&& elem)
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
			ID id = {data.back()};
			freeIDs.pop_back();
			data[id.asInt] = std::move(elem);
			isPresent[id.asInt] = true;
			return id;
		}
	}

	template <typename T>
	void Pool<T>::Release(const ID& id)
	{
		assert(isPresent[id.asInt]);
		isPresent[id.asInt] = false;
		freeIDs.push_back(id);
	}

	template <typename T>
	const T& Pool<T>::Get(const ID& id) const
	{
		assert(id.asInt != size_t(-1));
		assert(id.asInt < data.size());
		assert(isPresent[id.asInt]);
		return data[id.asInt];
	}

	template <typename T>
	T& Pool<T>::Get(const ID& id)
	{
		assert(id.asInt != size_t(-1));
		assert(id.asInt < data.size());
		assert(isPresent[id.asInt]);
		return data[id.asInt];
	}

	template <typename T>
	size_t Pool<T>::GetSize()
	{
		return data.size();
	}

	template <typename T>
	bool Pool<T>::IsPresent(ID id)
	{
		return isPresent[id.asInt];
	}
}
