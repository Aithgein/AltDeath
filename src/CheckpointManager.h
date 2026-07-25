#pragma once

class CheckpointManager final
{
public:
    static CheckpointManager& GetSingleton();

    void InitializeForms();
    void SetRespawnLocation();
    void PollShadesState();

    void Save(SKSE::SerializationInterface* a_intfc);
    void Load(SKSE::SerializationInterface* a_intfc);
    void Revert();

private:
    CheckpointManager() = default;

    [[nodiscard]] RE::TESObjectREFR* ResolveMarker() const;

    void SetRespawnLocationNow();
    void QueueTeleport();
    void TeleportNow();
    void Notify(const char* a_message) const;

    static constexpr std::string_view kShadesPlugin = "shade-of-mortality.esp";
    static constexpr RE::FormID kShadesEtherealEffectLocalID = 0x800;

    static constexpr std::string_view kSkyrimPlugin = "Skyrim.esm";
    static constexpr RE::FormID kXMarkerLocalID = 0x3B;

    RE::FormID _markerFormID{ 0 };
    RE::TESBoundObject* _xMarkerBase{ nullptr };
    RE::EffectSetting* _shadesEtherealEffect{ nullptr };

    bool _pollBaselineInitialized{ false };
    bool _lastEtherealActive{ false };

    std::atomic_bool _locationUpdatePending{ false };
    std::atomic_bool _teleportPending{ false };
};
