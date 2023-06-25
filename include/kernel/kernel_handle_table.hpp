#pragma once

#include <mutex>

#include "kernel_object.hpp"
#include "result/result.hpp"

namespace KernelSpace {
	struct Handle {
		static const std::uint32_t IndexBits = 15;
		static const std::uint32_t LinearIdBits = 16;
		static const std::uint32_t ReservedBits = 1;
		static const std::uint32_t IndexMask = (1 << IndexBits) - 1;

		static_assert(IndexBits + LinearIdBits + ReservedBits == sizeof(uint32_t) * CHAR_BIT, "Invalid Handle size");

		std::uint32_t raw_handle;

		inline std::uint16_t get_index() {
			return raw_handle & IndexMask;
		}

		inline std::uint16_t get_linear_id() {
			return raw_handle >> IndexBits;
		}

		inline std::uint8_t get_reserved() {
			return raw_handle >> (IndexBits + LinearIdBits);
		}
	};

	class KHandleTable {
	private:
		union EntryInfo {
			std::uint16_t linear_id;
			std::uint16_t next_free_index;
		};

		EntryInfo *m_entries;
		KAutoObject **m_objects;
		std::size_t m_table_size;
		std::size_t m_count;
		std::uint16_t m_next_linear_id;
		std::size_t m_free_index;
		std::mutex m_lock;

		KHandleTable(const KHandleTable&) = delete;
		KHandleTable& operator=(const KHandleTable&) = delete;
		KHandleTable(KHandleTable&&) = delete;
		KHandleTable& operator=(KHandleTable&&) = delete;

	public:
		explicit KHandleTable() : m_entries(nullptr), m_objects(nullptr), m_table_size(0), m_count(0), m_lock() {}

		inline void initialize(std::size_t table_size) {
			std::scoped_lock lock(m_lock);

			m_entries = new EntryInfo[table_size];
			m_objects = new KAutoObject*[table_size];
			m_table_size = table_size;
			m_next_linear_id = 1;
			m_free_index = -1;
			m_count = 0;


			for (std::size_t i = 0; i < table_size; i++) {
				m_entries[i].next_free_index = i - 1;
				m_objects[i] = nullptr;
				m_free_index = i;
			}
		}

		Result::HorizonResult Add(Handle *out_handle, KAutoObject *object);
		Result::HorizonResult Remove(Handle handle);
	};
};