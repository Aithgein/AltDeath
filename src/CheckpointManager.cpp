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

void CheckpointManager::NotifyLoaded()
{
    Notify("Shades Respawn Addon loaded");
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
    _shadesDeathCounter = dataHandler->LookupForm<RE::TESGlobal>(
        kShadesDeathCounterLocalID,
        kShadesPlugin);

    if (_shadesEtherealEffect && _shadesEtherealSpell) {
        spdlog::info(
            "Resolved Shades ethereal effect {:08X}, spell {:08X}, death counter {}",
            _shadesEtherealEffect->GetFormID(),
            _shadesEtherealSpell->GetFormID(),
            _shadesDeathCounter ? "present" : "missing");
    } else {
        spdlog::error(
            "Could not resolve Shades forms from {} (effect {:03X}, spell {:03X}). Addon inactive.",
            kShadesPlugin,
            kShadesEtherealEffectLocalID,
            kShadesEtherealSpellLocalID);
    }

    _pollBaselineInitialized = false;
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

void CheckpointManager::SetRespawnLocation()
{
    bool expected = false;
    if (!_locationUpdatePending.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        _locationUpdatePending.store(false);
        spdlog::error("SKSE task interface was unavailable for respawn-location update");
        Notify("Shades Respawn: location update failed");
        return;
    }

    // SKSE Menu Framework renders off the game thread. Run the actual
    // reference creation or movement on Skyrim's game thread.
    taskInterface->AddTask([this]() {
        SetRespawnLocationNow();
        _locationUpdatePending.store(false);
    });
}

void CheckpointManager::SetRespawnLocationNow()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        spdlog::error("Could not set respawn location: player unavailable");
        Notify("Shades Respawn: location update failed");
        return;
    }

    // Use the direct base-game lookup as a fallback. Skyrim.esm is always
    // load index 00, so 0x0000003B is the complete FormID for XMarker.
    if (!_xMarkerBase) {
        _xMarkerBase = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000003B);
    }

    if (!_xMarkerBase) {
        spdlog::error("Could not set respawn location: XMarker base unavailable");
        Notify("Shades Respawn: location update failed");
        return;
    }

    if (auto marker = ResolveMarker()) {
        marker->MoveTo(player);
        spdlog::info("Updated respawn location marker {:08X}", marker->GetFormID());
        Notify("Respawn location updated");
        return;
    }

    const auto marker = player->PlaceObjectAtMe(_xMarkerBase, true);
    if (!marker) {
        spdlog::error("Failed to create the runtime respawn marker on deferred game task");
        Notify("Shades Respawn: location update failed");
        return;
    }

    {
        const std::scoped_lock lock(_lock);
        _markerFormID = marker->GetFormID();
    }

    spdlog::info("Created respawn location marker {:08X}", marker->GetFormID());
    Notify("Respawn location updated");
}

void CheckpointManager::PollShadesState()
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player || !_shadesEtherealEffect) {
        return;
    }

    const bool etherealActive = player->HasMagicEffect(_shadesEtherealEffect);
    const float deathCounterValue = _shadesDeathCounter ? _shadesDeathCounter->value : 0.0F;

    if (!_pollBaselineInitialized) {
        _lastEtherealActive = etherealActive;
        _lastDeathCounterValue = deathCounterValue;
        _pollBaselineInitialized = true;
        spdlog::info(
            "Initialized resurrection polling baseline: ethereal={}, counter={}",
            etherealActive,
            deathCounterValue);
        return;
    }

    const bool etherealStarted = etherealActive && !_lastEtherealActive;
    const bool counterIncreased = _shadesDeathCounter && deathCounterValue > (_lastDeathCounterValue + 0.001F);

    if (etherealStarted || counterIncreased) {
        spdlog::info(
            "Polling detected Shades resurrection: etherealStarted={}, counter {} -> {}",
            etherealStarted,
            _lastDeathCounterValue,
            deathCounterValue);
        TriggerShadesResurrection();
    }

    if (!etherealActive) {
        _resurrectionLatched.store(false);
    }

    _lastEtherealActive = etherealActive;
    _lastDeathCounterValue = deathCounterValue;
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

    taskInterface->AddTask([this]() {
        PollShadesState();
        _stateCheckPending.store(false);
    });
}

void CheckpointManager::QueueTeleport()
{
    if (!ResolveMarker()) {
        spdlog::warn("Shades resurrected the player, but no respawn location has been set");
        Notify("Shades Respawn: set a respawn location first");
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
        spdlog::error("Respawn location vanished before teleport could run");
        Notify("Shades Respawn: teleport failed");
        return;
    }

    player->MoveTo(marker);
    spdlog::info("Teleported player to respawn location {:08X}", marker->GetFormID());
    Notify("Returned to respawn location");
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
        spdlog::error("Failed to write respawn marker FormID");
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
            spdlog::error("Unsupported respawn-location record: version {}, length {}", version, length);
            continue;
        }

        RE::FormID savedID = 0;
        if (a_intfc->ReadRecordData(&savedID, sizeof(savedID)) != sizeof(savedID)) {
            spdlog::error("Failed to read respawn marker FormID");
            continue;
        }

        RE::FormID resolvedID = 0;
        if (savedID != 0 && !a_intfc->ResolveFormID(savedID, resolvedID)) {
            spdlog::warn("Could not resolve saved respawn marker {:08X}", savedID);
            resolvedID = 0;
        }

        {
            const std::scoped_lock lock(_lock);
            _markerFormID = resolvedID;
        }

        spdlog::info("Loaded respawn marker {:08X}", resolvedID);
    }

    _pollBaselineInitialized = false;
}

void CheckpointManager::Revert()
{
    {
        const std::scoped_lock lock(_lock);
        _markerFormID = 0;
    }

    _lastDeathCounterValue = 0.0F;
    _pollBaselineInitialized = false;
    _lastEtherealActive = false;
    _locationUpdatePending.store(false);
    _teleportPending.store(false);
    _stateCheckPending.store(false);
    _resurrectionLatched.store(false);
}
