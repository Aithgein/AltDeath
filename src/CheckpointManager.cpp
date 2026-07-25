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

void CheckpointManager::Notify(const char* a_message) const
{
    if (a_message && *a_message) {
        RE::SendHUDMessage::ShowHUDMessage(a_message);
    }
}

void CheckpointManager::InitializeForms()
{
    const auto dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        return;
    }

    _xMarkerBase = dataHandler->LookupForm<RE::TESBoundObject>(kXMarkerLocalID, kSkyrimPlugin);
    _shadesEtherealEffect = dataHandler->LookupForm<RE::EffectSetting>(
        kShadesEtherealEffectLocalID,
        kShadesPlugin);

    _pollBaselineInitialized = false;
}

RE::TESObjectREFR* CheckpointManager::ResolveMarker() const
{
    return _markerFormID != 0 ? RE::TESForm::LookupByID<RE::TESObjectREFR>(_markerFormID) : nullptr;
}

void CheckpointManager::SetRespawnLocation()
{
    bool expected = false;
    if (!_locationUpdatePending.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        _locationUpdatePending.store(false);
        return;
    }

    // SKSE Menu Framework renders off the game thread. Reference creation and
    // movement must run on Skyrim's game thread.
    taskInterface->AddTask([this]() {
        SetRespawnLocationNow();
        _locationUpdatePending.store(false);
    });
}

void CheckpointManager::SetRespawnLocationNow()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    // Skyrim.esm is always load index 00, making 0x0000003B the complete
    // FormID for XMarker if the data-handler lookup was unavailable.
    if (!_xMarkerBase) {
        _xMarkerBase = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000003B);
    }

    if (!_xMarkerBase) {
        return;
    }

    if (auto marker = ResolveMarker()) {
        marker->MoveTo(player);
        Notify("You are bound.");
        return;
    }

    const auto marker = player->PlaceObjectAtMe(_xMarkerBase, true);
    if (!marker) {
        return;
    }

    _markerFormID = marker->GetFormID();
    Notify("You are bound.");
}

void CheckpointManager::PollShadesState()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player || !_shadesEtherealEffect) {
        return;
    }

    const bool etherealActive = player->HasMagicEffect(_shadesEtherealEffect);

    if (!_pollBaselineInitialized) {
        _lastEtherealActive = etherealActive;
        _pollBaselineInitialized = true;
        return;
    }

    if (etherealActive && !_lastEtherealActive) {
        QueueTeleport();
    }

    _lastEtherealActive = etherealActive;
}

void CheckpointManager::QueueTeleport()
{
    if (!ResolveMarker()) {
        Notify("You are not bound.");
        return;
    }

    bool expected = false;
    if (!_teleportPending.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        _teleportPending.store(false);
        return;
    }

    // Preserve the proven two-task delay so Shades can finish its resurrection
    // work before the player is moved.
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
        return;
    }

    player->MoveTo(marker);
    Notify("Returned to respawn location");
}

void CheckpointManager::Save(SKSE::SerializationInterface* a_intfc)
{
    if (!a_intfc || !a_intfc->OpenRecord(kRecordType, kRecordVersion)) {
        return;
    }

    a_intfc->WriteRecordData(&_markerFormID, sizeof(_markerFormID));
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
        if (type != kRecordType || version != kRecordVersion || length != sizeof(RE::FormID)) {
            continue;
        }

        RE::FormID savedID = 0;
        if (a_intfc->ReadRecordData(&savedID, sizeof(savedID)) != sizeof(savedID)) {
            continue;
        }

        RE::FormID resolvedID = 0;
        if (savedID != 0 && !a_intfc->ResolveFormID(savedID, resolvedID)) {
            resolvedID = 0;
        }

        _markerFormID = resolvedID;
    }

    _pollBaselineInitialized = false;
}

void CheckpointManager::Revert()
{
    _markerFormID = 0;
    _pollBaselineInitialized = false;
    _lastEtherealActive = false;
    _locationUpdatePending.store(false);
    _teleportPending.store(false);
}
