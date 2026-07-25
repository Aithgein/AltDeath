#pragma once

class EventSinks final :
    public RE::BSTEventSink<RE::TESSleepStopEvent>,
    public RE::BSTEventSink<RE::TESWaitStopEvent>,
    public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
{
public:
    static EventSinks& GetSingleton();
    void Register();

private:
    EventSinks() = default;

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESSleepStopEvent* a_event,
        RE::BSTEventSource<RE::TESSleepStopEvent>* a_source) override;

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESWaitStopEvent* a_event,
        RE::BSTEventSource<RE::TESWaitStopEvent>* a_source) override;

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESMagicEffectApplyEvent* a_event,
        RE::BSTEventSource<RE::TESMagicEffectApplyEvent>* a_source) override;
};
