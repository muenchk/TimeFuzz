#pragma once

#include <assert>
#include <atomic>

namespace smartpointers
{
	namespace detail
	{
		/// <summary>
		/// shared control block for smart and meta pointers
		/// </summary>
		struct CTRL
		{
			uint32_t smartrefs;
			uint32_t metarefs;

			std::atomic_bool _deleted = false;

			bool IncSmartRef()
			{
				if (_deleted == true)
					return false;
				std::atomic_ref<uint32_t> mysmartref(smartrefs);
				++mysmartref;
				return true;
			}

			bool DecSmartRef()
			{
				if (_deleted == true)
					return false;
				std::atomic_ref<uint32_t> mysmartref(smartrefs);
				std::atomic_ref<uint32_t> mymetarefs(metarefs);
				if ((--mysmartref) == 0 && mymetarefs == 0)
					return true;
				return false;
			}

			bool IncMetaRef()
			{
				if (_deleted == true)
					return false;
				std::atomic_ref<uint32_t> mymetaref(metarefs);
				++mymetaref;
				return true;
			}

			bool DecMetaRef()
			{
				if (_deleted == true)
					return false;
				std::atomic_ref<uint32_t> mymetarefs(metarefs);
				std::atomic_ref<uint32_t> mysmartref(smartrefs);
				if ((--mymetarefs) == 0 && mysmartref == 0)
					return true;
				return false;
			}

			bool Deleted()
			{
				return _deleted;
			}
		};
	}

	template <class T>
	class PointerHandle
	{
	};

	/// <summary>
	/// basically a shared_ptr
	/// </summary>
	/// <typeparam name="T"></typeparam>
	template <class T>
	class SmartPointer
	{
		constexpr SmartPointer() noexcept = default;
		constexpr SmartPointer(std::nullptr_t) noexcept {}

		SmartPointer(const NiPointer& a_rhs) :
			_ptr(a_rhs._ptr)
		{
			TryAttach();
		}

		NiPointer(NiPointer&& a_rhs) noexcept :
			_ptr(a_rhs._ptr)
		{
			a_rhs._ptr = nullptr;
		}

		~NiPointer() { TryDetach(); }

		NiPointer& operator=(const NiPointer& a_rhs)
		{
			if (this != std::addressof(a_rhs)) {
				TryDetach();
				_ptr = a_rhs._ptr;
				TryAttach();
			}
			return *this;
		}

		NiPointer& operator=(NiPointer&& a_rhs)
		{
			if (this != std::addressof(a_rhs)) {
				TryDetach();
				_ptr = a_rhs._ptr;
				a_rhs._ptr = nullptr;
			}
			return *this;
		}

		void reset() { TryDetach(); }

		[[nodiscard]] constexpr element_type* get() const noexcept
		{
			return _ptr;
		}

		[[nodiscard]] explicit constexpr operator bool() const noexcept
		{
			return static_cast<bool>(_ptr);
		}

		[[nodiscard]] constexpr element_type& operator*() const noexcept
		{
			assert(static_cast<bool>(*this));
			return *_ptr;
		}

		[[nodiscard]] constexpr element_type* operator->() const noexcept
		{
			assert(static_cast<bool>(*this));
			return _ptr;
		}

	protected:
		void TryAttach()
		{
			if (_ptr) {
				_ptr->IncRefCount();
			}
		}

		void TryDetach()
		{
			if (_ptr) {
				_ptr->DecRefCount();
				_ptr = nullptr;
			}
		}

		/// <summary>
		/// control block pointer
		/// </summary>
		CTRL* _ctrl = nullptr;
	};

	/// <summary>
	/// pointer that does not provide direct access to the underlying object, but instead must be used to get the underlying smart pointer
	/// </summary>
	/// <typeparam name="T"></typeparam>
	template <class T>
	class MetaPointer
	{
	protected:
		/// <summary>
		///
		/// </summary>
		void* _pointer = nullptr;
		/// <summary>
		/// control block pointer
		/// </summary>
		CTRL* _ctrl = nullptr;
	};
}
