#include "CheckpointManager.h"
#include "EventSinks.h"

EventSinks& EventSinks::GetSingleton()
{
    static EventSinks singleton;
    return singleton;
}

void EventSinks::Register()
{
    const auto source = RE::ScriptEventSourceHolder::GetSingleton();
    if (!source) {
        spdlog::error("ScriptEventSourceHolder was unavailable");
        return;
    }

    source->AddEventSink<RE::TESMagicEffectApplyEvent>(
        static_cast<RE::BSTEventSink<RE::TESMagicEffectApplyEvent>*>(this));
    source->AddEventSink<RE::TESActiveEffectApplyRemoveEvent>(
        static_cast<RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent>*>(this));
    source->AddEventSink<RE::TESHitEvent>(
        static_cast<RE::BSTEventSink<RE::TESHitEvent>*>(this));

    spdlog::info("Registered magic-effect, active-effect, and hit event sinks");
}

RE::BSEventNotifyControl EventSinks::ProcessEvent(
    const RE::TESMagicEffectApplyEvent* a_event,
    RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto target = a_event->target.get();
    if (!player || target != player) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto& manager = CheckpointManager::GetSingleton();
    const auto effect = manager.GetShadesEtherealEffect();
    const auto spell = manager.GetShadesEtherealSpell();

    // TESMagicEffectApplyEvent should report the MGEF FormID. Accepting the
    // spell FormID as well costs nothing and protects against runtime quirks.
    if ((effect && a_event->magicEffect == effect->GetFormID()) ||
        (spell && a_event->magicEffect == spell->GetFormID())) {
        spdlog::info("Observed exact Shades resurrection effect event {:08X}", a_event->magicEffect);
        manager.TriggerShadesResurrection();
    } else {
        // The event itself may arrive before the active effect is visible.
        manager.QueueShadesStateCheck();
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSinks::ProcessEvent(
    const RE::TESActiveEffectApplyRemoveEvent* a_event,
    RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>*)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (player && a_event->target.get() == player) {
        CheckpointManager::GetSingleton().QueueShadesStateCheck();
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSinks::ProcessEvent(
    const RE::TESHitEvent* a_event,
    RE::BSTEventSource<RE::TESHitEvent>*)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (player && a_event->target.get() == player) {
        // A fatal hit is processed by Resurrection API during the same game
        // update. The deferred state check sees Shades' ethereal effect after
        // its resurrection callback has run.
        CheckpointManager::GetSingleton().QueueShadesStateCheck();
    }

    return RE::BSEventNotifyControl::kContinue;
}
