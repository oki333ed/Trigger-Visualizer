#include "Utils.hpp"
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/SetupTriggerPopup.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/modify/LevelSettingsLayer.hpp>
#include <Geode/modify/SetupCameraOffsetTrigger.hpp>
#include <Geode/modify/SetupCameraModePopup.hpp>

#define TV_TRACE(...) log::info("[TV] " __VA_ARGS__)

static bool s_dynamicReady = false;

// return standrat sprite
static void resetDynamicIcons() {
    TV_TRACE("resetDynamicIcons begin");
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_objects) {
        TV_TRACE("resetDynamicIcons skip reason=no-editor-or-objects");
        return;
    }

    bool doCam = getSwitchValue("do-cam");
    bool colorCam = getSwitchValue("color-cam");
    TV_TRACE("resetDynamicIcons settings doCam={} colorCam={}", doCam, colorCam);

    Ref<CCArray> arr = lel->m_objects;
    size_t processed = 0;
    for (auto obj : CCArrayExt<EffectGameObject*>(arr)) {
        if (!obj) continue;
        ++processed;
        int id = obj->m_objectID;

        if (id == 31) {
            if (getSwitchValue("do-default") && !getSwitchValue("new-start")) {
                TextureUtils::setObjIcon(obj, "start.png"_spr);
                TV_TRACE("resetDynamicIcons set start icon objID={}", id);
            }
            continue;
        }

        if (doCam) {
            const char* camTex = nullptr;
            switch (id) {
                case 1914: camTex = colorCam ? "static.png"_spr : "cstatic.png"_spr; break;
                case 1916: camTex = colorCam ? "offset.png"_spr : "Coffset.png"_spr; break;
                case 2015: camTex = colorCam ? "rotatecam.png"_spr : "crotate.png"_spr; break;
                case 2062: camTex = "edge.png"_spr; break;
            }
            if (camTex) {
                TextureUtils::setObjIcon(obj, camTex);
                TV_TRACE("resetDynamicIcons set cam icon objID={} tex='{}'", id, camTex);
                continue;
            }
        }

        auto it = TextureUtils::iconMap.find(id);
        if (it != TextureUtils::iconMap.end()) {
            if (getSwitchValue(it->second.second)) {
                TextureUtils::setObjIcon(obj, it->second.first);
                TV_TRACE("resetDynamicIcons set default icon objID={} tex='{}' setting='{}'", id, it->second.first, it->second.second);
            }
        }
    }
    TV_TRACE("resetDynamicIcons done processed={}", processed);
}

// change texture
class $modify(MyEffectGameObject, EffectGameObject) {
    void customSetup() {
        TV_TRACE("customSetup begin objID={} toolboxInit={} dynamicReady={}", m_objectID, TextureUtils::g_isToolboxInit, s_dynamicReady);
        EffectGameObject::customSetup();

        int id = m_objectID;

        auto it = TextureUtils::iconMap.find(id);
        if (it != TextureUtils::iconMap.end()) {
            if (getSwitchValue(it->second.second)) {
                TextureUtils::setObjIcon(this, it->second.first);
                TV_TRACE("customSetup static icon applied objID={} tex='{}' setting='{}'", id, it->second.first, it->second.second);
            }
        }

        if (id == 31 && getSwitchValue("do-default") && !getSwitchValue("new-start")) {
            TextureUtils::setObjIcon(this, "start.png"_spr);
            TV_TRACE("customSetup start icon applied objID={}", id);
        }

        if (getSwitchValue("do-area")) {
             if (id == 3024) {
                 TextureUtils::setObjIcon(this, getSwitchValue("color-stop") ? "astopc.png"_spr : "astop.png"_spr);
                 TV_TRACE("customSetup area stop start objID={}", id);
             } else if (id == 3023) {
                 TextureUtils::setObjIcon(this, getSwitchValue("color-stop") ? "estopc.png"_spr : "estop.png"_spr);
                 TV_TRACE("customSetup area stop end objID={}", id);
             }
        }

        if (getSwitchValue("do-cam")) {
            bool colorCam = getSwitchValue("color-cam");
            const char* tex = nullptr;
            switch (id) {
                case 1913: tex = colorCam ? "zoom.png"_spr : "czoom.png"_spr; break;
                case 1914: tex = colorCam ? "static.png"_spr : "cstatic.png"_spr; break;
                case 1916: tex = colorCam ? "offset.png"_spr : "Coffset.png"_spr; break;
                case 2015: tex = colorCam ? "rotatecam.png"_spr : "crotate.png"_spr; break;
                case 2062: tex = "edge.png"_spr; break;
                case 2925: tex = "mode.png"_spr; break;
                case 2016: tex = "guide.png"_spr; break;
                case 1520: if (!getSwitchValue("new-shake")) tex = "shake.png"_spr; break;
            }
            if (tex) {
                TextureUtils::setObjIcon(this, tex);
                TV_TRACE("customSetup cam icon applied objID={} tex='{}'", id, tex);
            }
        }

       if (!TextureUtils::g_isToolboxInit && s_dynamicReady && getSwitchValue("dyn-enable")) {
            auto ds = TextureUtils::getDynamicSettings();
            TextureUtils::applyDynamicUpdatesCached(this, ds);
            TV_TRACE("customSetup dynamic applied objID={}", id);
        }
        TV_TRACE("customSetup end objID={}", id);
    }
};

// dynamic texture apply (create, copy...)
class $modify(ShowDynamic, EditorUI) {
    bool init(LevelEditorLayer* editorlayer) {
        TV_TRACE("EditorUI::init begin editorLayer={}", static_cast<void*>(editorlayer));
        TextureUtils::g_isToolboxInit = true;
        
        if (!EditorUI::init(editorlayer)) {
            TextureUtils::g_isToolboxInit = false;
            TV_TRACE("EditorUI::init failed");
            return false;
        }

        TextureUtils::g_isToolboxInit = false;
        TV_TRACE("EditorUI::init success");

        if (!getSwitchValue("dyn-ev") && !getSwitchValue("dyn-sfx") &&
            !getSwitchValue("dyn-item") && !getSwitchValue("dyn-ui") &&
            !getSwitchValue("dyn-start") && !getSwitchValue("dyn-cam") &&
            !getSwitchValue("dyn-game")) {
            TV_TRACE("EditorUI::init dynamic disabled by category switches");
            return true;
        }

        s_dynamicReady = false;
        TV_TRACE("EditorUI::init scheduling initial dynamic pass");
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.0f),
            CallFuncExt::create([]() {
                TV_TRACE("EditorUI::init deferred dynamic begin");
                s_dynamicReady = true;
                TextureUtils::clearDynamicCache();
                TextureUtils::applyDynamicChangesGlobal();
                TV_TRACE("EditorUI::init deferred dynamic end");
            }),
            nullptr
        ));
        return true;
    }

    GameObject* createObject(int objectID, cocos2d::CCPoint position) {
        TV_TRACE("EditorUI::createObject begin objectID={} x={} y={}", objectID, position.x, position.y);
        auto obj = EditorUI::createObject(objectID, position);
        if (!obj) {
            TV_TRACE("EditorUI::createObject skip reason=create-failed objectID={}", objectID);
            return obj;
        }
        if (!s_dynamicReady || !getSwitchValue("dyn-enable")) {
            TV_TRACE("EditorUI::createObject skip dynamic objectID={} ready={} dynEnable={}", objectID, s_dynamicReady, getSwitchValue("dyn-enable"));
            return obj;
        }

        if (auto eff = typeinfo_cast<EffectGameObject*>(obj)) {
            auto ds = TextureUtils::getDynamicSettings();
            TextureUtils::applyDynamicUpdatesCached(eff, ds);
            TV_TRACE("EditorUI::createObject dynamic applied objectID={}", objectID);
        }
        TV_TRACE("EditorUI::createObject end objectID={}", objectID);
        return obj;
    }

    cocos2d::CCArray* pasteObjects(gd::string str, bool withColor, bool noUndo) {
        TV_TRACE("EditorUI::pasteObjects begin withColor={} noUndo={} inputLen={}", withColor, noUndo, str.size());
        auto arr = EditorUI::pasteObjects(str, withColor, noUndo);
        if (!arr) {
            TV_TRACE("EditorUI::pasteObjects skip reason=paste-failed");
            return arr;
        }
        if (!s_dynamicReady || !getSwitchValue("dyn-enable")) {
            TV_TRACE("EditorUI::pasteObjects skip dynamic ready={} dynEnable={} count={}", s_dynamicReady, getSwitchValue("dyn-enable"), arr->count());
            return arr;
        }

        auto ds = TextureUtils::getDynamicSettings();
        size_t processed = 0;
        for (auto obj : CCArrayExt<GameObject*>(arr)) {
            if (auto eff = typeinfo_cast<EffectGameObject*>(obj)) {
                TextureUtils::applyDynamicUpdatesCached(eff, ds);
                ++processed;
            }
        }
        TV_TRACE("EditorUI::pasteObjects end count={} dynamicProcessed={}", arr->count(), processed);
        return arr;
    }

    void onDeselectAll(CCObject* sender) {
        TV_TRACE("EditorUI::onDeselectAll begin sender={}", static_cast<void*>(sender));
        EditorUI::onDeselectAll(sender);
        TextureUtils::applyDynamicChangesGlobal();
        TV_TRACE("EditorUI::onDeselectAll end");
    }
};

$execute {
    listenForSettingChanges<bool>("dyn-enable", [](bool value) {
        TV_TRACE("setting change dyn-enable={}", value);
        if (value) {
            TextureUtils::clearDynamicCache();
            TextureUtils::applyDynamicChangesGlobal();
            TV_TRACE("setting dyn-enable handled enable-path");
        } else {
            resetDynamicIcons();
            TextureUtils::clearDynamicCache();
            TV_TRACE("setting dyn-enable handled disable-path");
        }
    }, Mod::get());
}


// update dynamic texture on close popup
class $modify(SetupTriggerPopup) {
    void onClose(cocos2d::CCObject* sender) {
        TV_TRACE("SetupTriggerPopup::onClose begin sender={}", static_cast<void*>(sender));
        SetupTriggerPopup::onClose(sender); 
        
        TextureUtils::markDynamicDirty(this->m_gameObject);
        TextureUtils::markDynamicDirty(this->m_gameObjects);
        TextureUtils::applyDynamicChangesGlobal();
        TV_TRACE("SetupTriggerPopup::onClose end");
    }
};

class $modify(MySetupCameraOffsetTrigger, SetupCameraOffsetTrigger) {
    void onClose(cocos2d::CCObject* sender) {
        TV_TRACE("SetupCameraOffsetTrigger::onClose begin sender={}", static_cast<void*>(sender));
        SetupCameraOffsetTrigger::onClose(sender);

        TextureUtils::markDynamicDirty(this->m_gameObject);
        TextureUtils::markDynamicDirty(this->m_gameObjects);
        TextureUtils::applyDynamicChangesGlobal();
        TV_TRACE("SetupCameraOffsetTrigger::onClose end");
    }
};

class $modify(MySetupCameraModePopup, SetupCameraModePopup) {
    void onClose(cocos2d::CCObject* sender) {
        TV_TRACE("SetupCameraModePopup::onClose begin sender={}", static_cast<void*>(sender));
        SetupCameraModePopup::onClose(sender);

        TextureUtils::markDynamicDirty(this->m_gameObject);
        TextureUtils::markDynamicDirty(this->m_gameObjects);
        TextureUtils::applyDynamicChangesGlobal();
        TV_TRACE("SetupCameraModePopup::onClose end");
    }
};

class $modify(MyLevelSettingsLayer, LevelSettingsLayer) {
    void onClose(cocos2d::CCObject* sender) {
        TV_TRACE("LevelSettingsLayer::onClose begin sender={}", static_cast<void*>(sender));
        LevelSettingsLayer::onClose(sender);

        size_t marked = 0;
        if (auto lel = LevelEditorLayer::get()) {
            if (auto objects = lel->m_objects) {
                for (auto obj : CCArrayExt<EffectGameObject*>(objects)) {
                    if (obj && obj->m_objectID == 31) {
                        TextureUtils::markDynamicDirty(obj);
                        ++marked;
                    }
                }
            }
        }
        TextureUtils::applyDynamicChangesGlobal();
        TV_TRACE("LevelSettingsLayer::onClose end markedStartObjects={}", marked);
    }
};
