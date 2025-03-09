#include "Cache.h"

namespace Cache
{
	void EditorID::CacheEditorID(RE::FormID a_formID, const char* a_editorID)
	{
		Locker locker(_lock);
		_formIDToEditorIDMap.try_emplace(a_formID, a_editorID);
	}

	void EditorID::CacheEditorID(const RE::TESForm* a_form, const char* a_editorID)
	{
		CacheEditorID(a_form->GetFormID(), a_editorID);
	}

	const std::string& EditorID::GetEditorID(RE::FormID a_formID)
	{
		const auto it = _formIDToEditorIDMap.find(a_formID);
		if (it != _formIDToEditorIDMap.end()) {
			return it->second;
		}

		static std::string emptyStr;
		return emptyStr;
	}

	const std::string& EditorID::GetEditorID(const RE::TESForm* a_form)
	{
		return GetEditorID(a_form->GetFormID());
	}

	const std::string& GetEditorID(RE::FormID a_formID)
	{
		return EditorID::GetSingleton()->GetEditorID(a_formID);
	}

	const std::string& GetEditorID(const RE::TESForm* a_form)
	{
		return EditorID::GetSingleton()->GetEditorID(a_form);
	}
}

extern "C" DLLEXPORT const char* GetFormEditorID(std::uint32_t a_formID)
{
	return Cache::EditorID::GetSingleton()->GetEditorID(a_formID).c_str();
}
