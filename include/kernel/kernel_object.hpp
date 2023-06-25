#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

#include "helpers.hpp"

namespace KernelSpace {

	enum class KObjectTypeId : std::uint8_t {
		AutoObject,
		SynchronizationObject,
		Event = 0x1F,
		Semaphore = 0x2F,
		Timer = 0x35,
		Mutex = 0x39,
		Debug = 0x4D,
		ServerPort = 0x55,
		Dma = 0x59,
		ClientPort = 0x65,
		Session = 0x70,
		CodeSet = 0x65,
		Thread = 0x8D,
		AddressArbiter = 0x98,
		ServerSession = 0x95,
		ClientSession = 0xA5,
		SharedMemory = 0xB0,
		Port = 0xA8,
		Process = 0xC5,
		ResourceLimit = 0xC8,
	};

	struct KTypeObj {
		const char *name;
		KObjectTypeId typeId;
	};

	#define KERNEL_AUTO_OBJECT_NAME(cls) #cls

	#define DEFINE_KERNEL_AUTOOBJECT(cls, type) 											\
		cls(const cls&) = delete; 															\
		cls& operator=(const cls&) = delete; 												\
		cls(cls&&) = delete; 																\
		cls& operator=(cls&&) = delete; 													\
		private:																			\
			static constexpr inline KObjectTypeId ObjectType = KObjectTypeId::type; 		\
			static constexpr inline const char *ObjectName = KERNEL_AUTO_OBJECT_NAME(cls);	\
		public:																				\
			virtual KTypeObj GetTypeObj() const { return KTypeObj{ObjectName, ObjectType}; }

	class KAutoObject {
	private:
		std::atomic<std::uint32_t> m_refcount;

	public:
		inline explicit KAutoObject() : m_refcount(0) {}


		// NOTE: This method is supposed to be virtual as it depends on object resource limits + slab.
		// TODO: Make this virtual once resource limits is a thing.
		inline void destroy() {
			// TODO: Resource limits release
			finalize();
			// NOTE: This normally free the object from its slab free. 
			delete this;
		}

		virtual void initialize() {}
		virtual void finalize() {}

		inline void incrementRefcount() {
			std::uint32_t current = m_refcount.load();
			do {
				if (current == 0) {
					Helpers::panic("incrementRefcount: current is zero!");
				} else if (current > current + 1) {
					Helpers::panic("incrementRefcount: current overflowed!");
				}
			} while (!m_refcount.compare_exchange_weak(current, current + 1, std::memory_order_relaxed));
		}

		inline void decrementRefcount() {
			std::uint32_t current = m_refcount.load();
			do {
				if (current == 0) {
					Helpers::panic("decrementRefcount: current is zero!");
				}
			} while (!m_refcount.compare_exchange_weak(current, current - 1, std::memory_order_relaxed));

			if (current == 0) {
				destroy();
			}
		}

		template <typename T>
		static inline T *create() {
			static_assert(std::is_base_of<KAutoObject, T>::value, "create should be called with T derived from KAutoObject");

			T *res = new T();

			KAutoObject *tmp = reinterpret_cast<KAutoObject*>(res);
			tmp->incrementRefcount();
			tmp->initialize();

			return res;
		}

		DEFINE_KERNEL_AUTOOBJECT(KAutoObject, AutoObject);
	};

	template<typename T>
	class KScopedAutoObject {
		static_assert(std::is_base_of<KAutoObject, T>::value, "T sbould be derived from KAutoObject");

	private:
		T *m_object;

		KScopedAutoObject(const KScopedAutoObject&) = delete;
		KScopedAutoObject& operator=(const KScopedAutoObject&) = delete;

	public:
		constexpr inline KScopedAutoObject(T *object) : m_object(object) {
			if (m_object != nullptr) {
				reinterpret_cast<KAutoObject*>(m_object)->incrementRefcount();
			}
		}

		inline ~KScopedAutoObject() {
			if (m_object != nullptr) {
				reinterpret_cast<KAutoObject*>(m_object)->decrementRefcount();
			}

			m_object = nullptr;
		}

		constexpr inline KScopedAutoObject<T> &operator=(KScopedAutoObject<T> &&rhs) {
			std::swap(m_object, rhs.m_object);
		}

		constexpr inline T *operator->() { return m_object; }
		constexpr inline T &operator*() { return m_object; }
		constexpr inline T *get_pointer() { return m_object; }
	};

};
