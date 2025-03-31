#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <math.h>
#include <naomi/video.h>
#include <naomi/audio.h>
#include <naomi/maple.h>
#include <naomi/eeprom.h>
#include <naomi/thread.h>
#include <naomi/romfs.h>
#include <naomi/timer.h>
#include <naomi/system.h>
#include <xmp.h>

#define BUFSIZE 8192
#define SAMPLERATE 44100

typedef struct
{
    char filename[1024];
    volatile int exit;
    volatile int error;
    uint32_t thread;
} audiothread_instructions_t;

void *audiothread_xmp(void *param)
{
    audiothread_instructions_t *instructions = (audiothread_instructions_t *)param;

    xmp_context ctx = xmp_create_context();

    if (xmp_load_module(ctx, instructions->filename) < 0)
    {
        instructions->error = 1;
        return 0;
    }
    else if (xmp_start_player(ctx, SAMPLERATE, 0) != 0)
    {
        instructions->error = 2;
        xmp_release_module(ctx);
    }
    else
    {
        struct xmp_module_info mi;
        struct xmp_frame_info fi;

        xmp_get_module_info(ctx, &mi);
        audio_register_ringbuffer(AUDIO_FORMAT_16BIT, SAMPLERATE, BUFSIZE);

        while (xmp_play_frame(ctx) == 0 && instructions->exit == 0)
        {
            xmp_get_frame_info(ctx, &fi);
            int numsamples = fi.buffer_size / 4;
            uint32_t *samples = (uint32_t *)fi.buffer;

            while (numsamples > 0)
            {
                int actual_written = audio_write_stereo_data(samples, numsamples);
                if (actual_written < 0)
                {
                    instructions->error = 3;
                    break;
                }
                if (actual_written < numsamples)
                {
                    numsamples -= actual_written;
                    samples += actual_written;

                    // Sleep for the time it takes to play half our buffer so we can wake up and
                    // fill it again.
                    thread_sleep((int)(1000000.0 * (((float)BUFSIZE / 4.0) / (float)SAMPLERATE)));
                }
                else
                {
                    numsamples = 0;
                }
            }
        }

        audio_unregister_ringbuffer();

        xmp_end_player(ctx);
        xmp_release_module(ctx);
    }

    xmp_free_context(ctx);
    return 0;
}

audiothread_instructions_t * play(char *filename)
{
    audiothread_instructions_t *inst = malloc(sizeof(audiothread_instructions_t));
    memset(inst, 0, sizeof(audiothread_instructions_t));
    strcpy(inst->filename, filename);
    inst->thread = thread_create("audio", &audiothread_xmp, inst);
    thread_priority(inst->thread, 1);
    thread_start(inst->thread);
    return inst;
}

void stop(audiothread_instructions_t *inst)
{
    inst->exit = 1;
    thread_join(inst->thread);
    thread_destroy(inst->thread);
    free(inst);
}

typedef struct
{
    void *buffer;
    font_t *font;
} fontface_t;

fontface_t *load(char *filename, int size)
{
    FILE *fp = fopen(filename, "rb");
    fseek(fp, 0L, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    void *buffer = malloc(sz);
    if (!buffer) {
        return NULL;
    }

    fread(buffer, sz, 1, fp);
    fclose(fp);
    font_t *font = font_add(buffer, sz);
    if (!font) {
        free(buffer);
        return NULL;
    }

    font_set_size(font, size);
    
    fontface_t *ff = malloc(sizeof(fontface_t));
    if (!ff) {
        font_discard(font);
        free(buffer);
        return NULL;
    }

    ff->buffer = buffer;
    ff->font = font;
    return ff;
}

void unload(fontface_t *ff) {
    if (ff) {
        if (ff->font) {
            font_discard(ff->font);
        }
        if (ff->buffer) {
            free(ff->buffer);
        }

        free(ff);
    }
}

void main()
{
    // Get settings so we know how many controls to read.
    eeprom_t settings;
    eeprom_read(&settings);

    // Initialize some crappy video.
    video_init(VIDEO_COLOR_1555);
    video_set_background_color(rgb(0, 0, 0));

    // Initialize the ROMFS.
    romfs_init_default();

    // Initialize audio system.
    audio_init();

    // Load font.
    fontface_t *ff = load("rom://amiga.ttf", 16);

    // Kick off audio.
    audiothread_instructions_t *instructions = play("rom://tracktracking.xm");

    // Flag colors.
    color_t colors[5] = {
        rgb(91, 206, 250),
        rgb(245, 169, 184),
        rgb(255, 255, 255),
        rgb(245, 169, 184),
        rgb(91, 206, 250),
    };

    // Scroller message.
    char *message = "Trans rights are human rights. We're not going anywhere.";

    // Our scroller position.
    int pos = 0;

    // Calculate the size of the screen.
    while ( 1 )
    {
        // Speed is different on real hardware compared to Demul, so timer-based instead of code delay based.
        int timer = timer_start(50000);

        // Grab inputs.
        maple_poll_buttons();
        jvs_buttons_t pressed = maple_buttons_pressed();

        // Exit to system menu on test pressed.
        if (pressed.test || pressed.psw1)
        {
            enter_test_mode();
        }

        // Draw flag.
        if (settings.system.monitor_orientation == MONITOR_ORIENTATION_VERTICAL) {
            // Draw vertically.
            int bar_width = video_width() / 5;
            int bar_height = video_height();

            for (int i = 0; i < 5; i++) {
                video_fill_box(bar_width * i, 0, (bar_width * (i + 1) - 1), bar_height - 1, colors[i]);
            }
        } else {
            // Draw horizontally.
            int bar_width = video_width();
            int bar_height = video_height() / 5;

            for (int i = 0; i < 5; i++) {
                video_fill_box(0, bar_height * i, bar_width - 1, (bar_height * (i + 1)) - 1, colors[i]);
            }
        }

        // Display scroller text.
        int ycenter = (video_height() / 2) - 8;
        for (int x = 0; x < strlen(message); x++) {
            int xpos = video_width() + (x * 12) - (pos * 2);
            int ypos = ycenter + (int)(35.0 * sin((double)xpos / 50.0));
            video_draw_character(xpos, ypos, ff->font, rgb(127, 127, 127), message[x]);
        }

        // Wait for vblank and draw it!
        video_display_on_vblank();

        // Designed around 20FPS.
        while (timer_left(timer) > 0) { ; }
        timer_stop(timer);

        // Bump the position (20x a second for 60Hz).
        pos += 3;
        if ((pos * 2) > (video_width() + 900)) {
            pos = 0;
        }
    }

    // Never getting here, but clean up anyway.
    stop(instructions);
    unload(ff);
}

void test()
{
    video_init(VIDEO_COLOR_1555);

    int width = video_width() / 2;
    int height = video_height() / 2;

    while ( 1 )
    {
        maple_poll_buttons();
        jvs_buttons_t pressed = maple_buttons_pressed();

        // Exit back to system menu on test pressed.
        if (pressed.test || pressed.psw1)
        {
            enter_test_mode();
        }

        video_fill_screen(rgb(48, 48, 48));
        video_draw_debug_text(width - (8 * 4), height - 16, rgb(255, 255, 255), "TDOV 2025");
        video_draw_debug_text(width - (8 * 10), height + 8, rgb(255, 255, 255), "code by dragonminded");
        video_draw_debug_text(width - (8 * 12), height + 16, rgb(255, 255, 255), "music by dubmood&zabutom");
        video_draw_debug_text(width - (8 * 15), height + 24, rgb(255, 255, 255), "donate to a trans charity today");
        video_display_on_vblank();
    }
}
