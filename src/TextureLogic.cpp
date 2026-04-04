#include "Utils.hpp"
#include <Geode/binding/SFXTriggerGameObject.hpp>
#include <Geode/binding/ItemTriggerGameObject.hpp>
#include <Geode/binding/UISettingsGameObject.hpp>
#include <Geode/binding/EventLinkTrigger.hpp>
#include <Geode/binding/StartPosObject.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/CameraTriggerGameObject.hpp>
#include <Geode/binding/CountTriggerGameObject.hpp>
#include <Geode/binding/CollisionTriggerAction.hpp>
#include <Geode/binding/SpawnTriggerAction.hpp>
#include <Geode/binding/EnterEffectObject.hpp>
#include <Geode/binding/ColorActionSprite.hpp>
#include <Geode/binding/ColorAction.hpp>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Geode/utils/general.hpp>


bool TextureUtils::g_isToolboxInit = false;

CCSprite* createIconSprite(std::string const& name) {
    if (auto mod = Mod::get()) {
        auto modPath = mod->expandSpriteName(name.c_str());
        if (auto spr = CCSprite::create(std::string(modPath).c_str())) {
            return spr;
        }
    } else {
    }
    if (auto spr = CCSprite::createWithSpriteFrameName(name.c_str())) {
        return spr;
    }
    if (auto spr = CCSprite::create(name.c_str())) {
        return spr;
    }
    return CCSprite::create();
}

void TextureUtils::setObjIcon(EffectGameObject* obj, const std::string& texture) {
    if (!obj) {
        return;
    }
    if (auto newSpr = CCSprite::create(texture.c_str())) {
        obj->m_addToNodeContainer = true;
        obj->setTexture(newSpr->getTexture());
        obj->setTextureRect(newSpr->getTextureRect());
    }
    else {
    }
}
static std::optional<int> getIntKey(GameObject* obj, int key) {
    auto layer = LevelEditorLayer::get();
    if (!obj || !layer) {
        return std::nullopt;
    }

    auto gs = obj->getSaveString(layer);          
    std::string s = gs.c_str();            

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    std::string_view view{s};
    size_t pos = 0;
    bool isKey = true;
    int currentKey = 0;

    while (pos < view.size()) {
        auto next = view.find(',', pos);
        auto token = view.substr(pos, next - pos);

        if (auto r = geode::utils::numFromString<int>(token)) {
            if (isKey)
                currentKey = *r;
            else if (currentKey == key) {
                return *r;
            }
        }

        if (next == std::string_view::npos)
            break;

        pos = next + 1;
        isKey = !isKey;
    }

    return std::nullopt;
}

namespace {
using DynamicSettings = TextureUtils::DynamicSettings;
using CacheKey = std::uint64_t;

static std::unordered_map<CacheKey, std::uint64_t> s_dynamicCache;
static std::unordered_set<CacheKey> s_dirtyObjectKeys;
static bool s_hasLastSettings = false;
static DynamicSettings s_lastSettings{};

static CacheKey makePointerKey(const void* ptr) {
    auto raw = static_cast<CacheKey>(reinterpret_cast<std::uintptr_t>(ptr));
    return (static_cast<CacheKey>(1) << 63) | (raw & ((static_cast<CacheKey>(1) << 63) - 1));
}

static CacheKey makeCacheKey(EffectGameObject* obj) {
    if (!obj) return 0;
    if (obj->m_uniqueID != 0) return static_cast<CacheKey>(obj->m_uniqueID);
    return makePointerKey(obj);
}

static std::uint64_t hashCombine(std::uint64_t seed, std::uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
}

static std::uint64_t hashFloat(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool isDynamicColorTriggerID(int objectID) {
    switch (objectID) {
        case 899: // color
        case 29:  // bg
        case 30:  // ground
        case 105: // object
        case 744: // 3dl
        case 900: // ground2
        case 915: // line
            return true;
        default:
            return false;
    }
}

static const char* getColorTriggerBaseTexture(int objectID) {
    switch (objectID) {
        case 29: return "col_bg.png"_spr;
        case 30: return "col_grnd.png"_spr;
        case 105: return "col_obj.png"_spr;
        case 744: return "col_3dl.png"_spr;
        case 900: return "col_grnd2.png"_spr;
        case 915: return "col_line.png"_spr;
        case 899:
        default:
            return "col.png"_spr;
    }
}


static const char* getAreaTriggerBaseTexture(int objectID) {
    switch (objectID) {
        case 3006: return "amove.png"_spr;
        case 3007: return "arotate.png"_spr;
        case 3008: return "ascale.png"_spr;
        case 3009: return "aalpha.png"_spr;
        case 3010: return "apulse.png"_spr;
        default:
            return "amove.png"_spr;
    }
}

/*
    1: 262.0
    2: 262.0, 276
    3: 262.1, 283
    4: 262.1, 276, 283
    5: 262.1,
    6: 262.1, 276
    7: 262.2, 283
    8: 262.2, 276, 283
    9: 262.2
    10: 262.2, 276
*/

static int getAreaBucket(GameObject* obj) {
    if (!obj) return 1;
    int direct = getIntKey(obj, 262).value_or(0);
    bool reverse = getIntKey(obj, 276).value_or(0) != 0;
    bool sided = getIntKey(obj, 283).value_or(0) != 0;
    if (direct == 0) {
        return reverse ? 2 : 1;
    }
    if (direct == 1) {
        if (sided) {
            return reverse ? 4 : 3;
        }
        return reverse ? 6 : 5;
    }
    if (direct == 2) {
        if (sided) {
            return reverse ? 8 : 7;
        }
        return reverse ? 10 : 9;
    }
    return 1;
}
static ccColor3B getColorTriggerTint(EffectGameObject* obj) {
    if (!obj) return ccColor3B{255, 255, 255};

    auto r = getIntKey(obj, 7);
    auto g = getIntKey(obj, 8);
    auto b = getIntKey(obj, 9);
    if (r && g && b) {
        auto clampByte = [](int v) -> unsigned char {
            if (v < 0) return 0;
            if (v > 255) return 255;
            return static_cast<unsigned char>(v);
        };
        return ccColor3B{
            clampByte(*r),
            clampByte(*g),
            clampByte(*b),
        };
    }

    if (obj->m_mainActionSprite) {
        if (obj->m_mainActionSprite->m_colorAction) {
            return obj->m_mainActionSprite->m_colorAction->m_color;
        }
        return obj->m_mainActionSprite->m_color;
    }
    return obj->m_triggerTargetColor;
}

static bool settingsEqual(const DynamicSettings& a, const DynamicSettings& b) {
    return a.logic == b.logic &&
        a.dotEdit == b.dotEdit &&
        a.color == b.color &&
        a.cam == b.cam &&
        a.game == b.game &&
        a.dynColor == b.dynColor &&
        a.offEv == b.offEv;
}
static bool dynLogic(const DynamicSettings& s) { return s.logic; }
static bool dynCam(const DynamicSettings& s) { return s.cam; }
static bool dynGame(const DynamicSettings& s) { return s.game; }
static bool dynColor(const DynamicSettings& s) { return s.dynColor; }

static std::uint64_t sigArea(EffectGameObject* obj, const DynamicSettings&) {
    auto area = typeinfo_cast<EnterEffectObject*>(obj);
    if (!area) return 0;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(getAreaBucket(obj)));
}


static std::uint64_t sigEvent(EffectGameObject* obj, const DynamicSettings& s) {
    auto ev = typeinfo_cast<EventLinkTrigger*>(obj);
    if (!ev) return 0;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, ev->m_eventIDs.size());
    for (int id : ev->m_eventIDs) {
        sig = hashCombine(sig, static_cast<std::uint64_t>(id));
    }
    sig = hashCombine(sig, hashFloat(s.offEv));
    return sig;
}

static std::uint64_t sigSfx(EffectGameObject* obj, const DynamicSettings&) {
    auto sfx = typeinfo_cast<SFXTriggerGameObject*>(obj);
    if (!sfx) return 0;
    float vol = sfx->m_volume;

    float sfx5 = Mod::get()->getSettingValue<float>("sfx5");
    float sfx4 = Mod::get()->getSettingValue<float>("sfx4");
    float sfx3 = Mod::get()->getSettingValue<float>("sfx3");
    float sfx2 = Mod::get()->getSettingValue<float>("sfx2");

    int bucket = 1;
    if (vol > sfx5) bucket = 5;
    else if (vol > sfx4) bucket = 4;
    else if (vol > sfx3) bucket = 3;
    else if (vol > sfx2) bucket = 2;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(bucket));
}

static std::uint64_t sigComp(EffectGameObject* obj, const DynamicSettings&) {
    auto item = typeinfo_cast<ItemTriggerGameObject*>(obj);
    if (!item) return 0;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(item->m_resultType3 + 1));
}

static std::uint64_t sigEdit(EffectGameObject* obj, const DynamicSettings& s) {
    auto item = typeinfo_cast<ItemTriggerGameObject*>(obj);
    if (!item) return 0;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(item->m_resultType1 + 1));
    sig = hashCombine(sig, s.dotEdit ? 1u : 0u);
    return sig;
}

static std::uint64_t sigUi(EffectGameObject* obj, const DynamicSettings&) {
    auto ui = typeinfo_cast<UISettingsGameObject*>(obj);
    if (!ui) return 0;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(ui->m_xRef));
    sig = hashCombine(sig, static_cast<std::uint64_t>(ui->m_yRef));
    sig = hashCombine(sig, ui->m_xRelative ? 1u : 0u);
    sig = hashCombine(sig, ui->m_yRelative ? 1u : 0u);
    return sig;
}

static std::uint64_t sigStart(EffectGameObject* obj, const DynamicSettings&) {
    auto start = typeinfo_cast<StartPosObject*>(obj);
    if (!start || !start->m_startSettings) return 0;
    auto settings = start->m_startSettings;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(settings->m_startMode));
    sig = hashCombine(sig, static_cast<std::uint64_t>(static_cast<int>(settings->m_startSpeed)));
    sig = hashCombine(sig, settings->m_startMini ? 1u : 0u);
    sig = hashCombine(sig, settings->m_reverseGameplay ? 1u : 0u);
    sig = hashCombine(sig, settings->m_isFlipped ? 1u : 0u);
    return sig;
}

static std::uint64_t sigStopTexture(EffectGameObject* obj, const DynamicSettings&) {
    int variant = 0;

    if (auto v = getIntKey(obj, 580); v && *v == 1) variant = 1;
    else if (auto v = getIntKey(obj, 580); v && *v == 2) variant = 2;
    
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigMove(EffectGameObject* obj, const DynamicSettings&) {
    int variant = 0;
    if (obj->m_useMoveTarget) variant = 1;
    else if (obj->m_lockToPlayerX || obj->m_lockToPlayerY || obj->m_lockToCameraX || obj->m_lockToCameraY) variant = 2;
    if (auto v = getIntKey(obj, 394); v && *v == 1) variant = 3;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigRotate(EffectGameObject* obj, const DynamicSettings&) {
    int variant = 0;
    if (obj->m_useMoveTarget) variant = 1;
    if (auto v = getIntKey(obj, 394); v && *v == 1) variant = 2;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigPickup(EffectGameObject* obj, const DynamicSettings&) {
    auto count = typeinfo_cast<CountTriggerGameObject*>(obj);
    if (!count) return 0;
    int variant = 0;
    if (count->m_isOverride) variant = 1;
    else if (count->m_pickupTriggerMode == 1) variant = 2;
    else if (count->m_pickupTriggerMode == 2) variant = 3;
    else if (count->m_pickupCount < 0) variant = 4;
    else if (count->m_pickupCount > 0) variant = 5;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigColis(EffectGameObject* obj, const DynamicSettings&) {
    int variant = 0;
    if (obj->m_triggerOnExit) variant = 1;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigSpawn(EffectGameObject* obj, const DynamicSettings&) {
    int variant = 0;
    auto remap = getIntKey(obj, 442);
    if (remap > 0) {
        variant = 1;
    }

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigGravity(EffectGameObject* obj, const DynamicSettings&) {
    auto count = typeinfo_cast<EffectGameObject*>(obj);
    if (!count) return 0;
    int variant = 0;
    if (count->m_gravityValue > 1.0f) variant = 1;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigPulse(EffectGameObject* obj, const DynamicSettings&) {
    if (!obj) return 0;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(obj->m_triggerTargetColor.r + 1));
    sig = hashCombine(sig, static_cast<std::uint64_t>(obj->m_triggerTargetColor.g + 1));
    sig = hashCombine(sig, static_cast<std::uint64_t>(obj->m_triggerTargetColor.b + 1));
    return sig;
}

static std::uint64_t sigColorTrigger(EffectGameObject* obj, const DynamicSettings&) {
    if (!obj) return 0;
    auto color = getColorTriggerTint(obj);
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(color.r + 1));
    sig = hashCombine(sig, static_cast<std::uint64_t>(color.g + 1));
    sig = hashCombine(sig, static_cast<std::uint64_t>(color.b + 1));
    return sig;
}



static std::uint64_t sigCount(EffectGameObject* obj, const DynamicSettings&) {
    auto count = typeinfo_cast<CountTriggerGameObject*>(obj);
    if (!count) return 0;
    int variant = 0;
    if (count->m_pickupTriggerMode == 1) variant = 1;
    else if (count->m_pickupTriggerMode == 2) variant = 2;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}


//1,1916,2,1245,3,165,36,1,28,30,10,0.5,85,2;
//1,1916,2,1245,3,165,36,1,10,0.5,85,2;f

static std::uint64_t sigOffsetCam(EffectGameObject* obj, const DynamicSettings& s) {
    auto cam = typeinfo_cast<CameraTriggerGameObject*>(obj);
    if (!cam) return 0;
    auto vX = getIntKey(cam, 28);
    auto vY = getIntKey(cam, 29);
    bool xOnly = vX >= 1 && *vX >= 1;
    bool yOnly = vY >= 1 && *vY >= 1;
    int variant = 0;
    if (xOnly && !yOnly) variant = 1;
    else if (yOnly && !xOnly) variant = 2;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
    sig = hashCombine(sig, s.color ? 1u : 0u);
    return sig;
}

static std::uint64_t sigRotateCam(EffectGameObject* obj, const DynamicSettings& s) {
    auto cam = typeinfo_cast<CameraTriggerGameObject*>(obj);
    if (!cam) return 0;
    int variant = cam->m_rotationDegrees < 0.f ? 1 : 0;

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    sig = hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
    sig = hashCombine(sig, s.color ? 1u : 0u);
    return sig;
}

static std::uint64_t sigStaticCam(EffectGameObject* obj, const DynamicSettings& s) {
    auto cam = typeinfo_cast<CameraTriggerGameObject*>(obj);
    if (!cam) return 0;
    int variant = s.color ? 1 : 0;
    if (auto v = getIntKey(cam, 212); v && *v == 1) {
        variant = 2;
    }

    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(variant + 1));
}

static std::uint64_t sigEdgeCam(EffectGameObject* obj, const DynamicSettings&) {
    auto cam = typeinfo_cast<CameraTriggerGameObject*>(obj);
    if (!cam) return 0;
    std::uint64_t sig = static_cast<std::uint64_t>(obj->m_objectID);
    return hashCombine(sig, static_cast<std::uint64_t>(cam->m_edgeDirection + 1));
}

enum class DynamicAction {
    Event,
    Area,
    Sfx,
    Comp,
    Edit,
    Ui,
    Start,
    Stop,
    Move,
    Rotate,
    Pickup,
    Colis,
    Spawn,
    Gravity,
    Color,
    Pulse,
    Count,
    OffsetCam,
    RotateCam,
    StaticCam,
    EdgeCam
};

static const char* dynamicActionName(DynamicAction action) {
    switch (action) {
        case DynamicAction::Event: return "Event";
        case DynamicAction::Area: return "Area";
        case DynamicAction::Sfx: return "Sfx";
        case DynamicAction::Comp: return "Comp";
        case DynamicAction::Edit: return "Edit";
        case DynamicAction::Ui: return "Ui";
        case DynamicAction::Start: return "Start";
        case DynamicAction::Stop: return "Stop";
        case DynamicAction::Move: return "Move";
        case DynamicAction::Rotate: return "Rotate";
        case DynamicAction::Pickup: return "Pickup";
        case DynamicAction::Colis: return "Colis";
        case DynamicAction::Spawn: return "Spawn";
        case DynamicAction::Gravity: return "Gravity";
        case DynamicAction::Color: return "Color";
        case DynamicAction::Pulse: return "Pulse";
        case DynamicAction::Count: return "Count";
        case DynamicAction::OffsetCam: return "OffsetCam";
        case DynamicAction::RotateCam: return "RotateCam";
        case DynamicAction::StaticCam: return "StaticCam";
        case DynamicAction::EdgeCam: return "EdgeCam";
    }
    return "Unknown";
}

static void applyDynamicAction(DynamicAction action, EffectGameObject* obj, const DynamicSettings& s) {
    switch (action) {
        case DynamicAction::Event:
            TextureUtils::updateEventTexture(typeinfo_cast<EventLinkTrigger*>(obj), s.offEv);
            break;
        case DynamicAction::Sfx:
            TextureUtils::updateSFXTexture(typeinfo_cast<SFXTriggerGameObject*>(obj));
            break;
        case DynamicAction::Comp:
            TextureUtils::updateCompTexture(typeinfo_cast<ItemTriggerGameObject*>(obj));
            break;
        case DynamicAction::Area:
            TextureUtils::updateAreaTexture(typeinfo_cast<EnterEffectObject*>(obj));
            break;
        case DynamicAction::Edit:
            TextureUtils::updateEditTexture(typeinfo_cast<ItemTriggerGameObject*>(obj), s.dotEdit);
            break;
        case DynamicAction::Ui:
            TextureUtils::updateUiTexture(typeinfo_cast<UISettingsGameObject*>(obj));
            break;
        case DynamicAction::Start:
            TextureUtils::updateStartTexture(typeinfo_cast<StartPosObject*>(obj));
            break;
        case DynamicAction::Stop:
            TextureUtils::updateStopTexture(obj);
            break;
        case DynamicAction::Move:
            TextureUtils::updateMoveTexture(obj);
            break;
        case DynamicAction::Rotate:
            TextureUtils::updateRotateTexture(obj);
            break;
        case DynamicAction::Pickup:
            TextureUtils::updatePickupTexture(typeinfo_cast<CountTriggerGameObject*>(obj));
            break;
        case DynamicAction::Colis:
            TextureUtils::updateColisTexture(obj);
            break;
        case DynamicAction::Spawn:
            TextureUtils::updateSpawnTexture(obj);
            break;
        case DynamicAction::Gravity:
            TextureUtils::updateGravityTexture(typeinfo_cast<EffectGameObject*>(obj));
            break;
        case DynamicAction::Color:
            TextureUtils::updateColorTexture(obj);
            break;
        case DynamicAction::Pulse:
            TextureUtils::updatePulseTexture(obj);
            break;
        case DynamicAction::Count:
            TextureUtils::updateCountTexture(typeinfo_cast<CountTriggerGameObject*>(obj));
            break;
        case DynamicAction::OffsetCam:
            TextureUtils::updateOffsetCamTexture(typeinfo_cast<CameraTriggerGameObject*>(obj), s.color);
            break;
        case DynamicAction::RotateCam:
            TextureUtils::updateRotateCamTexture(typeinfo_cast<CameraTriggerGameObject*>(obj), s.color);
            break;
        case DynamicAction::StaticCam:
            TextureUtils::updateStaticCamTexture(typeinfo_cast<CameraTriggerGameObject*>(obj), s.color);
            break;
        case DynamicAction::EdgeCam:
            TextureUtils::updateEdgeCamTexture(typeinfo_cast<CameraTriggerGameObject*>(obj));
            break;
    }
}

struct DynamicRule {
    int objectID;
    bool (*enabled)(const DynamicSettings&);
    std::uint64_t (*signature)(EffectGameObject*, const DynamicSettings&);
    DynamicAction action;
};

static const DynamicRule kDynamicRules[] = {
    {3604, dynLogic, sigEvent, DynamicAction::Event},
    {3602, dynGame, sigSfx, DynamicAction::Sfx},
    {3620, dynLogic, sigComp, DynamicAction::Comp},
    {3619, dynLogic, sigEdit, DynamicAction::Edit},
    {3613, dynLogic, sigUi, DynamicAction::Ui},
    {31, dynGame, sigStart, DynamicAction::Start},
    {1616, dynLogic, sigStopTexture, DynamicAction::Stop},
    {901, dynGame, sigMove, DynamicAction::Move},
    {1346, dynGame, sigRotate, DynamicAction::Rotate},
    {1817, dynLogic, sigPickup, DynamicAction::Pickup},
    {1815, dynLogic, sigColis, DynamicAction::Colis},
    {1268, dynLogic, sigSpawn, DynamicAction::Spawn},
    {2066, dynGame, sigGravity, DynamicAction::Gravity},
    {3006, dynGame, sigArea, DynamicAction::Area},
    {3007, dynGame, sigArea, DynamicAction::Area},
    {3008, dynGame, sigArea, DynamicAction::Area},
    {3009, dynGame, sigArea, DynamicAction::Area},
    {3010, dynGame, sigArea, DynamicAction::Area},
    {899, dynColor, sigColorTrigger, DynamicAction::Color},
    {29, dynColor, sigColorTrigger, DynamicAction::Color},
    {30, dynColor, sigColorTrigger, DynamicAction::Color},
    {105, dynColor, sigColorTrigger, DynamicAction::Color},
    {744, dynColor, sigColorTrigger, DynamicAction::Color},
    {900, dynColor, sigColorTrigger, DynamicAction::Color},
    {915, dynColor, sigColorTrigger, DynamicAction::Color},
    {1006, dynColor, sigPulse, DynamicAction::Pulse},
    {1811, dynLogic, sigCount, DynamicAction::Count},
    {1916, dynCam, sigOffsetCam, DynamicAction::OffsetCam},
    {2015, dynCam, sigRotateCam, DynamicAction::RotateCam},
    {1914, dynCam, sigStaticCam, DynamicAction::StaticCam},
    {2062, dynCam, sigEdgeCam, DynamicAction::EdgeCam},
};

static const DynamicRule* findDynamicRule(int objectID) {
    for (const auto& rule : kDynamicRules) {
        if (rule.objectID == objectID) return &rule;
    }
    return nullptr;
}

static std::uint64_t getDynamicSignature(EffectGameObject* obj, const DynamicSettings& s) {
    if (!obj) return 0;
    auto rule = findDynamicRule(obj->m_objectID);
    if (!rule || !rule->enabled(s)) return 0;
    return rule->signature(obj, s);
}
} 

void TextureUtils::updateStopTexture(EffectGameObject* obj) {
    if (!obj) return;
    const char* tex = "stop.png"_spr;

    if (auto v = getIntKey(obj, 580); v && *v == 1) tex = "pause.png"_spr;

    else if (auto v = getIntKey(obj, 580); v && *v == 2) tex = "resume.png"_spr;
    
    setObjIcon(obj, tex);
}

void TextureUtils::updateRotateTexture(EffectGameObject* obj) {
    if (!obj) return;
    const char* tex = "rotate.png"_spr;

    if(obj->m_useMoveTarget) tex = "rotate_aim.png"_spr;

    if (auto v = getIntKey(obj, 394); v && *v == 1) {
        tex = "rotate_follow.png"_spr;
    }
    
    setObjIcon(obj, tex);
}

void TextureUtils::updateMoveTexture(EffectGameObject* obj) {
    if (!obj) return;
    const char* tex = "move.png"_spr;

    if(obj->m_useMoveTarget) tex = "move_target.png"_spr;

    else if(obj->m_lockToPlayerX || obj->m_lockToPlayerY || obj->m_lockToCameraX || obj->m_lockToCameraY) tex = "move_lock.png"_spr;

    if (auto v = getIntKey(obj, 394); v && *v == 1) {
        tex = "move_direction.png"_spr;
    }
    
    setObjIcon(obj, tex);
}

void TextureUtils::updatePickupTexture(CountTriggerGameObject* obj) {
    if (!obj) return;
    const char* tex = "pickup.png"_spr;

    if(obj->m_isOverride) tex = "p_override.png"_spr;

    else if(obj->m_pickupTriggerMode == 1) tex = "p_krat.png"_spr;

    else if(obj->m_pickupTriggerMode == 2) tex = "p_delete.png"_spr;
    
    else if(obj->m_pickupCount < 0) tex = "p_minus.png"_spr;

    else if(obj->m_pickupCount > 0) tex = "p_plus.png"_spr;

    setObjIcon(obj, tex);
}

void TextureUtils::updateColisTexture(EffectGameObject* obj) {
    if (!obj) return;
    const char* tex = "colis.png"_spr;

    if(obj->m_triggerOnExit) tex = "colis_exit.png"_spr;

    setObjIcon(obj, tex);
}

void TextureUtils::updateSpawnTexture(EffectGameObject* obj) {
    if (!obj) return;
    const char* tex = "spawn.png"_spr;

    auto remap = getIntKey(obj, 442);
    
    if (remap > 0) {
        tex = "spawn_remap.png"_spr;
    }

    setObjIcon(obj, tex);
}

void TextureUtils::updateGravityTexture(EffectGameObject* obj) {
    if (!obj) return;
    const char* tex = "gravity_low.png"_spr;

    if(obj->m_gravityValue > 1.0f) tex = "gravity_high.png"_spr;

    setObjIcon(obj, tex);
}

void TextureUtils::updateAreaTexture(EnterEffectObject* obj) {
    if (!obj) return;

    const char* baseTex = getAreaTriggerBaseTexture(obj->m_objectID);
    auto sprBase = CCSprite::create(baseTex);
    auto sprEmp = CCSprite::create("area1.png"_spr);

    switch (getAreaBucket(obj)) {
        case 2: sprEmp = CCSprite::create("area2.png"_spr); break;
        case 3: sprEmp = CCSprite::create("area3.png"_spr); break;
        case 4: sprEmp = CCSprite::create("area4.png"_spr); break;
        case 5: sprEmp = CCSprite::create("area5.png"_spr); break;
        case 6: sprEmp = CCSprite::create("area6.png"_spr); break;
        case 7: sprEmp = CCSprite::create("area7.png"_spr); break;
        case 8: sprEmp = CCSprite::create("area8.png"_spr); break;
        case 9: sprEmp = CCSprite::create("area10.png"_spr); break;
        case 10: sprEmp = CCSprite::create("area9.png"_spr); break;
        default: sprEmp = CCSprite::create("area1.png"_spr); break;
    }

    if (!sprBase || !sprEmp) {
        setObjIcon(obj, baseTex);
        return;
    }

    auto baseSize = sprBase->getContentSize();
    auto empSize = sprEmp->getContentSize();
    float w = baseSize.width > empSize.width ? baseSize.width : empSize.width;
    float h = baseSize.height > empSize.height ? baseSize.height : empSize.height;
    constexpr float kPad = 4.f;
    w += kPad * 2.f;
    h += kPad * 2.f;

    if (w <= 0.f || h <= 0.f) {
        setObjIcon(obj, baseTex);
        return;
    }

    auto center = CCPoint{w / 2.f, h / 2.f};

    sprBase->setPosition(center);
    sprEmp->setPosition(center);

    sprBase->setFlipY(true);
    sprEmp->setFlipY(true);

    auto rt = CCRenderTexture::create(w, h);
    if (!rt) return;

    rt->beginWithClear(0, 0, 0, 0);
    sprBase->visit();
    sprEmp->visit();
    rt->end();

    if (auto tex = rt->getSprite()->getTexture()) {
        ccTexParams tp = {GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        tex->setTexParameters(&tp);
        obj->m_addToNodeContainer = true;
        obj->setTexture(tex);
        obj->setTextureRect({0, 0, w, h});
    } else {
        setObjIcon(obj, baseTex);
    }
}
void TextureUtils::updateColorTexture(EffectGameObject* obj) {
    if (!obj) return;
    auto color = getColorTriggerTint(obj);
    const char* baseTex = getColorTriggerBaseTexture(obj->m_objectID);

    auto sprBase = CCSprite::create(baseTex);
    if (!sprBase) {
        setObjIcon(obj, baseTex);
        return;
    }

    auto sprEmp = CCSprite::create("col_emp.png"_spr);
    if (!sprEmp) {
        setObjIcon(obj, baseTex);
        return;
    }
    sprEmp->setColor(color);

    auto baseSize = sprBase->getContentSize();
    auto empSize = sprEmp->getContentSize();
    float w = baseSize.width > empSize.width ? baseSize.width : empSize.width;
    float h = baseSize.height > empSize.height ? baseSize.height : empSize.height;
    if (w <= 0.f || h <= 0.f) {
        setObjIcon(obj, baseTex);
        return;
    }

    constexpr float kPad = 2.f;
    float rtW = w + kPad * 2.f;
    float rtH = h + kPad * 2.f;
    float cx = rtW * 0.5f;
    float cy = rtH * 0.5f;

    sprBase->setPosition({cx, cy});
    sprEmp->setPosition({cx, cy});
    sprBase->setFlipY(true);
    sprEmp->setFlipY(true);

    auto rt = CCRenderTexture::create(rtW, rtH);
    if (!rt) {
        setObjIcon(obj, baseTex);
        return;
    }

    rt->beginWithClear(0, 0, 0, 0);
    sprBase->visit();
    sprEmp->visit();
    rt->end();

    if (auto tex = rt->getSprite()->getTexture()) {
        ccTexParams tp = {GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        tex->setTexParameters(&tp);
        obj->m_addToNodeContainer = true;
        obj->setTexture(tex);
        obj->setTextureRect({kPad, kPad, w, h});
    } else {
        setObjIcon(obj, baseTex);
    }
}

void TextureUtils::updatePulseTexture(EffectGameObject* obj) {
    if (!obj) return;

    auto sprPulse = CCSprite::create("pulse.png"_spr);
    auto sprRing = CCSprite::create("pulse_ring.png"_spr);
    if (!sprPulse || !sprRing) {
        setObjIcon(obj, "pulse.png"_spr);
        return;
    }

    auto pulseSize = sprPulse->getContentSize();
    auto ringSize = sprRing->getContentSize();
    float w = pulseSize.width > ringSize.width ? pulseSize.width : ringSize.width;
    float h = pulseSize.height > ringSize.height ? pulseSize.height : ringSize.height;
    if (w <= 0.f || h <= 0.f) {
        setObjIcon(obj, "pulse.png"_spr);
        return;
    }

    constexpr float kPad = 2.f;
    float rtW = w + kPad * 2.f;
    float rtH = h + kPad * 2.f;
    float cx = rtW * 0.5f;
    float cy = rtH * 0.5f;

    sprPulse->setPosition({cx, cy});
    sprRing->setPosition({cx, cy});
    sprPulse->setFlipY(true);
    sprRing->setFlipY(true);
    sprRing->setColor(obj->m_triggerTargetColor);

    auto rt = CCRenderTexture::create(rtW, rtH);
    if (!rt) {
        setObjIcon(obj, "pulse.png"_spr);
        return;
    }

    rt->beginWithClear(0, 0, 0, 0);
    sprPulse->visit();
    sprRing->visit();
    rt->end();

    if (auto tex = rt->getSprite()->getTexture()) {
        ccTexParams tp = {GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        tex->setTexParameters(&tp);
        obj->m_addToNodeContainer = true;
        obj->setTexture(tex);
        obj->setTextureRect({kPad, kPad, w, h});
    } else {
        setObjIcon(obj, "pulse.png"_spr);
    }
}

void TextureUtils::updateCountTexture(CountTriggerGameObject* obj) {
    if (!obj) return;
    const char* tex = "advcount.png"_spr;

    if(obj->m_pickupTriggerMode == 1) tex = "i_more.png"_spr;

    else if(obj->m_pickupTriggerMode == 2) tex = "i_less.png"_spr;
    
    else tex = "i_set.png"_spr;

    setObjIcon(obj, tex);
}

void TextureUtils::updateCompTexture(ItemTriggerGameObject* obj) {
    if (!obj) return;
    const char* tex = "comp.png"_spr;
    switch (obj->m_resultType3) {
        case 5: tex = "comp5.png"_spr; break;
        case 4: tex = "comp4.png"_spr; break;
        case 3: tex = "comp3.png"_spr; break;
        case 2: tex = "comp2.png"_spr; break;
        case 1: tex = "comp1.png"_spr; break;
        case 0: tex = "comp0.png"_spr; break;
    }
    setObjIcon(obj, tex);
}

void TextureUtils::updateEditTexture(ItemTriggerGameObject* obj, bool dot) {
    if (!obj) return;
    const char* tex = "edit.png"_spr;
    switch (obj->m_resultType1) {
        case 4: tex = "edit4.png"_spr; break;
        case 3: tex = dot ? "edit3d.png"_spr : "edit3a.png"_spr; break;
        case 2: tex = "edit2.png"_spr; break;
        case 1: tex = "edit1.png"_spr; break;
        case 0: tex = "edit0.png"_spr; break;
    }
    setObjIcon(obj, tex);
}

void TextureUtils::updateOffsetCamTexture(CameraTriggerGameObject* obj, bool color) {
    if (!obj) return;

    auto vX = getIntKey(obj, 28);
    auto vY = getIntKey(obj, 29);
    bool xOnly = vX >= 1 && *vX >= 1;
    bool yOnly = vY >= 1 && *vY >= 1;

    const char* tex = color ? "offset.png"_spr : "Coffset.png"_spr;

    if (xOnly && !yOnly)
        tex = color ? "offsetX.png"_spr : "coffsetX.png"_spr;
    else if (yOnly && !xOnly)
        tex = color ? "offsetY.png"_spr : "coffsetY.png"_spr;

    setObjIcon(obj, tex);
}

void TextureUtils::updateRotateCamTexture(CameraTriggerGameObject* obj, bool color) {
    if (!obj) return;

    const char* tex;
    if (color) {
        if (obj->m_rotationDegrees >= 0.f)
            tex = "rotatecam.png"_spr;
        if (obj->m_rotationDegrees < 0.f)
            tex = "rotatecamR.png"_spr;
    }
    else {
        if (obj->m_rotationDegrees >= 0.f)
            tex = "crotate.png"_spr;
        if (obj->m_rotationDegrees < 0.f)
            tex = "crotateR.png"_spr;
    }
    setObjIcon(obj, tex);
}

void TextureUtils::updateStaticCamTexture(CameraTriggerGameObject* obj, bool color) {
    if (!obj) return;

    const char* tex = color ? "static.png"_spr : "cstatic.png"_spr;

    if (auto v = getIntKey(obj, 212); v && *v == 1) {
        tex = "static_follow.png"_spr;
    }

    setObjIcon(obj, tex);
}    

void TextureUtils::updateEdgeCamTexture(CameraTriggerGameObject* obj) {
    if (!obj) return;
    const char* tex = "edge.png"_spr;
    switch (obj->m_edgeDirection) {
        case 4: tex = "edgeD.png"_spr; break;
        case 3: tex = "edgeT.png"_spr; break;
        case 2: tex = "edge.png"_spr; break;
        case 1: tex = "edgeL.png"_spr; break;
    }
    setObjIcon(obj, tex);
}    

void TextureUtils::updateSFXTexture(SFXTriggerGameObject* obj) {
    if (!obj) return;
    float vol = obj->m_volume;
    
    static auto getSfxVal = [](const char* key) { return Mod::get()->getSettingValue<float>(key); };
    
    const char* tex = "sfx1.png"_spr;
    if (vol > getSfxVal("sfx5")) tex = "sfx5.png"_spr;
    else if (vol > getSfxVal("sfx4")) tex = "sfx4.png"_spr;
    else if (vol > getSfxVal("sfx3")) tex = "sfx3.png"_spr;
    else if (vol > getSfxVal("sfx2")) tex = "sfx2.png"_spr;
    
    setObjIcon(obj, tex);
}

void TextureUtils::updateStartTexture(StartPosObject* obj) {
    if (!obj) {
        return;
    }
    
    auto settings = obj->m_startSettings;
    if (!settings) {
        return;
    }

    auto sprMain = CCSprite::create("start_title.png"_spr);
    if (!sprMain) {
        return;
    }

    float gap = 12.f; 

    const char* modeTex = "start_cube.png"_spr;
    switch (settings->m_startMode) {
        case 0: modeTex = "start_cube.png"_spr; break;
        case 1: modeTex = "start_ship.png"_spr; break;
        case 2: modeTex = "start_ball.png"_spr; break;
        case 3: modeTex = "start_ufo.png"_spr; break;
        case 4: modeTex = "start_wave.png"_spr; break;
        case 5: modeTex = "start_robot.png"_spr; break;
        case 6: modeTex = "start_spider.png"_spr; break;
        case 7: modeTex = "start_swing.png"_spr; break;
    }
    auto sprMode = CCSprite::create(modeTex);
    if (sprMode && settings->m_startMini) {
        sprMode->setScale(0.75f);
    }

    const char* speedTex = "start_s1.png"_spr;
    int speedVal = static_cast<int>(settings->m_startSpeed);
    switch (speedVal) {
        case 1: speedTex = "start_s0.png"_spr; break; 
        case 0: speedTex = "start_s1.png"_spr; break; 
        case 2: speedTex = "start_s2.png"_spr; break; 
        case 3: speedTex = "start_s3.png"_spr; break; 
        case 4: speedTex = "start_s4.png"_spr; break; 
    }
    auto sprSpeed = CCSprite::create(speedTex);

    auto sprRot = CCSprite::create("start_path.png"_spr); 
    if (sprRot) {
        if (settings->m_reverseGameplay) sprRot->setFlipX(true);
        sprRot->setFlipY(!settings->m_isFlipped);
    }

    float w = 50.f, h = 30.f;
    float cx = w / 2;
    float cy = h / 2; 

    float mainY = cy;
    
    float subY = cy + 3; 

    float subGap = (gap > 0.f) ? gap : 15.f;

    sprMain->setPosition({cx, mainY}); 
    sprMain->setFlipY(true); 

    if (sprMode) {
        sprMode->setPosition({cx - subGap, subY});
        sprMode->setFlipY(true);
    }

    if (sprSpeed) {
        sprSpeed->setPosition({cx, subY});
        sprSpeed->setFlipY(true);
    }

    if (sprRot) {
        sprRot->setPosition({cx + subGap, subY});
    }

    auto rt = CCRenderTexture::create(w, h);
    if (!rt) {
        return;
    }
    rt->beginWithClear(0, 0, 0, 0);
    
    sprMain->visit();
    if (sprMode) sprMode->visit();
    if (sprSpeed) sprSpeed->visit();
    if (sprRot) sprRot->visit();
    
    rt->end();

    if (auto tex = rt->getSprite()->getTexture()) {
        ccTexParams tp = {GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        tex->setTexParameters(&tp);
        
        obj->m_addToNodeContainer = true;
        obj->setTexture(tex);
        obj->setTextureRect({0, 0, w, h});
    }
    else {
    }
}

void TextureUtils::updateUiTexture(UISettingsGameObject* obj) {
    if (!obj) {
        return;
    }
    
    auto sprUiel = CCSprite::create("uiel.png"_spr);
    auto sprTitle = CCSprite::create("uititle.png"_spr);
    if (!sprUiel || !sprTitle) {
        return;
    }

    float w = 25.f, h = 35.f;
    float cx = w/2, cy = h/2;
    float uielX = cx, uielY = cy;

    if (obj->m_xRef == 3) uielX -= 3.f;
    else if (obj->m_xRef == 4) uielX += 3.f;
    
    if (obj->m_yRef == 7) uielY += 3.f;
    else if (obj->m_yRef == 8) uielY -= 3.f;

    sprTitle->setPosition({cx, cy});
    sprUiel->setPosition({uielX, uielY});
    sprTitle->setFlipY(true);
    sprUiel->setFlipY(true);

    auto rt = CCRenderTexture::create(w, h);
    if (!rt) {
        return;
    }
    rt->beginWithClear(0,0,0,0);
    sprTitle->visit();
    sprUiel->visit();

    if (obj->m_xRelative) {
        if (auto s = CCSprite::create("uix.png"_spr)) {
            s->setPosition({cx, cy}); s->setFlipY(true); s->visit();
        }
    }
    if (obj->m_yRelative) {
        if (auto s = CCSprite::create("uiy.png"_spr)) {
            s->setPosition({cx, cy}); s->setFlipY(true); s->visit();
        }
    }
    rt->end();

    if (auto tex = rt->getSprite()->getTexture()) {
        ccTexParams tp = {GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        tex->setTexParameters(&tp);
        obj->m_addToNodeContainer = true;
        obj->setTexture(tex);
        obj->setTextureRect({0, 0, w, h});
    }
    else {
    }
}

void TextureUtils::updateEventTexture(EventLinkTrigger* obj, float gap) {
    if (!obj) {
        return;
    }
    const auto& eids = obj->m_eventIDs;
    std::vector<const char*> singleTex, combinedTex;

    auto add = [&](const char* s, const char* c) {
        bool found = false;
        for(auto ptr : singleTex) if(ptr == s) { found = true; break; }
        if (!found) {
            singleTex.push_back(s);
            combinedTex.push_back(c);
        }
    };

    if (eids.empty()) {
        add("ev.png"_spr, "evs.png"_spr);
    } else {
        for (int id : eids) {
            if (id >= 1 && id <= 5) add("evland.png"_spr, "evlands.png"_spr);
            else if (id == 6) add("evhit.png"_spr, "evhits.png"_spr);
            else if ((id >= 7 && id <= 8) || (id >= 34 && id <= 44)) add("evorb.png"_spr, "evorbs.png"_spr);
            else if (id == 9 || (id >= 45 && id <= 49)) add("evpad.png"_spr, "evpads.png"_spr);
            else if ((id >= 10 && id <= 11) || (id >= 50 && id <= 52)) add("evgravity.png"_spr, "evgravitys.png"_spr);
            else if (id >= 12 && id <= 22) add("evjump.png"_spr, "evjumps.png"_spr);
            else if (id == 62 || id == 63) add("evcoin.png"_spr, "evcoins.png"_spr);
            else if (id >= 65 && id <= 68) add("evfall.png"_spr, "evfalls.png"_spr);
            else if (id == 69) add("evtop.png"_spr, "evtops.png"_spr);
            else if (id == 70) add("evup.png"_spr, "evups.png"_spr);
            else if (id == 71) add("evleft.png"_spr, "evlefts.png"_spr);
            else if (id == 72) add("evlef.png"_spr, "evlefs.png"_spr);
            else if (id == 73) add("evright.png"_spr, "evrights.png"_spr);
            else if (id == 74) add("evrig.png"_spr, "evrigs.png"_spr);
            else if (id == 75) add("evreverse.png"_spr, "evreverses.png"_spr);
            else if (id == 60 || id == 64) add("evsave.png"_spr, "evsaves.png"_spr);
            else if ((id >= 26 && id <= 33) || (id >= 50 && id <= 59)) add("evportal.png"_spr, "evportals.png"_spr);
            else add("ev.png"_spr, "evs.png"_spr);
        }
    }

    if (singleTex.size() == 1) {
        setObjIcon(obj, singleTex[0]);
        return;
    }

    auto spr1 = CCSprite::create(combinedTex[0]);
    auto spr2 = CCSprite::create(combinedTex[1]);
    auto sprText = CCSprite::create("evtitles.png"_spr);
    if (!spr1 || !spr2 || !sprText) {
        return;
    }

    float w = 100.f, h = 50.f;
    float cx = w/2, cy = h/2;
    spr1->setFlipY(true); spr2->setFlipY(true); sprText->setFlipY(true);
    spr1->setPosition({cx - gap, cy});
    spr2->setPosition({cx + gap, cy});
    sprText->setPosition({cx, cy});

    auto rt = CCRenderTexture::create(w, h);
    if (!rt) {
        return;
    }
    rt->beginWithClear(0,0,0,0);
    spr1->visit(); spr2->visit(); sprText->visit();
    rt->end();

    if (auto tex = rt->getSprite()->getTexture()) {

        ccTexParams tp = {GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        tex->setTexParameters(&tp);
        obj->m_addToNodeContainer = true;
        obj->setTexture(tex);
        obj->setTextureRect({0, 0, w, h});
    }
    else {
    }
}

TextureUtils::DynamicSettings TextureUtils::getDynamicSettings() {

    DynamicSettings settings;
    settings.logic = getSwitchValue("dyn-logic");
    settings.dotEdit = getSwitchValue("dot-edit");
    settings.cam = getSwitchValue("dyn-cam");
    settings.color = getSwitchValue("color-cam");
    settings.game = getSwitchValue("dyn-game");
    settings.dynColor = getSwitchValue("dyn-color");
    settings.offEv = Mod::get()->getSettingValue<float>("off-ev");
    return settings;
}

void TextureUtils::applyDynamicUpdates(EffectGameObject* obj, const DynamicSettings& s) {
    if (!obj) {
        return;
    }

    auto rule = findDynamicRule(obj->m_objectID);

    if (!rule) {
        return;
    }
    if (!rule->enabled(s)) {
        return;
    }
    applyDynamicAction(rule->action, obj, s);
}

void TextureUtils::applyDynamicUpdatesCached(EffectGameObject* obj, const DynamicSettings& s) {
    if (!obj) {
        return;
    }

    if (isDynamicColorTriggerID(obj->m_objectID)) {
        applyDynamicUpdates(obj, s);
        return;
    }

    auto key = makeCacheKey(obj);
    if (!key) {
        return;
    }

    auto sig = getDynamicSignature(obj, s);

    if (!sig) {
        s_dynamicCache.erase(key);
        return;
    }

    auto it = s_dynamicCache.find(key);
    if (it != s_dynamicCache.end() && it->second == sig) {
        return;
    }

    s_dynamicCache[key] = sig;
    applyDynamicUpdates(obj, s);
}

void TextureUtils::applyDynamicChangesGlobal() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_objects) {
        return;
    }
    
    if (!getSwitchValue("dyn-enable")) {
        return;
    }

    auto settings = getDynamicSettings();
    bool settingsChanged = !s_hasLastSettings || !settingsEqual(settings, s_lastSettings);
    s_hasLastSettings = true;
    s_lastSettings = settings;

    if (!settingsChanged && s_dirtyObjectKeys.empty()) {
        return;
    }

    if (settingsChanged) {
        Ref<CCArray> arr = lel->m_objects;
        for (auto obj : CCArrayExt<EffectGameObject*>(arr)) {
            if (obj) {
                applyDynamicUpdatesCached(obj, settings);
            }
        }
    } 
    
    else {
        Ref<CCArray> arr = lel->m_objects;
        for (auto obj : CCArrayExt<EffectGameObject*>(arr)) {
            if (!obj) {
                continue;
            }

            auto stableKey = makeCacheKey(obj);
            auto pointerKey = makePointerKey(obj);
            if (s_dirtyObjectKeys.find(stableKey) == s_dirtyObjectKeys.end() &&
                s_dirtyObjectKeys.find(pointerKey) == s_dirtyObjectKeys.end()) {
                continue;
            }

            applyDynamicUpdatesCached(obj, settings);
        }
    }

    s_dirtyObjectKeys.clear();
}

void TextureUtils::markDynamicDirty(EffectGameObject* obj) {
    if (!obj) {
        return;
    }
    s_dirtyObjectKeys.insert(makePointerKey(obj));
}

void TextureUtils::markDynamicDirty(CCArray* objects) {
    if (!objects) {
        return;
    }
    for (auto obj : CCArrayExt<EffectGameObject*>(objects)) {
        if (obj) markDynamicDirty(obj);
    }
}

void TextureUtils::clearDynamicCache() {
    s_dynamicCache.clear();
    s_dirtyObjectKeys.clear();
    s_hasLastSettings = false;
}

const std::unordered_map<int, std::pair<std::string, std::string>> TextureUtils::iconMap = {
    // SHADER
    {2904, {"shader.png"_spr, "do-shader"}}, {2905, {"shock.png"_spr, "do-shader"}},
    {2907, {"line.png"_spr, "do-shader"}}, {2909, {"glitch.png"_spr, "do-shader"}},
    {2910, {"chromatic.png"_spr, "do-shader"}}, {2911, {"chrglitch.png"_spr, "do-shader"}},
    {2912, {"pixelate.png"_spr, "do-shader"}}, {2913, {"circles.png"_spr, "do-shader"}},
    {2914, {"radial.png"_spr, "do-shader"}}, {2915, {"motion.png"_spr, "do-shader"}},
    {2916, {"bulge.png"_spr, "do-shader"}}, {2917, {"pinch.png"_spr, "do-shader"}},
    {2919, {"blind.png"_spr, "do-shader"}}, {2920, {"sepia.png"_spr, "do-shader"}},
    {2921, {"negative.png"_spr, "do-shader"}}, {2922, {"hue.png"_spr, "do-shader"}},
    {2923, {"color.png"_spr, "do-shader"}}, {2924, {"screen.png"_spr, "do-shader"}},
    
    // DEFAULT
    {899, {"col.png"_spr, "do-default"}},
    {29, {"col_bg.png"_spr, "do-default"}}, {30, {"col_grnd.png"_spr, "do-default"}},
    {105, {"col_obj.png"_spr, "do-default"}}, {744, {"col_3dl.png"_spr, "do-default"}},
    {900, {"col_grnd2.png"_spr, "do-default"}}, {915, {"col_line.png"_spr, "do-default"}},
    {901, {"move.png"_spr, "do-default"}}, {1006, {"pulse.png"_spr, "do-default"}},
    {1007, {"alpha.png"_spr, "do-default"}}, {1346, {"rotate.png"_spr, "do-default"}},  
    {2067, {"scale.png"_spr, "do-default"}}, {1585, {"animate.png"_spr, "do-default"}},
    {3016, {"advfollow.png"_spr, "do-default"}}, {3660, {"editadv.png"_spr, "do-default"}},
    {3661, {"target.png"_spr, "do-default"}}, {1814, {"followy.png"_spr, "do-default"}},
    {1935, {"timewarp.png"_spr, "do-default"}}, {1932, {"control.png"_spr, "do-default"}},
    {2999, {"mg.png"_spr, "do-default"}}, {3606, {"bgs.png"_spr, "do-default"}},
    {3612, {"mgs.png"_spr, "do-default"}}, 
    {2899, {"options.png"_spr, "do-default"}}, {3602, {"sfx.png"_spr, "do-default"}},
    {3603, {"esfx.png"_spr, "do-default"}}, {3600, {"end.png"_spr, "do-default"}},
    {2901, {"gpoff.png"_spr, "do-default"}}, {1917, {"reverse.png"_spr, "do-default"}},
    {1934, {"song.png"_spr, "do-default"}}, {3605, {"editsong.png"_spr, "do-default"}},
    {3029, {"bgc.png"_spr, "do-default"}}, {3030, {"gc.png"_spr, "do-default"}},
    {3031, {"mgc.png"_spr, "do-default"}}, {3604, {"ev.png"_spr, "do-default"}},
    {2066, {"gravity_low.png"_spr, "do-default"}}, {32, {"ghost.png"_spr, "do-default"}},
    {33, {"ghost_dis.png"_spr, "do-default"}}, {1612, {"hide_p.png"_spr, "do-default"}},
    {1613, {"show_p.png"_spr, "do-default"}}, {1818, {"bg.png"_spr, "do-default"}}, 
    {1819, {"offbg.png"_spr, "do-default"}},
    // LOGIC
    {1616, {"stop.png"_spr, "do-logic"}}, {1817, {"pickup.png"_spr, "do-logic"}},
    {1268, {"spawn.png"_spr, "do-logic"}}, {1347, {"follow.png"_spr, "do-logic"}},
    {1912, {"random.png"_spr, "do-logic"}}, {2068, {"advrandom.png"_spr, "do-logic"}},
    {1611, {"count.png"_spr, "do-logic"}}, {1811, {"advcount.png"_spr, "do-logic"}},
    {1595, {"touch.png"_spr, "do-logic"}}, {3619, {"edit.png"_spr, "do-logic"}},
    {3620, {"comp.png"_spr, "do-logic"}}, {3641, {"pers.png"_spr, "do-logic"}},
    {1812, {"dead.png"_spr, "do-logic"}}, {1815, {"colis.png"_spr, "do-logic"}},
    {3609, {"advcolis.png"_spr, "do-logic"}}, {3613, {"ui.png"_spr, "do-logic"}},

    // AREA
    {3006, {"amove.png"_spr, "do-area"}}, {3007, {"arotate.png"_spr, "do-area"}},
    {3008, {"ascale.png"_spr, "do-area"}}, {3009, {"aalpha.png"_spr, "do-area"}},
    {3010, {"apulse.png"_spr, "do-area"}}, {3011, {"eamove.png"_spr, "do-area"}},
    {3012, {"earotate.png"_spr, "do-area"}}, {3013, {"eascale.png"_spr, "do-area"}},
    {3014, {"eaalpha.png"_spr, "do-area"}}, {3015, {"eapulse.png"_spr, "do-area"}},
    {3017, {"emove.png"_spr, "do-area"}}, {3018, {"erotate.png"_spr, "do-area"}},
    {3019, {"escale.png"_spr, "do-area"}}, {3020, {"ealpha.png"_spr, "do-area"}},
    {3021, {"epulse.png"_spr, "do-area"}},

    // COLIS
    {3640, {"colisin.png"_spr, "do-colis"}}, {1816, {"colisblock.png"_spr, "do-colis"}},
    {3643, {"colistouch.png"_spr, "do-colis"}}
};



