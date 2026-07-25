#include "CheckpointManager.h"
#include "UI.h"

namespace
{
    std::jthread g_pollThread;
    constexpr std::uint32_t kSerializationID = 0x50525353;  // SSRP

    void OnSave(SKSE::SerializationInterface* a_intfc)
    {
        CheckpointManager::GetSingleton().Save(a_intfc);
    }

    void OnLoad(SKSE::SerializationInterface* a_intfc)
    {
        CheckpointManager::GetSingleton().Load(a_intfc);
    }

    void OnRevert(SKSE::SerializationInterface*)
    {
        CheckpointManager::GetSingleton().Revert();
    }

    void StartPolling()
    {
        if (g_pollThread.joinable()) {
            return;
        }

        g_pollThread = std::jthread([](std::stop_token a_stopToken) {
            using namespace std::chrono_literals;

            while (!a_stopToken.stop_requested()) {
                std::this_thread::sleep_for(100ms);
                if (a_stopToken.stop_requested()) {
                    break;
                }

                if (const auto taskInterface = SKSE::GetTaskInterface()) {
                    taskInterface->AddTask([]() {
                        CheckpointManager::GetSingleton().PollShadesState();
                    });
                }
            }
        });
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (a_message && a_message->type == SKSE::MessagingInterface::kDataLoaded) {
            CheckpointManager::GetSingleton().InitializeForms();
            StartPolling();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    Menu::Register();

    const auto serialization = SKSE::GetSerializationInterface();
    if (!serialization) {
        return false;
    }

    serialization->SetUniqueID(kSerializationID);
    serialization->SetSaveCallback(OnSave);
    serialization->SetLoadCallback(OnLoad);
    serialization->SetRevertCallback(OnRevert);

    const auto messaging = SKSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnSKSEMessage);
}
