#include "Utils.hpp"
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/SetupTriggerPopup.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/modify/LevelSettingsLayer.hpp>
#include <Geode/modify/SetupCameraOffsetTrigger.hpp>
#include <Geode/modify/SetupCameraModePopup.hpp>

static bool s_dynamicReady = false;

// return standrat sprite
static void resetDynamicIcons() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_objects) return;

    bool doCam = getSwitchValue("do-cam");
    bool colorCam = getSwitchValue("color-cam");

    Ref<CCArray> arr = lel->m_objects;
    for (auto baseObj : CCArrayExt<GameObject*>(arr)) {
        auto obj = typeinfo_cast<EffectGameObject*>(baseObj);
        if (!obj) continue;
        int id = obj->m_objectID;

        if (id == 31) {
            if (getSwitchValue("do-default") && !getSwitchValue("new-start")) {
                TextureUtils::setObjIcon(obj, "start.png"_spr);
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
                continue;
            }
        }

        auto const& iconMap = TextureUtils::getIconMap();
        auto it = iconMap.find(id);
        if (it != iconMap.end()) {
            if (getSwitchValue(it->second.second)) {
                TextureUtils::setObjIcon(obj, it->second.first);
            }
        }
    }
}

// change texture
class $modify(MyEffectGameObject, EffectGameObject) {
    void customSetup() {
        EffectGameObject::customSetup();

        // This mod is editor-focused. Skip all texture swaps outside editor
        // scenes to avoid startup/gameplay crashes on mobile.
        if (!Mod::get() || !LevelEditorLayer::get()) return;

        int id = m_objectID;

        auto const& iconMap = TextureUtils::getIconMap();
        auto it = iconMap.find(id);
        if (it != iconMap.end()) {
            if (getSwitchValue(it->second.second)) {
                TextureUtils::setObjIcon(this, it->second.first);
            }
        }

        if (id == 31 && getSwitchValue("do-default") && !getSwitchValue("new-start")) {
            TextureUtils::setObjIcon(this, "start.png"_spr);
        }

        if (getSwitchValue("do-area")) {
             if (id == 3024) {
                 TextureUtils::setObjIcon(this, getSwitchValue("color-stop") ? "astopc.png"_spr : "astop.png"_spr);
             } else if (id == 3023) {
                 TextureUtils::setObjIcon(this, getSwitchValue("color-stop") ? "estopc.png"_spr : "estop.png"_spr);
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
            if (tex) TextureUtils::setObjIcon(this, tex);
        }

        // Dynamic update inside customSetup is unsafe on some platforms because
        // object data can still be partially initialized here.
        // Dynamic textures are applied from editor-level passes instead.
    }
};

// dynamic texture apply (create, copy...)
class $modify(ShowDynamic, EditorUI) {
    bool init(LevelEditorLayer* editorlayer) {
        TextureUtils::g_isToolboxInit = true;
        
        if (!EditorUI::init(editorlayer)) {
            TextureUtils::g_isToolboxInit = false;
            return false;
        }

        TextureUtils::g_isToolboxInit = false;

        if (!getSwitchValue("dyn-ev") && !getSwitchValue("dyn-sfx") &&
            !getSwitchValue("dyn-item") && !getSwitchValue("dyn-ui") &&
            !getSwitchValue("dyn-start") && !getSwitchValue("dyn-cam") &&
            !getSwitchValue("dyn-game")) {
            return true;
        }

        s_dynamicReady = false;
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.0f),
            CallFuncExt::create([]() {
                s_dynamicReady = true;
                TextureUtils::clearDynamicCache();
                TextureUtils::applyDynamicChangesGlobal();
            }),
            nullptr
        ));
        return true;
    }

    GameObject* createObject(int objectID, cocos2d::CCPoint position) {
        auto obj = EditorUI::createObject(objectID, position);
        if (!obj || !s_dynamicReady || !getSwitchValue("dyn-enable")) return obj;

        if (auto eff = typeinfo_cast<EffectGameObject*>(obj)) {
            auto ds = TextureUtils::getDynamicSettings();
            TextureUtils::applyDynamicUpdatesCached(eff, ds);
        }
        return obj;
    }

    cocos2d::CCArray* pasteObjects(gd::string str, bool withColor, bool noUndo) {
        auto arr = EditorUI::pasteObjects(str, withColor, noUndo);
        if (!arr || !s_dynamicReady || !getSwitchValue("dyn-enable")) return arr;

        auto ds = TextureUtils::getDynamicSettings();
        for (auto obj : CCArrayExt<GameObject*>(arr)) {
            if (auto eff = typeinfo_cast<EffectGameObject*>(obj)) {
                TextureUtils::applyDynamicUpdatesCached(eff, ds);
            }
        }
        return arr;
    }

    void onDeselectAll(CCObject* sender) {
        EditorUI::onDeselectAll(sender);
        TextureUtils::applyDynamicChangesGlobal();
    }
};

$execute {
    auto mod = Mod::get();
    if (!mod) return;

    listenForSettingChanges<bool>("dyn-enable", [](bool value) {
        if (value) {
            TextureUtils::clearDynamicCache();
            TextureUtils::applyDynamicChangesGlobal();
        } else {
            resetDynamicIcons();
            TextureUtils::clearDynamicCache();
        }
    }, mod);
}


// update dynamic texture on close popup
class $modify(MySetupTriggerPopup, SetupTriggerPopup) {
    void onClose(cocos2d::CCObject* sender) {
        auto gameObject = this->m_gameObject;
        auto gameObjects = this->m_gameObjects;

        SetupTriggerPopup::onClose(sender);

        TextureUtils::markDynamicDirty(gameObject);
        TextureUtils::markDynamicDirty(gameObjects);
        TextureUtils::applyDynamicChangesGlobal();
    }
};

class $modify(MySetupCameraOffsetTrigger, SetupCameraOffsetTrigger) {
    void onClose(cocos2d::CCObject* sender) {
        auto gameObject = this->m_gameObject;
        auto gameObjects = this->m_gameObjects;

        SetupCameraOffsetTrigger::onClose(sender);

        TextureUtils::markDynamicDirty(gameObject);
        TextureUtils::markDynamicDirty(gameObjects);
        TextureUtils::applyDynamicChangesGlobal();
    }
};

class $modify(MySetupCameraModePopup, SetupCameraModePopup) {
    void onClose(cocos2d::CCObject* sender) {
        auto gameObject = this->m_gameObject;
        auto gameObjects = this->m_gameObjects;

        SetupCameraModePopup::onClose(sender);

        TextureUtils::markDynamicDirty(gameObject);
        TextureUtils::markDynamicDirty(gameObjects);
        TextureUtils::applyDynamicChangesGlobal();
    }
};

class $modify(MyLevelSettingsLayer, LevelSettingsLayer) {
    void onClose(cocos2d::CCObject* sender) {
        LevelSettingsLayer::onClose(sender);

        if (auto lel = LevelEditorLayer::get()) {
            if (auto objects = lel->m_objects) {
                for (auto baseObj : CCArrayExt<GameObject*>(objects)) {
                    auto obj = typeinfo_cast<EffectGameObject*>(baseObj);
                    if (obj && obj->m_objectID == 31) {
                        TextureUtils::markDynamicDirty(obj);
                    }
                }
            }
        }
        TextureUtils::applyDynamicChangesGlobal();
    }
};
