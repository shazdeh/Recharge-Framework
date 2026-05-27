#include "logger.h"
#include "ClibUtil/editorID.hpp";

std::string_view keywordID = "RF_Rechargeable";
std::string_view RechargeFull = "RF_RechargeFull";
std::unordered_map<std::string_view, const char*> chargeMap = {
    {"RF_RechargeValueGrand", "iSoulLevelValueGrand"},
    {"RF_RechargeValueGreater", "iSoulLevelValueGreater"},
    {"RF_RechargeValueCommon", "iSoulLevelValueCommon"},
    {"RF_RechargeValueLesser", "iSoulLevelValueLesser"},
    {"RF_RechargeValuePetty", "iSoulLevelValuePetty"}
};
std::string buffer;

bool IsRechargable(StaticFunctionTag*, TESForm* a_form) {
    if (!a_form) return false;
    if (a_form->HasKeywordByEditorID(keywordID)) {
        return true;
    }
    return false;
}

int GetItemCharge(StaticFunctionTag*, TESForm* a_form) {
    if (!a_form) return false;
    for (auto& charge : chargeMap) {
        if (a_form->HasKeywordByEditorID(charge.first)) {
            auto* value = GameSettingCollection::GetSingleton()->GetSetting(charge.second);
            return value->data.i;
        }
    }
    if (a_form->HasKeywordByEditorID(RechargeFull)) return 0;
    return GameSettingCollection::GetSingleton()->GetSetting("iSoulLevelValuePetty")->GetInteger();
}

class RequestListHandler : public GFxFunctionHandler {
public:
    virtual void Call(Params& params) override {
        if (params.argCount > 0 && params.args[0].IsString()) {
            buffer.clear();
            // std::string formID = params.args[0].GetString();
            std::string keywordID = params.args[0].GetString();
            BGSListForm* list = TESForm::LookupByEditorID<BGSListForm>(keywordID + "List");
            if (list) {
                auto* player = PlayerCharacter::GetSingleton();
                list->ForEachForm([player](TESForm* form) {
                    if (form && form->IsBoundObject()) {
                        auto* object = form->As<TESBoundObject>();
                        auto count = player->GetItemCount(object);
                        if (count < 1) return BSContainer::ForEachResult::kContinue;
                        buffer.append(clib_util::editorID::get_editorID(form) + "//" + form->GetName() + "//" +
                                      std::to_string(count) + "//" +
                                      std::to_string(GetItemCharge(nullptr, form)) + "||");
                    }
                    return BSContainer::ForEachResult::kContinue;
                });
                ConsoleLog::GetSingleton()->Print(fmt::format("data: {}", buffer).c_str());
                params.retVal->SetString(buffer);
            }
        }
    }
};

class ChargeHandler : public GFxFunctionHandler {
public:
    virtual void Call(Params& params) override {
        if (params.argCount > 1 && params.args[0].IsString() && params.args[1].IsBool()) {
            std::string formID = params.args[0].GetString();
            bool isLeft = params.args[1].GetBool();
            auto* form = TESForm::LookupByEditorID(formID);
            if (!form || !form->IsBoundObject()) return;
            auto* object = form->As<TESBoundObject>();
            auto* player = PlayerCharacter::GetSingleton();
            if (!player->GetItemCount(object)) return;
            auto* avo = player->AsActorValueOwner();
            auto av = isLeft ? ActorValue::kLeftItemCharge : ActorValue::kRightItemCharge;
            avo->ModActorValue(ACTOR_VALUE_MODIFIER::kDamage, av, GetItemCharge(nullptr, form));
            player->RemoveItem(object, 1, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            PlaySound("UIEnchantRecharge");
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
    _root.SetMember("RE_RequestList", fn1);
    GFxValue fn2;
    movie->CreateFunction(&fn2, new ChargeHandler());
    _root.SetMember("RE_Charge", fn2);

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
        static MyEventSink g_EventSink;
        UI::GetSingleton()->AddEventSink(&g_EventSink);
    } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
    }
}

bool PapyrusBinder(RE::BSScript::IVirtualMachine* vm) {
    std::string_view scriptName = "RechargeFramework_Utils";

    vm->RegisterFunction("IsRechargable", scriptName, IsRechargable);
    vm->RegisterFunction("GetItemCharge", scriptName, GetItemCharge);

    return false;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLog();
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    SKSE::GetPapyrusInterface()->Register(PapyrusBinder);
    return true;
}