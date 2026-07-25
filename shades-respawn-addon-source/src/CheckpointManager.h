#pragma once

class CheckpointManager final
{
public:
    static CheckpointManager& GetSingleton();

    void InitializeForms();
    void CaptureCheckpoint();
    void QueueTeleport();

    void Save(SKSE::SerializationInterface* a_intfc);
    void Load(SKSE::SerializationInterface* a_intfc);
    void Revert();

    [[nodiscard]] RE::EffectSetting* GetShadesEtherealEffect() const noexcept;

private:
    CheckpointManager() = default;

    [[nodiscard]] RE::TESObjectREFR* ResolveMarker() const;
    void TeleportNow();

    static constexpr std::string_view kShadesPlugin = "shade-of-mortality.esp";
    static constexpr RE::FormID kShadesEtherealEffectLocalID = 0x800;

    // XMarker in Skyrim.esm. It is invisible and suitable as a MoveTo target.
    static constexpr std::string_view kSkyrimPlugin = "Skyrim.esm";
    static constexpr RE::FormID kXMarkerLocalID = 0x3B;

    mutable std::mutex _lock;
    RE::FormID _markerFormID{ 0 };
    RE::TESBoundObject* _xMarkerBase{ nullptr };
    RE::EffectSetting* _shadesEtherealEffect{ nullptr };
    std::atomic_bool _teleportPending{ false };
};
