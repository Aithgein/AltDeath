#include "CheckpointManager.h"

namespace
{
    constexpr std::uint32_t kRecordType = 0x54504B43;  // CKPT
    constexpr std::uint32_t kRecordVersion = 1;
}

CheckpointManager& CheckpointManager::GetSingleton()
{
    static CheckpointManager singleton;
    return singleton;
}

void CheckpointManager::InitializeForms()
{
    const auto dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        spdlog::error("TESDataHandler was unavailable");
        return;
    }

    _xMarkerBase = dataHandler->LookupForm<RE::TESBoundObject>(kXMarkerLocalID, kSkyrimPlugin);
    if (!_xMarkerBase) {
        spdlog::error("Could not resolve XMarker {:08X} from {}", kXMarkerLocalID, kSkyrimPlugin);
    }

    _shadesEtherealEffect = dataHandler->LookupForm<RE::EffectSetting>(
        kShadesEtherealEffectLocalID,
        kShadesPlugin);
    _shadesEtherealSpell = dataHandler->LookupForm<RE::SpellItem>(
        kShadesEtherealSpellLocalID,
        kShadesPlugin);

    if (_shadesEtherealEffect && _shadesEtherealSpell) {
        spdlog::info(
            "Resolved Shades ethereal effect {:08X} and spell {:08X}",
            _shadesEtherealEffect->GetFormID(),
            _shadesEtherealSpell->GetFormID());
        RE::DebugNotification("Shades Respawn Addon loaded");
    } else {
        spdlog::error(
            "Could not resolve Shades forms from {} (effect {:03X}, spell {:03X}). Addon inactive.",
            kShadesPlugin,
            kShadesEtherealEffectLocalID,
            kShadesEtherealSpellLocalID);
        RE::DebugNotification("Shades Respawn Addon: Shades forms not found");
    }
}

RE::EffectSetting* CheckpointManager::GetShadesEtherealEffect() const noexcept
{
    return _shadesEtherealEffect;
}

RE::SpellItem* CheckpointManager::GetShadesEtherealSpell() const noexcept
{
    return _shadesEtherealSpell;
}

RE::TESObjectREFR* CheckpointManager::ResolveMarker() const
{
    RE::FormID markerID = 0;
    {
        const std::scoped_lock lock(_lock);
        markerID = _markerFormID;
    }

    return markerID != 0 ? RE::TESForm::LookupByID<RE::TESObjectREFR>(markerID) : nullptr;
}

bool CheckpointManager::IsShadesEtherealActive() const
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    return player && _shadesEtherealEffect && player->HasMagicEffect(_shadesEtherealEffect);
}

void CheckpointManager::CaptureCheckpoint()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player || !_xMarkerBase) {
        spdlog::error("Could not capture checkpoint: player or XMarker base unavailable");
        RE::DebugNotification("Shades Respawn: checkpoint failed");
        return;
    }

    if (auto marker = ResolveMarker()) {
        marker->MoveTo(player);
        spdlog::info("Updated respawn checkpoint at marker {:08X}", marker->GetFormID());
        RE::DebugNotification("Respawn point updated");
        return;
    }

    const auto marker = player->PlaceObjectAtMe(_xMarkerBase, true);
    if (!marker) {
        spdlog::error("Failed to create the runtime checkpoint marker");
        RE::DebugNotification("Shades Respawn: checkpoint failed");
        return;
    }

    {
        const std::scoped_lock lock(_lock);
        _markerFormID = marker->GetFormID();
    }

    spdlog::info("Created respawn checkpoint marker {:08X}", marker->GetFormID());
    RE::DebugNotification("Respawn point updated");
}

void CheckpointManager::TriggerShadesResurrection()
{
    bool expected = false;
    if (!_resurrectionLatched.compare_exchange_strong(expected, true)) {
        return;
    }

    spdlog::info("Shades resurrection detected");
    QueueTeleport();
}

void CheckpointManager::QueueShadesStateCheck()
{
    bool expected = false;
    if (!_stateCheckPending.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        _stateCheckPending.store(false);
        spdlog::error("SKSE task interface was unavailable for state check");
        return;
    }

    // Two queued tasks place the check after the current event and after the
    // Resurrection API subscriber has had a chance to apply Shades' effect.
    taskInterface->AddTask([this]() {
        const auto secondTask = SKSE::GetTaskInterface();
        if (!secondTask) {
            _stateCheckPending.store(false);
            return;
        }

        secondTask->AddTask([this]() {
            const bool shadesActive = IsShadesEtherealActive();
            if (shadesActive) {
                TriggerShadesResurrection();
            } else {
                _resurrectionLatched.store(false);
            }
            _stateCheckPending.store(false);
        });
    });
}

void CheckpointManager::QueueTeleport()
{
    if (!ResolveMarker()) {
        spdlog::warn("Shades resurrected the player, but no sleep/wait checkpoint exists yet");
        RE::DebugNotification("Shades Respawn: sleep or wait first");
        return;
    }

    bool expected = false;
    if (!_teleportPending.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        _teleportPending.store(false);
        spdlog::error("SKSE task interface was unavailable");
        return;
    }

    // Leave two game-thread turns between Shades' resurrection callback and
    // MoveTo. This avoids moving the player while the fatal-hit callback is
    // still unwinding.
    taskInterface->AddTask([this]() {
        const auto secondTask = SKSE::GetTaskInterface();
        if (!secondTask) {
            _teleportPending.store(false);
            return;
        }

        secondTask->AddTask([this]() {
            TeleportNow();
            _teleportPending.store(false);
        });
    });
}

void CheckpointManager::TeleportNow()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto marker = ResolveMarker();
    if (!player || !marker) {
        spdlog::error("Checkpoint vanished before teleport could run");
        RE::DebugNotification("Shades Respawn: teleport failed");
        return;
    }

    player->MoveTo(marker);
    spdlog::info("Teleported player to checkpoint {:08X}", marker->GetFormID());
    RE::DebugNotification("Returned to last rest point");
}

void CheckpointManager::Save(SKSE::SerializationInterface* a_intfc)
{
    if (!a_intfc) {
        return;
    }

    RE::FormID markerID = 0;
    {
        const std::scoped_lock lock(_lock);
        markerID = _markerFormID;
    }

    if (!a_intfc->OpenRecord(kRecordType, kRecordVersion)) {
        spdlog::error("Failed to open serialization record");
        return;
    }

    if (!a_intfc->WriteRecordData(&markerID, sizeof(markerID))) {
        spdlog::error("Failed to write checkpoint marker FormID");
    }
}

void CheckpointManager::Load(SKSE::SerializationInterface* a_intfc)
{
    if (!a_intfc) {
        return;
    }

    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;

    while (a_intfc->GetNextRecordInfo(type, version, length)) {
        if (type != kRecordType) {
            spdlog::warn("Skipping unknown serialization record {:08X}", type);
            continue;
        }

        if (version != kRecordVersion || length != sizeof(RE::FormID)) {
            spdlog::error(
                "Unsupported checkpoint record: version {}, length {}",
                version,
                length);
            continue;
        }

        RE::FormID savedID = 0;
        if (a_intfc->ReadRecordData(&savedID, sizeof(savedID)) != sizeof(savedID)) {
            spdlog::error("Failed to read checkpoint marker FormID");
            continue;
        }

        RE::FormID resolvedID = 0;
        if (savedID != 0 && !a_intfc->ResolveFormID(savedID, resolvedID)) {
            spdlog::warn("Could not resolve saved checkpoint marker {:08X}", savedID);
            resolvedID = 0;
        }

        {
            const std::scoped_lock lock(_lock);
            _markerFormID = resolvedID;
        }

        spdlog::info("Loaded checkpoint marker {:08X}", resolvedID);
    }
}

void CheckpointManager::Revert()
{
    {
        const std::scoped_lock lock(_lock);
        _markerFormID = 0;
    }
    _teleportPending.store(false);
    _stateCheckPending.store(false);
    _resurrectionLatched.store(false);
}
