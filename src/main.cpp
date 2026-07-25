#include "CheckpointManager.h"
#include "EventSinks.h"

namespace
{
    std::jthread g_pollThread;

    void SetupLog()
    {
        auto logDirectory = SKSE::log::log_directory();
        if (!logDirectory) {
            return;
        }

        *logDirectory /= "shades-respawn-addon.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            logDirectory->string(),
            true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
    }

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

        spdlog::info("Started resurrection polling service");
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        switch (a_message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
        {
            auto& manager = CheckpointManager::GetSingleton();
            manager.InitializeForms();
            EventSinks::GetSingleton().Register();
            StartPolling();
            break;
        }
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            CheckpointManager::GetSingleton().NotifyLoaded();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SetupLog();

    const auto serialization = SKSE::GetSerializationInterface();
    if (!serialization) {
        spdlog::critical("SKSE serialization interface was unavailable");
        return false;
    }

    serialization->SetUniqueID(kSerializationID);
    serialization->SetSaveCallback(OnSave);
    serialization->SetLoadCallback(OnLoad);
    serialization->SetRevertCallback(OnRevert);

    const auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        spdlog::critical("Failed to register the SKSE messaging listener");
        return false;
    }

    spdlog::info("Shades Respawn Addon DLL loaded");
    return true;
}
