#pragma once

#include "common.h"

TEAR_BEGIN

template <typename T>
class Vector {
public:
	std::span<T> asSpan() const {
		return std::span<T>(data_, size_);
	}

	operator std::span<T>() const {
		return asSpan();
	}

	size_t size() const {
		return static_cast<std::size_t>(size_);
	}

	size_t capacity() const {
		return static_cast<std::size_t>(capacity_);
	}

	T& operator[](size_t index) {
		return data_[index];
	}

	const T& operator[](size_t index) const {
		return data_[index];
	}

	// Iterator syntax
	struct Iterator {
		T* ptr;
		T& operator*() {
			return *ptr;
		}
		Iterator& operator++() {
			++ptr;
			return *this;
		}
		bool operator!=(const Iterator& other) const {
			return ptr != other.ptr;
		}
	};
	Iterator begin() {
		return Iterator{ data_ };
	}
	Iterator end() {
		return Iterator{ data_ + size_ };
	}

private:
	/* 0x000 */ int32_t size_;
	/* 0x004 */ int32_t capacity_;
	/* 0x008 */ T* data_;
};

struct String {
	union {
		char ssoBuf_[16];
		char* heapBuf_;
	};
	uint64_t size_;
	uint64_t capacity_;

	bool isHeap() const {
		return (capacity_ >> 56) != 0;
	}

	operator std::string() const {
		if (isHeap()) {
			return std::string(heapBuf_);
		}
		else {
			return std::string(ssoBuf_);
		}
	}
	operator std::string_view() const {
		if (isHeap()) {
			return std::string_view(heapBuf_);
		}
		else {
			return std::string_view(ssoBuf_);
		}
	}
	operator const char*() const {
		return c_str();
	}

	const char* c_str() const {
		if (isHeap()) {
			return heapBuf_;
		}
		else {
			return ssoBuf_;
		}
	}
};

TEAR_END
