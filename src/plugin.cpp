#include "log.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <SimpleIni.h>

static constexpr RE::FormID kLockpickFormID = 0x00000A;
static constexpr RE::FormID kGoldFormID = 0x00000F;

static int kLockpicksToAdd = 10;
static int kGoldToRemove = 150;

void PlaySound(const char* soundName)
{
    auto* audioManager = RE::BSAudioManager::GetSingleton();
    if (!audioManager) return;

    RE::BSSoundHandle handle;
    audioManager->BuildSoundDataFromEditorID(handle, soundName, 0);
    if (handle.IsValid())
        handle.Play();
}

std::string GetTranslation(const std::string& key)
{
    std::string result;
    SKSE::Translation::Translate(key, result);
    return result;
}

class LockpickRestockHandler : public RE::BSTEventSink<RE::TESActivateEvent>
{
public:
    static LockpickRestockHandler* GetSingleton()
    {
        static LockpickRestockHandler instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESActivateEvent* a_event,
        RE::BSTEventSource<RE::TESActivateEvent>*) override
    {
        if (!a_event || !a_event->objectActivated || !a_event->actionRef)
            return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_event->actionRef.get() != player)
            return RE::BSEventNotifyControl::kContinue;

        auto* target = a_event->objectActivated.get();
        auto* lock = target ? target->GetLock() : nullptr;
        if (!lock || !lock->IsLocked())
            return RE::BSEventNotifyControl::kContinue;

        auto* lockpickForm = RE::TESForm::LookupByID<RE::TESBoundObject>(kLockpickFormID);
        auto* goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(kGoldFormID);
        if (!lockpickForm || !goldForm)
            return RE::BSEventNotifyControl::kContinue;

        if (player->GetItemCount(lockpickForm) > 0)
            return RE::BSEventNotifyControl::kContinue;

        if (player->GetItemCount(goldForm) < kGoldToRemove)
        {
            RE::DebugNotification(GetTranslation("$LockpickBribePoor").c_str());
            SKSE::log::info("Lockpick restock skipped – insufficient gold");
            return RE::BSEventNotifyControl::kContinue;
        }

        player->AddObjectToContainer(lockpickForm, nullptr, kLockpicksToAdd, nullptr);
        player->RemoveItem(goldForm, kGoldToRemove,
            RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

        PlaySound("ITMGoldDownSD");

        RE::DebugNotification(GetTranslation("$LockpickBribeSuccess").c_str());
        SKSE::log::info("Restocked {} lockpicks, removed {} gold", kLockpicksToAdd, kGoldToRemove);

        return RE::BSEventNotifyControl::kContinue;
    }
};

void OnDataLoaded()
{
    SKSE::Translation::ParseTranslation("LockpickBribe");

    auto* scriptEvents = RE::ScriptEventSourceHolder::GetSingleton();
    if (scriptEvents)
    {
        scriptEvents->AddEventSink<RE::TESActivateEvent>(LockpickRestockHandler::GetSingleton());
        SKSE::log::info("LockpickRestock: registered TESActivateEvent sink");
    }
    else
    {
        SKSE::log::error("LockpickRestock: failed to get ScriptEventSourceHolder");
    }
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type)
    {
    case SKSE::MessagingInterface::kDataLoaded:
        OnDataLoaded();
        break;
    case SKSE::MessagingInterface::kPostLoad:      break;
    case SKSE::MessagingInterface::kPreLoadGame:   break;
    case SKSE::MessagingInterface::kPostLoadGame:  break;
    case SKSE::MessagingInterface::kNewGame:       break;
    }
}

void LoadSettings()
{
    CSimpleIniA ini;
    ini.SetUnicode();

    const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    const std::string path = "Data/SKSE/Plugins/" + std::string(plugin->GetName()) + ".ini";
    ini.LoadFile(path.c_str());

    kLockpicksToAdd = static_cast<std::uint32_t>(ini.GetDoubleValue("General", "iLockpicksToAdd", 10));
    kGoldToRemove = static_cast<std::uint32_t>(ini.GetDoubleValue("General", "iGoldToRemove", 150));
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SetupLog();
    LoadSettings();

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", MessageHandler))
        return false;

    return true;
}