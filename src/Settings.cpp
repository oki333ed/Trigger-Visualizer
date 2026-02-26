#include "Utils.hpp"
#include <Geode/loader/SettingV3.hpp>
#include <Geode/loader/Mod.hpp>

using namespace geode::prelude;

#define TV_TRACE(...) log::info("[TV] " __VA_ARGS__)

// custom settings (only custom settings (maybe (if I’m not lying (but I’m not lying (maybe)))))
class SpriteSwitchSettingV3 : public SettingBaseValueV3<bool> {
public:
    std::string m_spr1;
    std::string m_spr2;

    static Result<std::shared_ptr<SettingV3>> parse(std::string const& key, std::string const& modID, matjson::Value const& json) {
        TV_TRACE("SpriteSwitchSettingV3::parse key='{}' modID='{}'", key, modID);
        auto res = std::make_shared<SpriteSwitchSettingV3>();
        auto root = checkJson(json, "SpriteSwitchSettingV3");
        res->parseBaseProperties(key, modID, root);
        root.has("spr1").into(res->m_spr1);
        root.has("spr2").into(res->m_spr2);
        return root.ok(std::static_pointer_cast<SettingV3>(res));
    }
    SettingNodeV3* createNode(float width) override;
};

class SpriteSwitchNodeV3 : public SettingValueNodeV3<SpriteSwitchSettingV3> {
protected:
    bool init(std::shared_ptr<SpriteSwitchSettingV3> setting, float width) {
        if (!SettingValueNodeV3::init(setting, width)) return false;
        TV_TRACE("SpriteSwitchNodeV3::init key='{}' width={}", setting->getKey(), width);

        auto menu = this->getButtonMenu();
        menu->removeAllChildren();

        auto btn1Spr = createIconSprite(setting->m_spr1);
        btn1Spr->setScale(0.6f);
        auto btn1 = CCMenuItemSpriteExtra::create(btn1Spr, this, menu_selector(SpriteSwitchNodeV3::onSelectFalse));
        btn1->setTag(0);

        auto btn2Spr = createIconSprite(setting->m_spr2);
        btn2Spr->setScale(0.6f);
        auto btn2 = CCMenuItemSpriteExtra::create(btn2Spr, this, menu_selector(SpriteSwitchNodeV3::onSelectTrue));
        btn2->setTag(1);

        menu->addChild(btn1);
        menu->addChild(btn2);

        auto layout = RowLayout::create();
        layout->setGap(10.f);
        layout->setAxisAlignment(AxisAlignment::End);
        menu->setLayout(layout);
        menu->setContentSize({ 90.f, 40.f });
        menu->updateLayout();

        this->updateState(nullptr);
        TV_TRACE("SpriteSwitchNodeV3::init done key='{}'", setting->getKey());
        return true;
    }

    void onSelectFalse(CCObject*) {
        TV_TRACE("SpriteSwitchNodeV3::onSelectFalse key='{}'", this->getSetting()->getKey());
        this->setValue(false, nullptr);
    }
    void onSelectTrue(CCObject*) {
        TV_TRACE("SpriteSwitchNodeV3::onSelectTrue key='{}'", this->getSetting()->getKey());
        this->setValue(true, nullptr);
    }

    void updateState(CCNode* invoker) override {
        SettingValueNodeV3::updateState(invoker);
        bool val = this->getValue();
        TV_TRACE("SpriteSwitchNodeV3::updateState key='{}' value={}", this->getSetting()->getKey(), val);
        auto menu = this->getButtonMenu();
        if (auto btn1 = static_cast<CCMenuItemSpriteExtra*>(menu->getChildByTag(0))) {
            btn1->setColor(val ? ccGRAY : ccWHITE);
            btn1->setOpacity(val ? 120 : 255);
        }
        if (auto btn2 = static_cast<CCMenuItemSpriteExtra*>(menu->getChildByTag(1))) {
            btn2->setColor(val ? ccWHITE : ccGRAY);
            btn2->setOpacity(val ? 255 : 120);
        }
    }

public:
    static SpriteSwitchNodeV3* create(std::shared_ptr<SpriteSwitchSettingV3> setting, float width) {
        auto ret = new SpriteSwitchNodeV3();
        if (ret->init(setting, width)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

SettingNodeV3* SpriteSwitchSettingV3::createNode(float width) {
    return SpriteSwitchNodeV3::create(std::static_pointer_cast<SpriteSwitchSettingV3>(shared_from_this()), width);
}

$execute {
    if (auto mod = Mod::get()) {
        TV_TRACE("register custom setting type='sprite-switch'");
        (void)mod->registerCustomSettingType("sprite-switch", &SpriteSwitchSettingV3::parse);
    }
}

bool getSwitchValue(std::string const& key) {
    auto mod = Mod::get();
    if (!mod) return false;

    auto anySetting = mod->getSetting(key);
    if (!anySetting) return false;

    if (auto setting = std::dynamic_pointer_cast<SpriteSwitchSettingV3>(anySetting)) {
        return setting->getValue();
    }
    return mod->getSettingValue<bool>(key);
}
