#include "logger.h"
#include "ClibUtil/editorID.hpp";

std::vector<int> GetPluginVersion(StaticFunctionTag*) { return {1, 0, 2}; }

BGSKeyword* RechargeableKeyword;
const char* keywordPrefix = "RF_Rechargeable_";
std::vector<BGSKeyword*> chargeKeywords(6);
std::vector<int> chargeValues(6);

std::string buffer;

#ifdef GetObject
    #undef GetObject
#endif

int GetItemChargeValue(StaticFunctionTag*, TESForm* a_form);

InventoryEntryData* GetEntryDataAtIndex(int index) {
    if (auto menu = UI::GetSingleton()->GetMenu<InventoryMenu>().get()) {
        auto items = menu->GetRuntimeData().itemList->items;
        if (index < items.size() && index >= 0) {
            return items[index]->data.objDesc;
        }
    }
    return nullptr;
}

template <typename T>
T* GetExtraDataByType(InventoryEntryData* entryData) {
    auto extraLists = entryData->extraLists;
    if (extraLists) {
        for (const auto& xList : *extraLists) {
            if (!xList) continue;
            if (auto extraData = xList->GetByType<T>(); extraData) {
                return extraData;
            }
        }
    }
    return nullptr;
}

float GetEntryMaxCharge(InventoryEntryData* entryData) {
    auto* object = entryData->GetObject();
    if (!object->Is(FormType::Weapon)) return -1.0f;
    auto weapon = object->As<TESObjectWEAP>();
    if (!weapon) return -1.0f;
    if (weapon->formEnchanting) {
        return weapon->amountofEnchantment;
    } else if (ExtraEnchantment* extraEnchant = GetExtraDataByType<ExtraEnchantment>(entryData); extraEnchant) {
        return extraEnchant->charge;
    }

    return -1.0f;
}

std::string GetRechargeableKeyword(InventoryEntryData* entryData) {
    if (TESBoundObject* boundObject = entryData->GetObject(); boundObject) {
        if (auto kwform = boundObject->As<BGSKeywordForm>(); kwform) {
            for (const auto* keyword : kwform->GetKeywords()) {
                std::string id = keyword->GetFormEditorID();
                if (id.starts_with(keywordPrefix)) {
                    return id;
                }
            }
        }
    }

    auto enchantment = entryData->GetEnchantment();
    if (!enchantment) return "";
    for (const auto* effect : enchantment->effects) {
        auto* base = effect->baseEffect;
        if (!base) continue;
        for (const auto* keyword : base->GetKeywords()) {
            std::string id = keyword->GetFormEditorID();
            if (id.starts_with(keywordPrefix)) {
                return id;
            }
        }
    }

    return "";
}

class RequestListHandler : public GFxFunctionHandler {
public:
    virtual void Call(Params& params) override {
        if (params.argCount == 1 && params.args[0].IsNumber()) {
            auto index = params.args[0].GetUInt();
            auto entryData = GetEntryDataAtIndex(index);
            if (!entryData) return;
            auto keywordID = GetRechargeableKeyword(entryData);
            if (keywordID.empty()) return;
            auto* player = PlayerCharacter::GetSingleton();
            buffer.clear();

            buffer = std::to_string(GetEntryMaxCharge(entryData)) + "||";
            BGSListForm* list = TESForm::LookupByEditorID<BGSListForm>(keywordID + "List");
            if (list) {
                list->ForEachForm([player](TESForm* form) {
                    if (form && form->IsBoundObject()) {
                        auto* object = form->As<TESBoundObject>();
                        auto count = player->GetItemCount(object);
                        if (count < 1) return BSContainer::ForEachResult::kContinue;
                        buffer.append(clib_util::editorID::get_editorID(form) + "//" + form->GetName() + "//" +
                                      std::to_string(count) + "//" + std::to_string(GetItemChargeValue(nullptr, form)) +
                                      "||");
                    }
                    return BSContainer::ForEachResult::kContinue;
                });
                params.retVal->SetString(buffer);
            }
        }
    }
};

class ChargeHandler : public GFxFunctionHandler {
public:
    virtual void Call(Params& params) override {
        if (params.argCount == 2 && params.args[0].IsString() && params.args[1].IsNumber()) {
            std::string formID = params.args[0].GetString();
            auto index = params.args[1].GetUInt();
            auto* form = TESForm::LookupByEditorID(formID);
            if (!form || !form->IsBoundObject()) return;
            auto entryData = GetEntryDataAtIndex(index);
            if (!entryData) return;
            auto* object = form->As<TESBoundObject>();
            auto* player = PlayerCharacter::GetSingleton();
            if (!player->GetItemCount(object)) return;
            
            auto chargeAmount = GetItemChargeValue(nullptr, form);
            if (chargeAmount == 0) chargeAmount = GetEntryMaxCharge(entryData); // 0: full recharge
            ExtraCharge* xCharge = GetExtraDataByType<ExtraCharge>(entryData);
            if (xCharge) {
                xCharge->charge += chargeAmount;
                player->RemoveItem(object, 1, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                PlaySound("UIEnchantRecharge");
            }
        }
    }
};

class ItemIsUsingRF : public GFxFunctionHandler {
public:
    virtual void Call(Params& params) override {
        if (params.argCount == 1 && params.args[0].IsNumber()) {
            auto index = params.args[0].GetUInt();
            auto entryData = GetEntryDataAtIndex(index);
            if (!entryData) return;
            if (TESBoundObject* boundObject = entryData->GetObject(); boundObject) {
                if (boundObject->As<BGSKeywordForm>()->HasKeyword(RechargeableKeyword)) {
                    params.retVal->SetBoolean(true);
                    return;
                }
            }
            auto enchantment = entryData->GetEnchantment();
            if (!enchantment) return;
            if (enchantment->HasKeyword(RechargeableKeyword)) {
                params.retVal->SetBoolean(true);
            }
        }
    }
};

static void Inject(BSFixedString menuName) {
    auto ui = UI::GetSingleton();
    if (!ui) return;

    GPtr<IMenu> menu = ui->GetMenu(menuName);
    if (!menu || !menu->uiMovie) {
        return;
    }

    auto movie = menu->uiMovie;

    GFxValue _root;
    movie->GetVariable(&_root, "_root");

    GFxValue fn1;
    movie->CreateFunction(&fn1, new RequestListHandler());
    _root.SetMember("RF_RequestList", fn1);
    GFxValue fn2;
    movie->CreateFunction(&fn2, new ChargeHandler());
    _root.SetMember("RF_Charge", fn2);
    GFxValue fn3;
    movie->CreateFunction(&fn3, new ItemIsUsingRF());
    _root.SetMember("RF_IsEnabled", fn3);

    GFxValue args[2];
    args[0] = GFxValue("RF");
    args[1] = GFxValue(658);
    _root.Invoke("createEmptyMovieClip", nullptr, args, 2);
    if (movie->GetVariable(&_root, "_root.RF")) {
        GFxValue args[1];
        args[0] = GFxValue("rechargefw_inject.swf");
        _root.Invoke("loadMovie", nullptr, args, 1);
    }
}

class MyEventSink : public RE::BSTEventSink<MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (event->menuName == InventoryMenu::MENU_NAME) {
            if (event->opening) {
                Inject(event->menuName);
            } else {
                buffer.clear();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        RechargeableKeyword = TESForm::LookupByEditorID<BGSKeyword>("RF_Rechargeable");
        if (!RechargeableKeyword) return;
        chargeKeywords[0] = TESForm::LookupByEditorID<BGSKeyword>("RF_RechargeValuePetty");
        chargeKeywords[1] = TESForm::LookupByEditorID<BGSKeyword>("RF_RechargeValueLesser");
        chargeKeywords[2] = TESForm::LookupByEditorID<BGSKeyword>("RF_RechargeValueCommon");
        chargeKeywords[3] = TESForm::LookupByEditorID<BGSKeyword>("RF_RechargeValueGreater");
        chargeKeywords[4] = TESForm::LookupByEditorID<BGSKeyword>("RF_RechargeValueGrand");
        chargeKeywords[5] = TESForm::LookupByEditorID<BGSKeyword>("RF_RechargeFull");

        auto* settings = GameSettingCollection::GetSingleton();
        chargeValues[0] = settings->GetSetting("iSoulLevelValuePetty")->GetInteger();
        chargeValues[1] = settings->GetSetting("iSoulLevelValueLesser")->GetInteger();
        chargeValues[2] = settings->GetSetting("iSoulLevelValueCommon")->GetInteger();
        chargeValues[3] = settings->GetSetting("iSoulLevelValueGreater")->GetInteger();
        chargeValues[4] = settings->GetSetting("iSoulLevelValueGrand")->GetInteger();
        chargeValues[5] = 0;

        static MyEventSink g_EventSink;
        UI::GetSingleton()->AddEventSink(&g_EventSink);
    } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
    }
}

bool UsesRechargeFramework(StaticFunctionTag*, TESForm* a_form) {
    if (!a_form) return false;
    if (a_form->Is(FormType::Weapon)) {
        return a_form->As<TESObjectWEAP>()->HasKeyword(RechargeableKeyword);
    } else if (a_form->Is(FormType::Enchantment)) {
        return a_form->As<EnchantmentItem>()->HasKeyword(RechargeableKeyword);
    }
    return false;
}

int GetItemChargeValue(StaticFunctionTag*, TESForm* a_form) {
    if (!a_form || !a_form->IsBoundObject()) return false;
    auto keywordForm = a_form->As<BGSKeywordForm>();
    int i = 0;
    for (const auto* kw : chargeKeywords) {
        if (keywordForm->HasKeyword(kw)) {
            return chargeValues[i];
        }
        i++;
    }

    return chargeValues[0];  // if no keyword is provided, the item is considered a "petty" gem
}

bool PapyrusBinder(RE::BSScript::IVirtualMachine* vm) {
    std::string_view scriptName = "RechargeFramework_Utils";

    vm->RegisterFunction("GetVersion", scriptName, GetPluginVersion);
    vm->RegisterFunction("UsesRechargeFramework", scriptName, UsesRechargeFramework);
    vm->RegisterFunction("GetItemChargeValue", scriptName, GetItemChargeValue);

    return false;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLog();
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    SKSE::GetPapyrusInterface()->Register(PapyrusBinder);
    return true;
}