#pragma once
#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

CCSprite* createIconSprite(std::string const& name);
bool getSwitchValue(std::string const& key);

struct TextureUtils {
    static bool g_isToolboxInit;
    
    static const std::unordered_map<int, std::pair<std::string, std::string>> iconMap;

    static void setObjIcon(EffectGameObject* obj, const std::string& texture);
    
    static void updateCompTexture(ItemTriggerGameObject* obj);
    static void updateEditTexture(ItemTriggerGameObject* obj, bool dot);

    static void updateSFXTexture(SFXTriggerGameObject* obj);
    static void updateUiTexture(UISettingsGameObject* obj);
    static void updateEventTexture(EventLinkTrigger* obj, float gap);
    static void updateStartTexture(StartPosObject* obj);

    static void updateStopTexture(EffectGameObject* obj);
    static void updateMoveTexture(EffectGameObject* obj);
    static void updateRotateTexture(EffectGameObject* obj);

    static void updatePickupTexture(CountTriggerGameObject* obj);
    static void updateCountTexture(CountTriggerGameObject* obj);

    static void updateColisTexture(EffectGameObject* obj);
    static void updateSpawnTexture(EffectGameObject* obj);
    static void updateGravityTexture(EffectGameObject* obj);

    static void updateOffsetCamTexture(CameraTriggerGameObject* obj, bool color);
    static void updateRotateCamTexture(CameraTriggerGameObject* obj, bool color);
    static void updateEdgeCamTexture(CameraTriggerGameObject* obj);
    static void updateStaticCamTexture(CameraTriggerGameObject* obj, bool color);

    struct DynamicSettings {
        bool ev, sfx, item, ui, dotEdit, start, color, cam, game;
        float offEv;
    };
    static DynamicSettings getDynamicSettings();
    static void applyDynamicUpdates(EffectGameObject* obj, const DynamicSettings& settings);
    static void applyDynamicUpdatesCached(EffectGameObject* obj, const DynamicSettings& settings);
    
    static void applyDynamicChangesGlobal();
    static void markDynamicDirty(EffectGameObject* obj);
    static void markDynamicDirty(CCArray* objects);
    static void clearDynamicCache();
};
