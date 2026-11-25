/**
 * @file ev_timer.hpp
 * @brief Событийные миллисекундные таймеры: TimerService, EvMsTimer, EvRetryTimer.
 *
 * MsTimer и ReplyTimer не заменяются — их по-прежнему опрашивают через timedOut().
 * Здесь дедлайны проверяет TimerService::tick(now) и сам вызывает слушателя.
 *
 * Вызов только из основного цикла. Из аппаратного ISR tick() звать нельзя:
 * callback может снова взводить и отменять таймеры, критической секции нет.
 *
 * Срок — закрытая база MsTimer (start / stop / timedOut, то же полукольцо).
 * Очереди — mem::List: ожидающие и FIFO уже истёкших. Элемент живёт не больше
 * чем в одном списке. Куча не используется. Копирование и перемещение таймера
 * и сервиса запрещены: идентичность — адрес.
 *
 * Длительность должна быть < 2^31 мс. Сумму now+duration == 0 MsTimer сдвигает
 * на +1 мс, чтобы 0 по-прежнему значило «остановлен».
 *
 * Сервис не синглтон: ссылка передаётся в конструктор таймера. Независимые
 * экземпляры — разные домены времени и тесты.
 *
 * ---------------------------------------------------------------------------
 * Жизнь объектов
 * ---------------------------------------------------------------------------
 * Таймер не владеет ни слушателем, ни сервисом.
 * Слушатель и сервис должны жить, пока таймер могут start() или пока не
 * отработал его последний callback.
 * Из своего callback нельзя уничтожать этот таймер и его TimerService.
 * Другой таймер из callback уничтожить можно: деструктор снимет его со списка.
 * Деструктор сервиса молча снимает все таймеры, callback не вызывается.
 * После этого start() у них звать нельзя (ссылка на сервис мертва).
 * Уничтожение таймера раньше сервиса тоже безопасно: узел сам выходит из списка.
 *
 * ---------------------------------------------------------------------------
 * Состояния EvMsTimer
 * ---------------------------------------------------------------------------
 * Часы и список вместе:
 *   Idle    — не в списке, MsTimer остановлен.
 *             isRunning()==false, isPending()==false
 *   Waiting — в списке ожидания, MsTimer идёт.
 *             isRunning()==true
 *   Due     — снят с ожидания, часы остановлены, узел в очереди callback.
 *             isRunning()==false, isPending()==true
 *
 * Перед вызовом callback узел вынимается из очереди (часы уже стоят),
 * поэтому внутри callback свой таймер неактивен и его можно start() заново.
 *
 * ---------------------------------------------------------------------------
 * tick(now) — один проход
 * ---------------------------------------------------------------------------
 * 1. В очередь истёкших попадают только таймеры, которые уже ждали и чей срок
 *    наступил к этому вызову (MsTimer::timedOut). Ожидание отсортировано по
 *    абсолютному сроку, поэтому это префикс списка: с головы, пока она просрочена.
 *    Равные сроки сохраняют порядок постановки.
 * 2. При переносе часы останавливаются: до callback таймер уже не isRunning().
 * 3. Их callback'и выполняются в этом же проходе.
 * 4. start() из callback ставит таймер в ожидание. Проверяется он не раньше
 *    следующего tick, даже если длительность 0 и срок уже наступил.
 * 5. Вложенный tick() из callback ничего не делает.
 * 6. stop() ожидающего — в эту очередь он не попадёт.
 *    stop() того, кто уже в очереди истёкших — его callback не вызывается.
 *    start() по такому таймеру снимает старый callback и ставит новый срок
 *    в ожидание на место по порядку дедлайнов.
 * 7. Повторный start() уже ожидающего меняет срок и заново вставляет узел.
 *    Более ранний срок оказывается ближе к голове.
 *
 * ---------------------------------------------------------------------------
 * EvRetryTimer
 * ---------------------------------------------------------------------------
 * Наследник EvMsTimer. В сеть ничего не шлёт — только зовёт слушателя.
 * Счётчик как у ReplyTimer: каждый успешный start() делает ++attempts.
 * Просрочка при attempts < max  — ITimerListener::onTimer (можно повторять).
 * Просрочка при attempts >= max — только onAttemptsOver, onTimer не зовётся.
 * Новая серия: clear() (attempts=0, stop), затем start().
 * stop() снимает текущее окно и не обнуляет attempts. Полный сброс серии — clear().
 */

#pragma once

#include <cstdint>

#include "mem/list.hpp"
#include "ms_timer.hpp"

namespace MISC {

static_assert(sizeof(time_t) == sizeof(std::uint32_t), "half-modulus 2^31 рассчитан на 32-bit time_t");

class EvMsTimer;
class EvRetryTimer;
class TimerService;

/** Получатель истечения. Таймер его не удаляет и не владеет им. */
class ITimerListener {
public:
    /** @a timer — тот, чей срок вышел. По адресу различают несколько своих. */
    virtual void onTimer(EvMsTimer& timer) noexcept = 0;

protected:
    ITimerListener() = default;
    ~ITimerListener() = default;
};

/**
 * Слушатель EvRetryTimer.
 * onTimer — очередная попытка ещё разрешена (отправка — дело слушателя).
 * onAttemptsOver — лимит выбран, onTimer в этот раз не вызывается.
 */
class IRetryTimerListener : public ITimerListener {
public:
    virtual void onAttemptsOver(EvRetryTimer& timer) noexcept = 0;

protected:
    IRetryTimerListener() = default;
    ~IRetryTimerListener() = default;
};

/**
 * Одноразовый событийный таймер.
 * Обе базы закрытые: срок — MsTimer, узел очереди — mem::ListNode.
 * mem::List<EvMsTimer> — друг, иначе static_cast узла к таймеру недоступен.
 * Ожидание — пока часы идут; очередь callback — часы уже остановлены.
 */
class EvMsTimer : private mem::ListNode, private MsTimer {
    friend class mem::List<EvMsTimer>;

public:
    using MsTimer::kStopped;

    EvMsTimer(TimerService& service, ITimerListener& listener) noexcept
        : _service(service)
        , _listener(listener)
    {}

    virtual ~EvMsTimer() { stop(); }

    EvMsTimer(const EvMsTimer&) = delete;
    EvMsTimer& operator=(const EvMsTimer&) = delete;
    EvMsTimer(EvMsTimer&&) = delete;
    EvMsTimer& operator=(EvMsTimer&&) = delete;

    /**
     * Взвести срок now_ms + duration_ms (MsTimer::start) и встать в ожидание
     * по абсолютному сроку. Равный срок — после уже стоящих с тем же дедлайном.
     * Уже в очереди callback — старый callback снимается.
     */
    void start(time_t now_ms, time_t duration_ms) noexcept;

    /** Отмена: снять с ожидания или из очереди callback. Повторный stop() пустой. */
    void stop() noexcept;

    /** Ждёт срока. В очереди callback и внутри callback — false (часы остановлены). */
    using MsTimer::isRunning;

    /** Часы остановлены, узел ещё в очереди callback этого прохода. */
    [[nodiscard]] bool isPending() const noexcept { return linked() && !isRunning(); }

protected:
    /** Срок вышел, таймер уже Idle. По умолчанию — listener.onTimer(*this). */
    virtual void onExpired() noexcept;

    /**
     * start() уже поставил таймер в ожидание.
     * EvRetryTimer считает здесь попытку.
     */
    virtual void onArmed() noexcept {}

    [[nodiscard]] ITimerListener& listener() noexcept { return _listener; }

private:
    friend class TimerService;

    using MsTimer::timedOut;

    /** Остановить часы, не трогая список. */
    inline void stopTimer() noexcept { MsTimer::stop(); }

    TimerService& _service;
    ITimerListener& _listener;
};

/**
 * Попытки по таймауту. Каждый start() — новая попытка (++attempts), затем окно ожидания.
 * Истечение окна: ещё есть запас — onTimer; запас кончился — onAttemptsOver.
 * Отправку кадра делает слушатель, не таймер.
 * stop() снимает текущее окно и не обнуляет attempts. Полный сброс серии — clear().
 */
class EvRetryTimer : public EvMsTimer {
public:
    EvRetryTimer(TimerService& service, IRetryTimerListener& listener, uint8_t max_attempts) noexcept
        : EvMsTimer(service, listener)
        , _max(max_attempts)
    {}

    /** Остановить и обнулить счётчик. Слушателя не зовёт. */
    void clear() noexcept
    {
        stop();
        _attempts = 0;
    }

    [[nodiscard]] uint8_t attempts() const noexcept { return _attempts; }
    [[nodiscard]] uint8_t maxAttempts() const noexcept { return _max; }
    [[nodiscard]] bool isAttemptsOver() const noexcept { return _attempts >= _max; }

protected:
    void onExpired() noexcept override;
    void onArmed() noexcept override { ++_attempts; }

private:
    uint8_t _attempts = 0;
    uint8_t _max;
};

/**
 * Домен времени. Не глобальный: сколько экземпляров — столько независимых очередей.
 * tick() — из основного цикла, не из ISR.
 * Объявлен после EvMsTimer: mem::List требует полный тип элемента.
 */
class TimerService {
public:
    TimerService() noexcept = default;
    ~TimerService();

    TimerService(const TimerService&) = delete;
    TimerService& operator=(const TimerService&) = delete;
    TimerService(TimerService&&) = delete;
    TimerService& operator=(TimerService&&) = delete;

    /**
     * Снять просроченных на этот now и вызвать их слушателей.
     * Вложенный вызов из callback игнорируется.
     */
    void tick(time_t now_ms) noexcept;

private:
    friend class EvMsTimer;

    void disarm(mem::List<EvMsTimer>& list) noexcept;
    [[nodiscard]] bool enlist(EvMsTimer& timer) noexcept;

    mem::List<EvMsTimer> _waiting;
    mem::List<EvMsTimer> _due;
    bool _busy = false;
};

inline void EvMsTimer::start(time_t now_ms, time_t duration_ms) noexcept
{
    if (linked()) {
        if (MsTimer::isRunning()) {
            _service._waiting.remove(*this);
        } else {
            _service._due.remove(*this);
        }
    }
    MsTimer::start(now_ms, duration_ms);
    if (!_service.enlist(*this)) {
        stopTimer();
        return;
    }
    onArmed();
}

inline void EvMsTimer::stop() noexcept
{
    if (linked()) {
        if (MsTimer::isRunning()) {
            _service._waiting.remove(*this);
        } else {
            _service._due.remove(*this);
        }
    }
    stopTimer();
}

inline void EvMsTimer::onExpired() noexcept
{
    _listener.onTimer(*this);
}

inline void EvRetryTimer::onExpired() noexcept
{
    auto& sink = static_cast<IRetryTimerListener&>(listener());
    if (isAttemptsOver()) {
        sink.onAttemptsOver(*this);
        return;
    }
    sink.onTimer(*this);
}

inline TimerService::~TimerService()
{
    disarm(_waiting);
    disarm(_due);
}

inline void TimerService::tick(time_t now_ms) noexcept
{
    if (_busy) {
        return;
    }
    _busy = true;
    while (EvMsTimer* head = _waiting.front()) {
        if (!head->timedOut(now_ms)) {
            break;
        }
        EvMsTimer* t = _waiting.popFront();
        t->stopTimer();
        if (!_due.pushBack(*t)) {
            continue;
        }
    }
    while (EvMsTimer* t = _due.popFront()) {
        t->onExpired();
    }
    _busy = false;
}

inline bool TimerService::enlist(EvMsTimer& timer) noexcept
{
    EvMsTimer* const head = _waiting.front();
    if (head == nullptr) {
        return _waiting.pushBack(timer);
    }
    if (timer.expiresBefore(*head)) {
        return _waiting.pushFront(timer);
    }
    EvMsTimer* const tail = _waiting.back();
    if (tail != head && !timer.expiresBefore(*tail)) {
        return _waiting.pushBack(timer);
    }
    for (EvMsTimer* p = _waiting.next(*head); p != nullptr; p = _waiting.next(*p)) {
        if (timer.expiresBefore(*p)) {
            return _waiting.insertBefore(*p, timer);
        }
    }
    return _waiting.pushBack(timer);
}

inline void TimerService::disarm(mem::List<EvMsTimer>& list) noexcept
{
    while (EvMsTimer* t = list.popFront()) {
        t->stopTimer();
    }
}

} // namespace MISC
