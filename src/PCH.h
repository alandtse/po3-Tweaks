#pragma once

#define NOMMNOSOUND

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#pragma warning(disable: 4100)

#pragma warning(push)
#include <robin_hood.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <SimpleIni.h>
#include <xbyak/xbyak.h>
#pragma warning(pop)

namespace logger = SKSE::log;
namespace string = SKSE::stl::string;

using namespace std::literals;

namespace stl
{
	using namespace SKSE::stl;

	void asm_replace(std::uintptr_t a_from, std::size_t a_size, std::uintptr_t a_to);

	template <class T>
	void asm_replace(std::uintptr_t a_from)
	{
		asm_replace(a_from, T::size, reinterpret_cast<std::uintptr_t>(T::func));
	}

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		T::func = vtbl.write_vfunc(T::size, T::thunk);
	}
}

#define DLLEXPORT __declspec(dllexport)

#include "Version.h"
