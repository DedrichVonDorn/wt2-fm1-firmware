/* SPDX-License-Identifier: GPL-3.0-only
 * Boot, audio ISR and main loop adapted from Felucca's main.c / audio.c, Copyright (C) 2026
 * Leo Kuroshita (@kurogedelic), Hugelton Instruments. */
/* WT-1 on the FM-1 (X0X's boot, as Felucca's): boot (WDT first, boot-loop guard, guards), LCD, input (TIMER5 10 kHz),
 * audio (ALNK0), USB; then the main loop: UI at ~60 frames/s, input polled in between. */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[], _c1_start[], _c1_end[], _c1_load[];

/* --------------------------------------------------------------- audio --- */
/* X0X: 512-frame halves (11.6 ms; Felucca 256). The render runs inside the DMA's deadline, and a
 * busy pattern averaging ~80 % ran over it about one half in 300 (a heavy step): a dropped half.
 * Twice the half averages a heavy step over twice the time, for one more half of latency. */
#define FS 44100u                           /* the I2S rate (about 44,145 Hz measured on X0X) */
#define HALF_FRAMES 512u                    /* I2S half buffer: 11.6 ms at 44.1 kHz */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define REND_FRAMES 256u                    /* engine_render's most per call */
#if FELUCCA_UAC
_Static_assert(REND_FRAMES == UA_HALF, "usb.c sizes the USB audio ring band for this block");
#endif
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

#define DBG_MAGIC 0x44424731u                       /* "DBG1": read with `fm1t memr` */
struct x0x_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots, stage;
    uint32_t crash_seen;                            /* the crash count already reported at a boot */
} x0x_dbg __attribute__((section(".noinit")));
static uint8_t safe_mode;
#if X0X_DEBUG
/* development: the slowest block since boot, taken apart (read over USB: tools/fm1_debug.py) */
extern volatile uint32_t eng_dbg_wait, eng_dbg_nsub;
static volatile uint32_t dbg_tmr;                   /* 24 MHz ticks of TIMER5 nested in the render */
struct x0x_worst_s {
    uint32_t magic, us, wait, tmr, nsub, half_no, over90, over100, blocks, late_in;
} x0x_worst;
#endif                           /* two failed boots: no audio, USB on (safe_main) */

static void render_block(int32_t *o)                /* ALNK0: one half, measured */
{
    uint32_t t0 = fm1_ticks(), us, budget = HALF_FRAMES * 1000000u / FS;
    x0x_dbg.in_audio = 1;
#if FELUCCA_UAC
    uac_render_start();
#endif
    {
        uint32_t k;
        for (k = 0; k < HALF_FRAMES; k += REND_FRAMES) {
            wt_render(o + 2u * k, REND_FRAMES);
#if FELUCCA_UAC
            if (synth.p[P_USB_AUDIO]) uac_tap(o + 2u * k, REND_FRAMES);       /* the USB audio input: the same output (the USB AUDIO switch) */
#endif
        }
    }
    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    x0x_dbg.cpu_q8 = (x0x_dbg.cpu_q8 * 15u + (us * 256u) / budget) / 16u;
    audio_cpu_pct = (x0x_dbg.cpu_q8 * 100u) >> 8;
    wt_synth_load(&synth, us * 100u / budget);         /* too slow: the synth sheds unison lanes before the audio breaks up */
    if (us * 100u / budget > audio_peak_pct)
        audio_peak_pct = us * 100u / budget;
    x0x_dbg.halves++;
    x0x_dbg.last_us = us;
    if (us > x0x_dbg.max_us)
        x0x_dbg.max_us = us;
    x0x_dbg.in_audio = 0;
}

void fm1_alnk0_irq(void)                            /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        render_block(&abuf[half * HALF_WORDS]);
        fm1_audio_ack_half();
        if (fm1_audio_free_half() != half) {
#if X0X_DEBUG
            x0x_worst.late_in++;
#endif
            x0x_dbg.late++;                         /* the DMA moved on while we rendered */
            audio_xruns++;
        }
    }
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}

static void audio_silence(void)                     /* IRQs off (flash erase): the DMA would loop stale audio */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}

/* ------------------------------------------------------------ timer --- */
static uint8_t usb_due;
void fm1_timer5_irq(void)
{
    static uint32_t sub;
#if X0X_DEBUG
    uint32_t tt0 = fm1_ticks();
#endif
    fm1_timer5_ack();
    x0x_dbg.timer_irqs++;
    if (x0x_dbg.in_audio)
        x0x_dbg.nested++;
    fm1_input_tick();
    /* 2 kHz: all USB SIE traffic lives here. This ISR now nests into the audio render (so the key
     * matrix keeps its rhythm, below); USB shares the MIDI queues with the audio ISR, so a poll that
     * falls inside the render waits for the first tick outside it, as it did before the nesting. */
    if (sub % 5u == 0u) {
        usb_due = 1;
#if FELUCCA_UAC
        /* the USB audio stream cannot wait for the render (one packet per 1 ms frame, a render takes
         * up to ~11.6 ms): it runs nested too. It touches only EP4 (INDEX is set on every access) and
         * the consumer side of its ring, and usb_poll never runs nested, so the two never interleave */
        if (x0x_dbg.in_audio)
            uac_service();
#endif
    }
    if (usb_due && !x0x_dbg.in_audio) {
        usb_due = 0;
        usb_poll();
#if X0X_TRS
        uart_midi_poll();                           /* the TRS jack: same context, same queue */
#endif
    }
    if (++sub == 10u)
        sub = 0;
    {
        static uint32_t last, acc;
        uint32_t now = fm1_ticks();
        acc += now - last;
        last = now;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
#if X0X_DEBUG
    if (x0x_dbg.in_audio)
        dbg_tmr += fm1_ticks() - tt0;
#endif
}
extern void isr_timer5(void);

/* ------------------------------------------------------------- boot --- */
#define BOOTGUARD_MAGIC 0x42475244u
struct bootguard_s bootguard __attribute__((section(".noinit")));   /* declared in x0x.c (ota_commit) */

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 8, 240, &FONT_S, WT_PRODUCT " CRASH", C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(10, 40, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->pc);
    draw_text_box(10, 60, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->emu);
    draw_text_box(10, 84, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->dbg);
    draw_text_box(10, 102, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->rets);
    draw_text_box(10, 120, 220, &FONT_S, b, C_WHITE, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

static void enter_uboot(const char *why)
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 110, 240, &FONT_S, why, C_WHITE, 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                          /* intentional reset: not a failed boot */
    fm1_enter_uboot();
}

/* SAFE MODE: WT-1 crashed (or hung) twice while starting. Nothing that makes sound runs: no audio,
 * no engine, no UI; USB is on, so the web installer (WT-1, X0X or the stock firmware) and M-UPGRADE can
 * reach it. PLAY tries WT-1 again. The chip's own update mode stays the last resort (fm1_cstart). */
static void safe_main(void)
{
    char b[24];
    uint32_t t0, play;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 18, 240, &FONT_B, WT_PRODUCT " SAFE MODE", C_HI, 1);
    draw_text_box(0, 52, 240, &FONT_S, "IT CRASHED TWICE", C_WHITE, 1);
    draw_text_box(0, 72, 240, &FONT_S, "WHILE STARTING.", C_WHITE, 1);
    draw_text_box(0, 102, 240, &FONT_S, "NO SOUND. USB IS ON:", C_WHITE, 1);
    draw_text_box(0, 122, 240, &FONT_S, "REINSTALL FROM THE", C_WHITE, 1);
    draw_text_box(0, 142, 240, &FONT_S, "WEB INSTALLER.", C_WHITE, 1);
    draw_text_box(0, 172, 240, &FONT_S, "PLAY: TRY AGAIN", C_HI, 1);
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 208, 240, &FONT_S, b, C_GRAY, 1);
    }
    fm1_input_init();
    panel_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 1);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    play = 1u << panel.btn[B_PLAY];
    t0 = fm1_ms;
    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (bootguard.pending && fm1_ms - t0 > 10000u)
            bootguard.pending = 0;                  /* safe mode itself is up: not another failed boot */
        if ((fm1_in.buttons & play) && fm1_ms - t0 > 500u) {
            bootguard.failed = 0;                   /* try WT-1 again, from scratch */
            bootguard.pending = 0;
            draw_text_box(0, 172, 240, &FONT_S, "STARTING " WT_PRODUCT "...", C_HI, 1);
            usb_detach();
            fm1_delay_ms(30);
            fm1_reboot();
        }
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (flash_ok)
                ota_session();                      /* an install commits and resets; else back here */
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
    }
}

#if WT_DIAG    /* a diagnostic build (WT_DIAG=1 ./build-fw.sh): each boot stage is named on the screen, so a stop shows where */
#define STAGE(t) do { draw_text_box(0, 192, 240, &FONT_S, t, C_WHITE, 1); fm1_delay_ms(600); fm1_wdt_feed(); } while (0)
#else
#define STAGE(t) do { } while (0)
#endif

static void fm1_main(void)
{
    int32_t knob __attribute__((unused)) = 512 * 16;
    uint32_t last_frame = 0, k;
    {
        uint32_t f = irq_save();
        flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());   /* the expected 1 MiB part, else RAM only */
        irq_restore(f);
        if (flash_ok) {
            fl_plain_window_init();                 /* user sample slots read as plaintext through XIP */
            for (k = 0; k < SMP_USER_SLOTS; k++)
                smp_user_scan(k);
            ota_boot_cleanup();
        }
    }
    lcd_init();
    palette_set(5);
    draw_text_box(0, 96, 240, &FONT_M, WT_PRODUCT, C_HI, 1);
    draw_text_box(0, 134, 240, &FONT_S, "WAVETABLE SYNTH", C_GRAY, 1);
    if (x0x_dbg.magic != DBG_MAGIC) {
        memset(&x0x_dbg, 0, sizeof x0x_dbg);
        x0x_dbg.magic = DBG_MAGIC;
    }
    if (safe_mode)
        safe_main();                                /* never returns */
    if (fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != x0x_dbg.crash_seen) {
        char b[16];                                 /* a crash since the last boot: say so, a moment */
        x0x_dbg.crash_seen = fm1_crash.count;
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 176, 240, &FONT_S, "RESTARTED AFTER A CRASH", RGB(255, 80, 60), 1);
        draw_text_box(0, 196, 240, &FONT_S, b, C_GRAY, 1);
        fm1_wdt_feed();
        fm1_delay_ms(2500);
        fm1_wdt_feed();
    }
    x0x_dbg.boots++;
    x0x_dbg.max_us = 0;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    led_pos_init();
    plat_perf_init();
    draw_text_box(0, 168, 240, &FONT_S, "BUILDING WAVETABLES...", C_AMB, 1);
    {
        uint32_t t0 = fm1_ticks();
        if (wt_boot() != 0) {
            draw_text_box(0, 190, 240, &FONT_S, "TABLE BUILD FAILED", RGB(255, 80, 60), 1);
            for (;;) fm1_wdt_feed();                /* safe mode takes over after the boot guard trips */
        }
        x0x_dbg.stage = (fm1_ticks() - t0) / (FM1_TICKS_PER_US * 1000u);   /* ms the build took (console) */
    }
    STAGE("1 TABLES BUILT");
    wui_init();
    STAGE("2 SYNTH AND SCREENS READY");
    plat_cpu2_set(1);                               /* the second core, at power-on before the audio (as X0X does); if it does not answer: one core */
    wt_par_run = wt_par_run_fn;
    wt_par_wait = plat_cpu2_wait;
    wt_clock = plat_cycles;
    wt_midi_out = plat_midi_out;
    audio_init();
    STAGE("3 AUDIO RUNNING");
    usb_start();
#if X0X_TRS
    uart_midi_init();                               /* before TIMER5: it read-modify-writes port H too */
#endif
    /* above ALNK0 (3): the key / encoder / LED matrix is scanned one column per tick and must keep its rhythm */
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
#if WT_P37
    if ((fm1_in.buttons & FM1_OCT_BOTH) == FM1_OCT_BOTH)
#else
    if ((fm1_in.buttons & 3u) == 3u)
#endif
        panel_setup();                              /* OCT- + OCT+ held at power-on */
    STAGE("4 INTERRUPTS ON");
    fm1_delay_ms(300);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    STAGE("5 FIRST SCREEN");
    wui_frame();
    STAGE("6 ENTERING THE LOOP");

    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {     /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        {
            int32_t b = fm1_adc_read(FM1_ADC_BATT);     /* the battery: a slow average (plat_battery) */
            if (b > 0)
                batt_raw = batt_raw ? batt_raw + (b - batt_raw) / 32 : b;
        }
#if !WT_P37          /* (the P37's K1 turns the pages; its master level is the MASTER parameter, on a fader) */
        {
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                uint32_t k10;
                knob += (a * 16 - knob) / 8;
                k10 = (uint32_t)(knob / 16);
                master_q12 = (k10 * k10) >> 8;           /* 0 .. ~4096 */
            }
        }
#endif
        {   /* OCT- + OCT+ held 5 s: update mode (Felucca's, unchanged) */
            static uint32_t t0, shown;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both) {
                shown = 0;
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != shown) {
                    char d[24] = "UPDATE MODE IN  ...";
                    d[15] = (char)('0' + left);
                    lcd_fill(0, 220, 240, 20, C_BLACK);
                    draw_text_box(0, 222, 240, &FONT_S, d, C_HI, 1);
                    shown = left;
                }
            } else if (fm1_ms - t0 > 5000u) {
                enter_uboot("UBOOT");
            }
        }
        {   const uint8_t *fp; uint32_t fn;                  /* the editor's patch transfer (F0 7D 'W' 'T' ..), else the updater's handshake */
            static uint8_t sxo[480];
            if (ota_frame_get(&fp, &fn) && fn >= 4u && fp[0] == 0x7D && fp[1] == 'W' && fp[2] == 'T') {
                uint32_t rl = wui_sysex(fp + 3, fn - 3u, sxo);
                ota_frame_done();
                if (rl) ota_wire_send(sxo, rl);
            } else ota_service();                   /* M-UPGRADE handshake */
        }
        if (usb.ota_req) {
            usb.ota_req = 0;
            wq_put(WC_ALLOFF, 0, 0);
            if (flash_ok)
                ota_session();                      /* returns only if nothing was committed */
            lcd_fill(0, 0, 240, 240, C_BLACK);
            wui.dirty = 1;
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
        if (fm1_ms - last_frame >= 16u) {           /* ~60 frames/s at most */
            last_frame = fm1_ms;
            x0x_dbg.ui_frames++;
            wui_frame();
        }
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt;
    fm1_cpu1_halt();                                /* a second core left running by the reset would read
                                                     * the RAM cleared below (Jangada, fm1-x0x#10) */
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 4u) {                   /* safe mode failed too: the chip's own update mode */
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    p3 |= (bootguard.failed >= 2u) << 8;            /* two failed boots: safe mode (kept past the .bss clear) */
    fm1_irq_init();
    irq_stack_mark();                               /* before any interrupt: PERF's IRQ STACK */
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    for (s = _c1_load, d = _c1_start; d < _c1_end; s++, d++)
        *d = *s;                                    /* the second core's loop (fm1_cpu1.h) */
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    safe_mode = (uint8_t)(p3 >> 8);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}
