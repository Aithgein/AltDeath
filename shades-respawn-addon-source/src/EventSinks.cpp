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

    source->AddEventSink<RE::TESSleepStopEvent>(
        static_cast<RE::BSTEventSink<RE::TESSleepStopEvent>*>(this));
    source->AddEventSink<RE::TESWaitStopEvent>(
        static_cast<RE::BSTEventSink<RE::TESWaitStopEvent>*>(this));
    source->AddEventSink<RE::TESMagicEffectApplyEvent>(
        static_cast<RE::BSTEventSink<RE::TESMagicEffectApplyEvent>*>(this));

    spdlog::info("Registered sleep, wait, and magic-effect event sinks");
}

RE::BSEventNotifyControl EventSinks::ProcessEvent(
    const RE::TESSleepStopEvent* a_event,
    RE::BSTEventSource<RE::TESSleepStopEvent>*)
{
    if (a_event && !a_event->interrupted) {
        CheckpointManager::GetSingleton().CaptureCheckpoint();
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSinks::ProcessEvent(
    const RE::TESWaitStopEvent* a_event,
    RE::BSTEventSource<RE::TESWaitStopEvent>*)
{
    if (a_event && !a_event->interrupted) {
        CheckpointManager::GetSingleton().CaptureCheckpoint();
    }

    return RE::BSEventNotifyControl::kContinue;
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
    const auto shadesEffect = CheckpointManager::GetSingleton().GetShadesEtherealEffect();

    if (player && target == player && shadesEffect &&
        a_event->magicEffect == shadesEffect->GetFormID()) {
        CheckpointManager::GetSingleton().QueueTeleport();
    }

    return RE::BSEventNotifyControl::kContinue;
}
