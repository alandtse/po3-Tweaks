#pragma once

namespace Cache
{
	class EditorID : public ISingleton<EditorID>
	{
	public:
		void CacheEditorID(RE::FormID a_formID, const char* a_editorID);
		void CacheEditorID(const RE::TESForm* a_form, const char* a_editorID);

		const std::string& GetEditorID(RE::FormID a_formID);
		const std::string& GetEditorID(const RE::TESForm* a_form);

	private:
		using Lock = std::mutex;
		using Locker = std::scoped_lock<Lock>;

		mutable Lock                 _lock;
		Map<RE::FormID, std::string> _formIDToEditorIDMap;
	};

	const std::string& GetEditorID(RE::FormID a_formID);
	const std::string& GetEditorID(const RE::TESForm* a_form);
}
