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

    if (_shadesEtherealEffect) {
        spdlog::info(
            "Resolved Shades ethereal effect: {:08X}",
            _shadesEtherealEffect->GetFormID());
    } else {
        spdlog::error(
            "Could not resolve Shades ethereal effect {:03X} from {}. The addon will remain inactive.",
            kShadesEtherealEffectLocalID,
            kShadesPlugin);
    }
}

RE::EffectSetting* CheckpointManager::GetShadesEtherealEffect() const noexcept
{
    return _shadesEtherealEffect;
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

void CheckpointManager::CaptureCheckpoint()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player || !_xMarkerBase) {
        return;
    }

    if (auto marker = ResolveMarker()) {
        marker->MoveTo(player);
        spdlog::info("Updated respawn checkpoint at marker {:08X}", marker->GetFormID());
        return;
    }

    const auto marker = player->PlaceObjectAtMe(_xMarkerBase, true);
    if (!marker) {
        spdlog::error("Failed to create the runtime checkpoint marker");
        return;
    }

    {
        const std::scoped_lock lock(_lock);
        _markerFormID = marker->GetFormID();
    }

    spdlog::info("Created respawn checkpoint marker {:08X}", marker->GetFormID());
}

void CheckpointManager::QueueTeleport()
{
    if (!ResolveMarker()) {
        spdlog::warn("Shades resurrected the player, but no sleep/wait checkpoint exists yet");
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

    // Defer until Shades has returned from its resurrection callback.
    taskInterface->AddTask([this]() {
        TeleportNow();
        _teleportPending.store(false);
    });
}

void CheckpointManager::TeleportNow()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto marker = ResolveMarker();
    if (!player || !marker) {
        spdlog::error("Checkpoint vanished before teleport could run");
        return;
    }

    player->MoveTo(marker);
    spdlog::info("Teleported player to checkpoint {:08X}", marker->GetFormID());
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
}
