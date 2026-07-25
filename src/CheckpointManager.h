#pragma once

class CheckpointManager final
{
public:
    static CheckpointManager& GetSingleton();

    void InitializeForms();
    void CaptureCheckpoint();

    // Called when an exact Shades resurrection effect event is observed.
    void TriggerShadesResurrection();

    // Called after events that may precede or follow the Shades ethereal effect.
    // The actual check is deferred to the game thread.
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

    void QueueTeleport();
    void TeleportNow();

    static constexpr std::string_view kShadesPlugin = "shade-of-mortality.esp";
    static constexpr RE::FormID kShadesEtherealEffectLocalID = 0x800;
    static constexpr RE::FormID kShadesEtherealSpellLocalID = 0x801;

    // XMarker in Skyrim.esm. It is invisible and suitable as a MoveTo target.
    static constexpr std::string_view kSkyrimPlugin = "Skyrim.esm";
    static constexpr RE::FormID kXMarkerLocalID = 0x3B;

    mutable std::mutex _lock;
    RE::FormID _markerFormID{ 0 };
    RE::TESBoundObject* _xMarkerBase{ nullptr };
    RE::EffectSetting* _shadesEtherealEffect{ nullptr };
    RE::SpellItem* _shadesEtherealSpell{ nullptr };

    std::atomic_bool _teleportPending{ false };
    std::atomic_bool _stateCheckPending{ false };
    std::atomic_bool _resurrectionLatched{ false };
};
