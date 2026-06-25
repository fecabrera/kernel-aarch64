import "debug";
import "cpu";
import "dtb";
import "syscall";
import "scheduler";
import "interrupts/gic";
import "interrupts/irq";

/**
 * Timing state maintained by the timer driver across each tick interrupt.
 * Used to compute the real elapsed interval and system uptime in milliseconds.
 */
struct sched_info {
    ticks: uint64;         // cntpct_el0 value captured at the last tick interrupt
    initial_ticks: uint64; // cntpct_el0 value captured at timer_init time
    frequency: uint64;     // counter frequency in Hz, read from cntfrq_el0
    interval: uint64;      // desired tick interval in milliseconds
}

const DEFAULT_TIMER_INTERVAL = 10;

@static let timer_irq: uint32;
@static let tinfo: struct sched_info;

/**
 * Initializes the ARM generic timer. Reads the timer frequency from cntfrq_el0,
 * sets the countdown from the current interval (default: DEFAULT_TIMER_INTERVAL),
 * enables the timer, and registers the IRQ with the GIC using the IRQ number
 * read from the DTB. Optionally call timer_set_interval() before this to
 * override the default interval.
 */
fn timer_init() {
    // Get frequency
    tinfo.initial_ticks = get_cntpct_el0();
    tinfo.frequency = get_cntfrq_el0();
    tinfo.interval = DEFAULT_TIMER_INTERVAL;

    // Set timer countdown
    set_cntp_tval_el0(time_quantum(1));

    // Enable timer, unmask interrupt
    set_cntp_ctl_el0(CNTP_CTL_ENABLE);

    // register uptime syscall
    syscall_register_handler(syscall::UPTIME, syscall_uptime_handler);

    if (dtb_get_timer_irq_number(&timer_irq) == 0) {
        dprintk("[timer] Initializing IRQ: %i\n", timer_irq);
        irq_register_handler(timer_irq, timer_irq_handler);
        gic_enable_irq(timer_irq);
    } else {
        dprintk("[timer] IRQ not found!!\n");
    }
}

/**
 * Returns the system uptime in milliseconds, computed from cntpct_el0 ticks
 * elapsed since timer_init was called.
 *
 * @return system uptime in milliseconds
 */
fn timer_get_uptime() -> uint64 {
    let ticks = get_cntpct_el0();
    dprintk("ticks = %llu, initial_ticks = %llu, uptime = %llu ms\n", ticks, tinfo.initial_ticks, hz_to_ms(ticks - tinfo.initial_ticks));
    return hz_to_ms(ticks - tinfo.initial_ticks);
}

/**
 * Sets the timer tick interval in milliseconds.
 * Must be called before timer_init().
 *
 * @param interval: tick interval in milliseconds (e.g. 10 for 10ms)
 */
fn timer_set_interval(interval: uint64) {
    tinfo.interval = interval;
}

/**
 * IRQ handler for the EL1 physical timer. Captures the current counter value,
 * computes the elapsed interval in milliseconds since the previous tick, calls
 * scheduler_handler with that interval to preempt the current task, then
 * reloads the countdown register for the next tick.
 * Registered with irq_register_handler at timer_init time.
 *
 * @param irq: IRQ ID passed by the dispatcher (unused)
 * @param ctx: saved register frame from the interrupted context
 *
 * @return saved context of the next task to run as selected by the scheduler
 */
fn timer_irq_handler(irq: uint32, ctx: struct cpu_context*) -> struct cpu_context* {
    let last_ticks = tinfo.ticks;

    tinfo.ticks = get_cntpct_el0();
    tinfo.frequency = get_cntfrq_el0();

    let interval = hz_to_ms(tinfo.ticks - last_ticks);
    let uptime = hz_to_ms(tinfo.ticks - tinfo.initial_ticks);

    let next_ctx = scheduler_handler(ctx, interval);

    // Set timer countdown
    set_cntp_tval_el0(time_quantum(1));

    return next_ctx;
}

/**
 * Syscall handler for SYSCALL_UPTIME. Reads the current cntpct_el0 tick count,
 * computes elapsed milliseconds since timer_init, and returns the result in x0.
 *
 * @param ctx: saved register frame of the calling process
 *
 * @return ctx with x0 set to the system uptime in milliseconds
 */
fn syscall_uptime_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    dprintk("[timer] uptime(), ctx->x0 = %u\n", ctx->x[0]);

    let uptime = timer_get_uptime();
    dprintk("[timer] uptime = %d ms\n", uptime);

    ctx->x[0] = uptime;
    return ctx;
}

/**
 * Converts a duration in milliseconds to counter ticks at the current frequency.
 *
 * @param value: duration in milliseconds
 *
 * @return equivalent number of counter ticks
 */
@inline fn ms_to_hz(value: uint64) -> uint64 {
    return value * tinfo.frequency / 1000;
}

/**
 * Converts a count of counter ticks to milliseconds at the current frequency.
 *
 * @param value: number of counter ticks
 *
 * @return equivalent duration in milliseconds
 */
@inline fn hz_to_ms(value: uint64) -> uint64 {
    return value * 1000 / tinfo.frequency;
}

/**
 * Returns the countdown value (in ticks) for n tick intervals, i.e. the number
 * of ticks to program into cntp_tval_el0 to fire after n * interval ms.
 *
 * @param n: number of tick intervals
 *
 * @return countdown value in counter ticks
 */
@inline fn time_quantum(n: uint64) -> uint64 {
    return ms_to_hz(n * tinfo.interval);
}
