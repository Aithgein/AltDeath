#pragma once

class CheckpointManager final
{
public:
    static CheckpointManager& GetSingleton();

    void InitializeForms();
    void CaptureCheckpoint();
    void PollShadesState();
    void NotifyLoaded();

    void TriggerShadesResurrection();
    void QueueShadesStateCheck();

    void Save(SKSE::SerializationInterface* a_intfc);
    void Load(SKSE::SerializationInterface* a_intfc);
    void Revert();

    [[nodiscard]] RE::EffectSetting* GetShadesEtherealEffect() const noexcept;
    [[nodiscard]] RE::SpellItem* GetShadesEtherealSpell() const noexcept;

private:
    CheckpointManager() = default;

    [[nodiscard]] RE::TESObjectREFR* ResolveMarker() const;
    [[nodiscard]] bool IsShadesEtherealActive() const;

    void CaptureCheckpointNow();
    void QueueTeleport();
    void TeleportNow();
    void Notify(const char* a_message) const;

    static constexpr std::string_view kShadesPlugin = "shade-of-mortality.esp";
    static constexpr RE::FormID kShadesEtherealEffectLocalID = 0x800;
    static constexpr RE::FormID kShadesEtherealSpellLocalID = 0x801;
    static constexpr RE::FormID kShadesDeathCounterLocalID = 0x81D;

    static constexpr std::string_view kSkyrimPlugin = "Skyrim.esm";
    static constexpr RE::FormID kXMarkerLocalID = 0x3B;

    mutable std::mutex _lock;
    RE::FormID _markerFormID{ 0 };
    RE::TESBoundObject* _xMarkerBase{ nullptr };
    RE::EffectSetting* _shadesEtherealEffect{ nullptr };
    RE::SpellItem* _shadesEtherealSpell{ nullptr };
    RE::TESGlobal* _shadesDeathCounter{ nullptr };

    float _lastDeathCounterValue{ 0.0F };
    bool _pollBaselineInitialized{ false };
    bool _lastEtherealActive{ false };

    std::atomic_bool _checkpointPending{ false };
    std::atomic_bool _teleportPending{ false };
    std::atomic_bool _stateCheckPending{ false };
    std::atomic_bool _resurrectionLatched{ false };
};
