set(sources ${sources}
	src/Cache.cpp
	src/Experimental.cpp
	src/Experimental/CleanupOrphanedActiveEffects.cpp
	src/Experimental/ModifySuspendedStackFlushTimeout.cpp
	src/Experimental/ScriptSpeedup.cpp
	src/Experimental/UpdateGameTimers.cpp
	src/Fixes.cpp
	src/Fixes/AttachLightHitEffectCrash.cpp
	src/Fixes/CacheEditorIDs.cpp
	src/Fixes/CombatDialogue.cpp
	src/Fixes/CrosshairRefEventVR.cpp
	src/Fixes/DistantRefLoadCrash.cpp
	src/Fixes/EffectShaderZBuffer.cpp
	src/Fixes/FlagSpellsAsNoAbsorb.cpp
	src/Fixes/IsFurnitureAnimTypeForFurniture.cpp
	src/Fixes/MapMarkerPlacement.cpp
	src/Fixes/OffensiveSpellAI.cpp
	src/Fixes/ProjectileRange.cpp
	src/Fixes/ReapplySpellsOnLoad.cpp
	src/Fixes/RestoreCantTakeBook.cpp
	src/Fixes/RestoreJumpingBonus.cpp
	src/Fixes/SkinnedDecalDelete.cpp
	src/Fixes/ToggleCollision.cpp
	src/Fixes/ToggleGlobalAI.cpp
	src/Fixes/UseFurnitureInCombat.cpp
	src/PCH.cpp
	src/Papyrus.cpp
	src/Settings.cpp
	src/Tweaks.cpp
	src/Tweaks/DynamicSnowMaterial.cpp
	src/Tweaks/FactionStealing.cpp
	src/Tweaks/GameTimeAffectsSounds.cpp
	src/Tweaks/GrabbingIsStealing.cpp
	src/Tweaks/LoadDoorPrompt.cpp
	src/Tweaks/NoCheatMode.cpp
	src/Tweaks/NoCritSneakMessages.cpp
	src/Tweaks/NoPoisonPrompt.cpp
	src/Tweaks/NoRipplesOnHover.cpp
	src/Tweaks/RememberLockPickAngleVR.cpp
	src/Tweaks/ScreenshotToConsole.cpp
	src/Tweaks/SilentSneakPowerAttacks.cpp
	src/Tweaks/SitToWait.cpp
	src/Tweaks/VoiceModulation.cpp
	src/main.cpp
)
