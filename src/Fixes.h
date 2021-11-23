#pragma once

#include "Settings.h"

namespace Fixes
{
	void Install();
}

//nullptr crash re: QueuedReference
namespace QueuedRefCrash
{
	struct SetFadeNode
	{
		static void func([[maybe_unused]] RE::TESObjectCELL* a_cell, const RE::TESObjectREFR* a_ref)
		{
			const auto root = a_ref->Get3D();
			const auto fadeNode = root ? root->AsFadeNode() : nullptr;

			if (fadeNode) {
				fadeNode->unk144 = 0;
			}
		}
		static inline constexpr std::size_t size = 0x2D;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> func{ REL::ID(19116) };
		stl::asm_replace<SetFadeNode>(func.address());

		logger::info("Installed queued ref crash fix"sv);
	}
}

//fixes not being able to place markers near POI when fast travel is disabled
namespace MapMarker
{
	struct IsFastTravelEnabled
	{
		static bool thunk(RE::PlayerCharacter* a_this, bool a_hideNotification)
		{
			const auto enabled = func(a_this, a_hideNotification);
			if (!enabled) {
				const auto UI = RE::UI::GetSingleton();
				auto mapMenu = UI ? UI->GetMenu<RE::MapMenu>() : nullptr;

				if (mapMenu) {
					mapMenu->PlaceMarker();
				}
			}
			return enabled;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(53095) };
		stl::write_thunk_call<IsFastTravelEnabled>(target.address() + 0x328);

		logger::info("Installed map marker placement fix"sv);
	}
}

//restores DontTake flag functionality
namespace CantTakeBook
{
	namespace Button
	{
		struct ShowTakeButton
		{
			static std::int32_t thunk(RE::GFxMovieView* a_movie, const char* a_text, RE::FxResponseArgs<2>& a_args)
			{
				const auto ref = RE::BookMenu::GetTargetReference();
				const auto book = ref ? RE::BookMenu::GetTargetForm() : nullptr;

				if (book && !book->CanBeTaken()) {
					RE::GFxValue* params = nullptr;  //param[0] = ??, param[1] = book ref exists, param[2] = stealing
					a_args.GetValues(&params);

					params[1].SetBoolean(false);
				}

				return func(a_movie, a_text, a_args);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		inline void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(51057) };
			stl::write_thunk_call<ShowTakeButton>(target.address() + 0x636);
		}
	}

	namespace Prompt
	{
		struct ProcessMessage
		{
			static RE::UI_MESSAGE_RESULTS thunk(RE::BookMenu* a_this, RE::UIMessage& a_message)
			{
				if (a_this->book3D && a_this->unk96 == 1) {
					const auto ref = RE::BookMenu::GetTargetReference();  //is not in inventory
					const auto data = ref ? static_cast<RE::BSUIMessageData*>(a_message.data) : nullptr;

					if (data && data->fixedStr.data() == RE::UserEvents::GetSingleton()->accept.data()) {  //direct BSFixedString compare causes crashes?
						if (const auto book = RE::BookMenu::GetTargetForm(); book && !book->CanBeTaken()) {
							return RE::UI_MESSAGE_RESULTS::kIgnore;
						}
					}
				}

				return func(a_this, a_message);
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline constexpr std::size_t size = 0x04;
		};

		inline void Install()
		{
			stl::write_vfunc<RE::BookMenu, ProcessMessage>();
		}
	}

	inline void Install()
	{
		Button::Install();
		Prompt::Install();

		logger::info("Installed 'Can't Be Taken' book flag fix"sv);
	}
}

//adjusts range of projectile fired while moving
namespace ProjectileRange
{
	struct UpdateCombatThreat
	{
		static void thunk(/*RE::CombatManager::CombatThreats*/ std::uintptr_t a_threats, RE::Projectile* a_projectile)
		{
			using Type = RE::FormType;

			if (a_projectile && (a_projectile->Is(Type::ProjectileMissile) || a_projectile->Is(Type::ProjectileCone))) {
				const auto base = a_projectile->GetBaseObject();
				const auto projectileBase = base ? base->As<RE::BGSProjectile>() : nullptr;
				const auto baseSpeed = projectileBase ? projectileBase->data.speed : 0.0f;
				if (baseSpeed > 0.0f) {
					const auto velocity = a_projectile->linearVelocity;
					a_projectile->range *= velocity.Length() / baseSpeed;
				}
			}

			func(a_threats, a_projectile);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(44222) };
		stl::write_thunk_call<UpdateCombatThreat>(target.address() + 0x79D);

		logger::info("Installed projectile range fix"sv);
	}
}

//fixes combat dialogue
namespace CombatDialogue
{
	struct SayCombatDialogue
	{
		static bool thunk(std::uintptr_t a_combatDialogueManager, RE::Actor* a_speaker, RE::Actor* a_target, RE::DIALOGUE_TYPE a_type, RE::DIALOGUE_DATA::Subtype a_subtype, bool a_ignoreSpeakingDone, RE::CombatController* a_combatController)
		{
			if (a_subtype == RE::DIALOGUE_DATA::Subtype::kLostToNormal && a_target && a_target->IsDead()) {
				const auto combatGroup = a_speaker ? a_speaker->GetCombatGroup() : nullptr;
				if (combatGroup && combatGroup->searchState == 0) {
					a_subtype = RE::DIALOGUE_DATA::Subtype::kCombatToNormal;
				}
			}
			return func(a_combatDialogueManager, a_speaker, a_target, a_type, a_subtype, a_ignoreSpeakingDone, a_combatController);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(44803) };
		stl::write_thunk_call<SayCombatDialogue>(target.address() + 0x135);

		logger::info("Installed combat dialogue fix"sv);
	}
}

//fixes added spells not being reapplied on actor load
//fixes no death dispel spells from not being reapplied on actor load
namespace Spells
{
	namespace detail
	{
		struct PermanentMagicFunctor
		{
			RE::MagicCaster* caster{ nullptr };
			RE::Actor* actor{ nullptr };
			std::uint8_t isSpellType{ 0xFF };
			std::uint8_t isNotSpellType{ 0xAF };
			std::uint8_t flags{ 0 };
			std::uint8_t pad13{ 0 };
			std::uint32_t pad14{ 0 };
		};
		static_assert(sizeof(PermanentMagicFunctor) == 0x18);

		inline bool Apply(PermanentMagicFunctor& a_applier, RE::SpellItem* a_spell)
		{
			using func_t = decltype(&Apply);
			REL::Relocation<func_t> func{ REL::ID(34464) };
			return func(a_applier, a_spell);
		}
	}

	namespace ReapplyAdded
	{
		struct GetAliasInstanceArray
		{
			static RE::ExtraAliasInstanceArray* thunk(RE::ExtraDataList* a_list)
			{
				const auto actor = stl::adjust_pointer<RE::Character>(a_list, -0x70);
				const auto caster = actor && !actor->IsPlayerRef() && !actor->addedSpells.empty() ?
                                        actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) :
                                        nullptr;
				if (caster) {
					detail::PermanentMagicFunctor applier{ caster, actor };
					applier.flags = applier.flags & 0xF9 | 1;
					for (const auto& spell : actor->addedSpells) {
						Apply(applier, spell);
					}
				}
				return func(a_list);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		inline void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(38753) };
			stl::write_thunk_call<GetAliasInstanceArray>(target.address() + 0x115);
		}
	}

	namespace DispelAdded
	{
		struct GetAliasInstanceArray
		{
			static RE::ExtraAliasInstanceArray* thunk(RE::ExtraDataList* a_list)
			{
				const auto actor = stl::adjust_pointer<RE::Character>(a_list, -0x70);
				if (actor && !actor->IsPlayerRef() && !actor->addedSpells.empty()) {
					auto handle = RE::ActorHandle{};
					for (const auto& spell : actor->addedSpells) {
						if (spell && spell->IsValid()) {
							actor->GetMagicTarget()->DispelEffect(spell, handle);
						}
					}
				}
				return func(a_list);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		inline void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(38754) };
			stl::write_thunk_call<GetAliasInstanceArray>(target.address() + 0x131);
		}
	}

	namespace ReapplyOnDeath
	{
		struct Load3D
		{
			static RE::NiAVObject* thunk(RE::Actor* a_actor, bool a_backgroundLoading)
			{
				const auto node = func(a_actor, a_backgroundLoading);
				const auto caster = node && a_actor->IsDead() && !a_actor->IsPlayerRef() ?
                                        a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) :
                                        nullptr;
				if (caster) {
					detail::PermanentMagicFunctor applier{ caster, a_actor };
					applier.flags = applier.flags & 0xF9 | 1;

					const auto has_no_dispel_flag = [&](const RE::SpellItem& a_spell) {
						return std::ranges::any_of(a_spell.effects, [&](const auto& effect) {
							const auto effectSetting = effect ? effect->baseEffect : nullptr;
							return effectSetting && effectSetting->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kNoDeathDispel);
						});
					};

					const auto npc = a_actor->GetActorBase();
					const auto actorEffects = npc ? npc->actorEffects : nullptr;
					if (actorEffects && actorEffects->spells) {
						const std::span span(actorEffects->spells, actorEffects->numSpells);
						for (const auto& spell : span) {
							if (spell && has_no_dispel_flag(*spell)) {
								Apply(applier, spell);
							}
						}
					}

					if (Settings::GetSingleton()->fixes.addedSpell.value) {
						for (const auto& spell : a_actor->addedSpells) {
							if (spell && has_no_dispel_flag(*spell)) {
								Apply(applier, spell);
							}
						}
					}
				}
				return node;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		inline void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(37177) };
			stl::write_thunk_call<Load3D>(target.address() + 0xD);

			logger::info("Installed no death dispel spell reapply fix"sv);
		}
	}
}

//patches IsFurnitureAnimType to work on furniture references
namespace IsFurnitureAnimTypeFix
{
	struct IsFurnitureAnimType
	{
		struct detail
		{
			static std::uint32_t GetEquippedFurnitureType(RE::Actor* a_actor)
			{
				using func_t = decltype(&GetEquippedFurnitureType);
				REL::Relocation<func_t> func{ REL::ID(37732) };
				return func(a_actor);
			}

			static std::uint32_t GetFurnitureType(const RE::TESFurniture* a_furniture)
			{
				using FLAGS = RE::TESFurniture::ActiveMarker;

				const auto flags = a_furniture->furnFlags;
				if (flags.all(FLAGS::kCanSit)) {
					return 1;
				}
				if (flags.all(FLAGS::kCanSleep)) {
					return 2;
				}
				if (flags.all(FLAGS::kCanLean)) {
					return 4;
				}
				return 0;
			}
		};

		static bool func(RE::TESObjectREFR* a_this, std::uint32_t a_type, void*, double& a_result)
		{
			a_result = 0.0;
			if (!a_this) {
				return true;
			}

			if (const auto actor = a_this->As<RE::Actor>(); actor) {
				if (detail::GetEquippedFurnitureType(actor) == a_type) {
					a_result = 1.0;
				}
			} else {
				const auto base = a_this->GetBaseObject();
				const auto furniture = base ? base->As<RE::TESFurniture>() : nullptr;
				if (furniture) {
					if (detail::GetFurnitureType(furniture) == a_type) {
						a_result = 1.0;
					}
				} else {
					return true;
				}
			}

			const auto log = RE::ConsoleLog::GetSingleton();
			if (log && log->IsConsoleMode()) {
				log->Print("IsFurnitureAnimType >> %0.2f", a_result);
			}

			return true;
		}
		static inline constexpr std::size_t size = 0x87;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> func{ REL::ID(21668) };
		stl::asm_replace<IsFurnitureAnimType>(func.address());

		logger::info("Installed IsFurnitureAnimType fix"sv);
	}
}

//nullptr crash re: AttachLightHitEffectVisitor
namespace AttachLightCrash
{
	struct AttachLightHitEffectVisitor
	{
		static std::uint32_t func(RE::AttachLightHitEffectVisitor* a_this, RE::ReferenceEffect* a_hitEffect)
		{
			if (a_hitEffect->IsModelAttached()) {
				auto root = a_hitEffect->GetTargetRoot();
				const auto attachLightObj = root ?
                                                root->GetObjectByName(RE::FixedStrings::GetSingleton()->attachLight) :  //crash here because no null check
                                                nullptr;
				if (attachLightObj) {
					root = attachLightObj;
				}
				if (root && root != a_this->actorRoot) {
					a_this->attachLightNode = root;
				}
				if (a_this->attachLightNode) {
					return 0;
				}
			} else {
				a_this->unk18 = false;
			}
			return 1;
		}
		
		//FixedStrings::GetSingleton() got inlined
		static inline constexpr std::size_t size = 0xEC;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> func{ REL::ID(34388) };
		stl::asm_replace<AttachLightHitEffectVisitor>(func.address());

		logger::info("Installed light attach crash fix"sv);
	}
}

//conjuration spell no absorb
namespace SpellNoAbsorb
{
	inline void Install()
	{
		using Archetype = RE::EffectArchetypes::ArchetypeID;
		using SpellFlag = RE::SpellItem::SpellFlag;

		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler) {
			const auto settings = Settings::GetSingleton();
			const auto noConj = settings->fixes.noConjurationAbsorb.value;
			const auto noHostile = settings->tweaks.noHostileAbsorb.value;

			if (!noConj && !noHostile) {
				return;
			}

			constexpr auto is_conjuration = [](const auto& effect) {
				const auto baseEffect = effect ? effect->baseEffect : nullptr;
				return baseEffect && baseEffect->HasArchetype(Archetype::kSummonCreature);
			};

			constexpr auto is_non_hostile = [](const auto& effect) {
				const auto baseEffect = effect ? effect->baseEffect : nullptr;
				return baseEffect && !baseEffect->IsHostile() && !baseEffect->IsDetrimental();
			};

			for (const auto& spell : dataHandler->GetFormArray<RE::SpellItem>()) {
				if (spell && spell->data.flags.none(SpellFlag::kNoAbsorb)) {
					if (noConj && std::ranges::any_of(spell->effects, is_conjuration) || noHostile && std::ranges::all_of(spell->effects, is_non_hostile)) {
						spell->data.flags.set(SpellFlag::kNoAbsorb);
					}
				}
			}
		}
	}
}

//patches GetEquipped to work with left hand
namespace GetEquippedFix
{
	struct GetEquipped
	{
		struct detail
		{
			static bool get_worn(const RE::TESObjectREFR::InventoryItemMap& a_inventory, RE::TESForm* a_item)
			{
				return std::ranges::any_of(a_inventory, [a_item](const auto& itemData) {
					const auto& [item, data] = itemData;
					const auto& [count, entry] = data;
					
					return item == a_item ? entry->IsWorn() : false;
				});
			}
		};

		static bool func(RE::TESObjectREFR* a_this, RE::TESForm* a_item, void*, double& a_result)
		{
			a_result = 0.0;

			const auto actor = a_this ? a_this->As<RE::Actor>() : nullptr; 
			if (actor && a_item) {
				auto inventory = actor->GetInventory();
				
				const auto list = a_item->As<RE::BGSListForm>(); 
				if (list) {
					auto result = std::ranges::any_of(list->forms, [&](const auto& form) {
						return form && form->IsBoundObject() && detail::get_worn(inventory, form);
					});
					if (!result && list->scriptAddedTempForms) {
						result = std::ranges::any_of(*list->scriptAddedTempForms, [&](const auto& formID) {
							auto form = RE::TESForm::LookupByID(formID);
							return form && form->IsBoundObject() && detail::get_worn(inventory, form);
						});
					}
					if (result) {
						a_result = 1.0;
					}
				} else if (a_item->IsBoundObject() && detail::get_worn(inventory, a_item)) {
					a_result = 1.0;
				}
			}

			const auto log = RE::ConsoleLog::GetSingleton();
			if (log && log->IsConsoleMode()) {
				log->Print("GetEquipped >> %0.2f", a_result);
			}

			return true;
		}
		
		//inlining here too
		static inline constexpr std::size_t size = 0x18D;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> func{ REL::ID(21536) };
		stl::asm_replace<GetEquipped>(func.address());

		logger::info("Installed GetEquipped fix"sv);
	}
}

//fixes z buffer flag for non-detect life shaders
namespace EffectShaderZBufferFix
{
	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(360087) };

		constexpr std::uint8_t zeroes[] = { 0x0, 0x0, 0x0, 0x0 };
		REL::safe_write(target.address() + 0x1C, zeroes, 4);

		logger::info("Installed effect shader z buffer fix"sv);
	}
}

//nullptr crash re: QueuedReference
namespace ToggleCollisionFix
{
	const auto no_collision_flag = static_cast<std::uint32_t>(RE::CFilter::Flag::kNoCollision);

	struct ToggleCollision
	{
		struct detail
		{
			static void ToggleGlobalCollision()
			{
				using func_t = decltype(&ToggleGlobalCollision);
				REL::Relocation<func_t> func{ REL::ID(13375) };
				return func();
			}

			static bool& get_collision_state()
			{
				REL::Relocation<bool*> collision_state{ REL::ID(400334) };
				return *collision_state;
			}
		};

		static bool func(void*, void*, RE::TESObjectREFR* a_ref)
		{
			if (a_ref) {
				bool hasCollision = a_ref->HasCollision();
				bool isActor = a_ref->Is(RE::FormType::ActorCharacter);

				if (!isActor) {
					const auto root = a_ref->Get3D();
					if (root) {
						const auto cell = a_ref->GetParentCell();
						const auto world = cell ? cell->GetbhkWorld() : nullptr;

						if (world) {
							RE::BSWriteLockGuard locker(world->worldLock);

							RE::BSVisit::TraverseScenegraphCollision(root, [&](RE::bhkNiCollisionObject* a_col) -> RE::BSVisit::BSVisitControl {
								auto& body = a_col->body;
								auto hkpBody = body ? static_cast<RE::hkpWorldObject*>(body->referencedObject.get()) : nullptr;
								if (hkpBody) {
									auto& filter = hkpBody->collidable.broadPhaseHandle.collisionFilterInfo;
									if (hasCollision) {
										filter |= no_collision_flag;
									} else {
										filter &= ~no_collision_flag;
									}
								}
								return RE::BSVisit::BSVisitControl::kContinue;
							});
						}
					}
				}

				a_ref->SetCollision(!hasCollision);

				const auto log = RE::ConsoleLog::GetSingleton();
				if (log && log->IsConsoleMode()) {
					const char* result = hasCollision ? "off" : "on";
					log->Print("%s collision %s", a_ref->GetName(), result);
				}
			} else {
				detail::ToggleGlobalCollision();

				const auto log = RE::ConsoleLog::GetSingleton();
				if (log && log->IsConsoleMode()) {
					const char* result = detail::get_collision_state() ? "Off" : "On";
					log->Print("Collision -> %s", result);
				}
			}

			return true;
		}
		static inline constexpr std::size_t size = 0x83;
	};

	struct ApplyMovementDelta
	{
		struct detail
		{
			static bool should_disable_collision(RE::Actor* a_actor, float a_delta)
			{
				auto controller = a_actor->GetCharController();
				if (!controller) {
					return false;
				}

				auto& collisionObj = controller->bumpedCharCollisionObject;
				if (!collisionObj) {
					return false;
				}

				auto& filter = collisionObj->collidable.broadPhaseHandle.collisionFilterInfo;
				if (filter & no_collision_flag) {
					return false;
				}

				auto colRef = RE::TESHavokUtilities::FindCollidableRef(collisionObj->collidable);
				if (colRef && colRef->HasCollision()) {
					return false;
				}

				filter |= no_collision_flag;

				func(a_actor, a_delta);

				filter &= ~no_collision_flag;

				return true;
			}
		};

		static void thunk(RE::Actor* a_actor, float a_delta)
		{
			if (!detail::should_disable_collision(a_actor, a_delta)) {
				return func(a_actor, a_delta);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(37350) };
		stl::write_thunk_call<ApplyMovementDelta>(target.address() + 0xFB);

		REL::Relocation<std::uintptr_t> func{ REL::ID(22825) };
		stl::asm_replace<ToggleCollision>(func.address());

		logger::info("Installed toggle collision fix"sv);
	}
}

namespace LoadFormEditorIDs
{
	struct detail
	{
		static void add_to_game_map(RE::TESForm* a_form, const char* a_str)
		{
			const auto& [map, lock] = RE::TESForm::GetAllFormsByEditorID();
			const RE::BSWriteLockGuard locker{ lock };
			if (map) {
				map->emplace(a_str, a_form);
			}
		}
	};

	struct SetFormEditorID
	{
		static bool thunk(RE::TESForm* a_this, const char* a_str)
		{
			if (!string::is_empty(a_str)) {
				detail::add_to_game_map(a_this, a_str);
			}

			return func(a_this, a_str);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t size = 0x33;
	};

	struct SetFormEditorID_REFR
	{
		static bool thunk(RE::TESForm* a_this, const char* a_str)
		{
			if (!string::is_empty(a_str) && !a_this->IsDynamicForm()) {
				detail::add_to_game_map(a_this, a_str);
			}

			return func(a_this, a_str);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t size = 0x33;
	};

	inline void Install()
	{
		//stl::write_vfunc<RE::TESForm, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSKeyword, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSLocationRefType, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSAction, SetFormEditorID>();
		stl::write_vfunc<RE::BGSTextureSet, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSMenuIcon, SetFormEditorID>();
		//stl::write_vfunc<RE::TESGlobal, SetFormEditorID>();
		stl::write_vfunc<RE::TESClass, SetFormEditorID>();
		stl::write_vfunc<RE::TESFaction, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSHeadPart, SetFormEditorID>();
		stl::write_vfunc<RE::TESEyes, SetFormEditorID>();
		//stl::write_vfunc<RE::TESRace, SetFormEditorID>();
		//stl::write_vfunc<RE::TESSound, SetFormEditorID>();
		stl::write_vfunc<RE::BGSAcousticSpace, SetFormEditorID>();
		stl::write_vfunc<RE::EffectSetting, SetFormEditorID>();
		//stl::write_vfunc<RE::Script, SetFormEditorID>();
		stl::write_vfunc<RE::TESLandTexture, SetFormEditorID>();
		stl::write_vfunc<RE::EnchantmentItem, SetFormEditorID>();
		stl::write_vfunc<RE::SpellItem, SetFormEditorID>();
		stl::write_vfunc<RE::ScrollItem, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectACTI, SetFormEditorID>();
		stl::write_vfunc<RE::BGSTalkingActivator, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectARMO, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectBOOK, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectCONT, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectDOOR, SetFormEditorID>();
		stl::write_vfunc<RE::IngredientItem, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectLIGH, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectMISC, SetFormEditorID>();
		stl::write_vfunc<RE::BGSApparatus, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectSTAT, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSStaticCollection, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSMovableStatic, SetFormEditorID>();
		stl::write_vfunc<RE::TESGrass, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectTREE, SetFormEditorID>();
		stl::write_vfunc<RE::TESFlora, SetFormEditorID>();
		stl::write_vfunc<RE::TESFurniture, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectWEAP, SetFormEditorID>();
		stl::write_vfunc<RE::TESAmmo, SetFormEditorID>();
		stl::write_vfunc<RE::TESNPC, SetFormEditorID>();
		stl::write_vfunc<RE::TESLevCharacter, SetFormEditorID>();
		stl::write_vfunc<RE::TESKey, SetFormEditorID>();
		stl::write_vfunc<RE::AlchemyItem, SetFormEditorID>();
		stl::write_vfunc<RE::BGSIdleMarker, SetFormEditorID>();
		stl::write_vfunc<RE::BGSNote, SetFormEditorID>();
		stl::write_vfunc<RE::BGSConstructibleObject, SetFormEditorID>();
		stl::write_vfunc<RE::BGSProjectile, SetFormEditorID>();
		stl::write_vfunc<RE::BGSHazard, SetFormEditorID>();
		stl::write_vfunc<RE::TESSoulGem, SetFormEditorID>();
		stl::write_vfunc<RE::TESLevItem, SetFormEditorID>();
		stl::write_vfunc<RE::TESWeather, SetFormEditorID>();
		stl::write_vfunc<RE::TESClimate, SetFormEditorID>();
		stl::write_vfunc<RE::BGSShaderParticleGeometryData, SetFormEditorID>();
		stl::write_vfunc<RE::BGSReferenceEffect, SetFormEditorID>();
		stl::write_vfunc<RE::TESRegion, SetFormEditorID>();
		//stl::write_vfunc<RE::NavMeshInfoMap, SetFormEditorID>();
		//stl::write_vfunc<RE::TESObjectCELL, SetFormEditorID>();

		stl::write_vfunc<RE::TESObjectREFR, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::Actor, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::Character, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::PlayerCharacter, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::MissileProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::ArrowProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::GrenadeProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::BeamProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::FlameProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::ConeProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::BarrierProjectile, SetFormEditorID_REFR>();
		stl::write_vfunc<RE::Hazard, SetFormEditorID_REFR>();

		//stl::write_vfunc<RE::TESWorldSpace, SetFormEditorID>();
		//stl::write_vfunc<RE::TESObjectLAND, SetFormEditorID>();
		//stl::write_vfunc<RE::NavMesh, SetFormEditorID>();
		//stl::write_vfunc<RE::TESTopic, SetFormEditorID>();
		stl::write_vfunc<RE::TESTopicInfo, SetFormEditorID>();
		//stl::write_vfunc<RE::TESQuest, SetFormEditorID>();
		//stl::write_vfunc<RE::TESIdleForm, SetFormEditorID>();
		stl::write_vfunc<RE::TESPackage, SetFormEditorID>();
		stl::write_vfunc<RE::DialoguePackage, SetFormEditorID>();
		stl::write_vfunc<RE::TESCombatStyle, SetFormEditorID>();
		stl::write_vfunc<RE::TESLoadScreen, SetFormEditorID>();
		stl::write_vfunc<RE::TESLevSpell, SetFormEditorID>();
		//stl::write_vfunc<RE::TESObjectANIO, SetFormEditorID>();
		stl::write_vfunc<RE::TESWaterForm, SetFormEditorID>();
		stl::write_vfunc<RE::TESEffectShader, SetFormEditorID>();
		stl::write_vfunc<RE::BGSExplosion, SetFormEditorID>();
		stl::write_vfunc<RE::BGSDebris, SetFormEditorID>();
		stl::write_vfunc<RE::TESImageSpace, SetFormEditorID>();
		//stl::write_vfunc<RE::TESImageSpaceModifier, SetFormEditorID>();
		stl::write_vfunc<RE::BGSListForm, SetFormEditorID>();
		stl::write_vfunc<RE::BGSPerk, SetFormEditorID>();
		stl::write_vfunc<RE::BGSBodyPartData, SetFormEditorID>();
		stl::write_vfunc<RE::BGSAddonNode, SetFormEditorID>();
		stl::write_vfunc<RE::ActorValueInfo, SetFormEditorID>();
		stl::write_vfunc<RE::BGSCameraShot, SetFormEditorID>();
		stl::write_vfunc<RE::BGSCameraPath, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSVoiceType, SetFormEditorID>();
		stl::write_vfunc<RE::BGSMaterialType, SetFormEditorID>();
		stl::write_vfunc<RE::BGSImpactData, SetFormEditorID>();
		stl::write_vfunc<RE::BGSImpactDataSet, SetFormEditorID>();
		stl::write_vfunc<RE::TESObjectARMA, SetFormEditorID>();
		stl::write_vfunc<RE::BGSEncounterZone, SetFormEditorID>();
		stl::write_vfunc<RE::BGSLocation, SetFormEditorID>();
		stl::write_vfunc<RE::BGSMessage, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSRagdoll, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSDefaultObjectManager, SetFormEditorID>();
		stl::write_vfunc<RE::BGSLightingTemplate, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSMusicType, SetFormEditorID>();
		stl::write_vfunc<RE::BGSFootstep, SetFormEditorID>();
		stl::write_vfunc<RE::BGSFootstepSet, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSStoryManagerBranchNode, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSStoryManagerQuestNode, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSStoryManagerEventNode, SetFormEditorID>();
		stl::write_vfunc<RE::BGSDialogueBranch, SetFormEditorID>();
		stl::write_vfunc<RE::BGSMusicTrackFormWrapper, SetFormEditorID>();
		stl::write_vfunc<RE::TESWordOfPower, SetFormEditorID>();
		stl::write_vfunc<RE::TESShout, SetFormEditorID>();
		stl::write_vfunc<RE::BGSEquipSlot, SetFormEditorID>();
		stl::write_vfunc<RE::BGSRelationship, SetFormEditorID>();
		stl::write_vfunc<RE::BGSScene, SetFormEditorID>();
		stl::write_vfunc<RE::BGSAssociationType, SetFormEditorID>();
		stl::write_vfunc<RE::BGSOutfit, SetFormEditorID>();
		stl::write_vfunc<RE::BGSArtObject, SetFormEditorID>();
		stl::write_vfunc<RE::BGSMaterialObject, SetFormEditorID>();
		stl::write_vfunc<RE::BGSMovementType, SetFormEditorID>();
		//stl::write_vfunc<RE::BGSSoundDescriptorForm, SetFormEditorID>();
		stl::write_vfunc<RE::BGSDualCastData, SetFormEditorID>();
		stl::write_vfunc<RE::BGSSoundCategory, SetFormEditorID>();
		stl::write_vfunc<RE::BGSSoundOutput, SetFormEditorID>();
		stl::write_vfunc<RE::BGSCollisionLayer, SetFormEditorID>();
		stl::write_vfunc<RE::BGSColorForm, SetFormEditorID>();
		stl::write_vfunc<RE::BGSReverbParameters, SetFormEditorID>();
		stl::write_vfunc<RE::BGSLensFlare, SetFormEditorID>();

		logger::info("Installed editorID cache"sv);
	}
}
