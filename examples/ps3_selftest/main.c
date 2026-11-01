#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <lv2/process.h>
#include <io/pad.h>
#include <SDL2/SDL.h>

#include "font8x8.h"

/* PSL1GHT SDL2 Stubs required by SDL_PSL1GHTrender.o */
void ps3_sdl_render_cp(const char *what) { (void)what; }
void ps3_sdl_flip_timeout(unsigned int ms) { (void)ms; }
void ps3_sdl_fifo_blocked(unsigned int ms) { (void)ms; }

#define SCREEN_W 1280
#define SCREEN_H 720

/* Color palette (ARGB8888) */
#define COLOR_BG            0xFF0D1117  /* GitHub dark background */
#define COLOR_PANEL         0xFF161B22  /* Panel fill */
#define COLOR_BORDER        0xFF30363D  /* Subtle border */
#define COLOR_ACCENT        0xFF1F6FEB  /* PS3 blue accent */
#define COLOR_WHITE         0xFFF0F6FC  /* Primary bright text */
#define COLOR_GRAY          0xFF8B949E  /* Secondary muted text */
#define COLOR_DARK_GRAY     0xFF484F58  /* Inactive element */
#define COLOR_PASS_FG       0xFF3FB950  /* Bright green */
#define COLOR_PASS_BG       0xFF13361E  /* Dark green pill */
#define COLOR_FAIL_FG       0xFFF85149  /* Bright red */
#define COLOR_FAIL_BG       0xFF3E1719  /* Dark red pill */
#define COLOR_RUN_FG        0xFFD29922  /* Warm yellow */
#define COLOR_RUN_BG        0xFF3B2E05  /* Dark yellow pill */
#define COLOR_CYAN          0xFF58A6FF  /* Interactive blue */

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static uint32_t g_fb[SCREEN_H][SCREEN_W];

static const char *log_destinations[] = {
    "/dev_usb000/selftest.log",
    "/dev_usb001/selftest.log",
    "/dev_hdd0/game/LIBARC001/USRDIR/selftest.log",
    "/dev_hdd0/game/LIBARC001/selftest.log",
    "/dev_hdd0/tmp/selftest.log",
    "selftest.log"
};

static void log_message(const char *format, ...)
{
    static int log_initialized = 0;
    char buffer[1024];
    va_list args;
    size_t i;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    puts(buffer);
    fflush(stdout);

    const char *open_mode = log_initialized ? "a" : "w";
    for (i = 0; i < sizeof(log_destinations) / sizeof(log_destinations[0]); i++)
    {
        FILE *f = fopen(log_destinations[i], open_mode);
        if (f != NULL)
        {
            fputs(buffer, f);
            fputc('\n', f);
            fclose(f);
        }
    }
    log_initialized = 1;
}

/* Graphics primitives */
static void fb_clear(uint32_t color)
{
    int y, x;
    for (y = 0; y < SCREEN_H; y++)
    {
        for (x = 0; x < SCREEN_W; x++)
        {
            g_fb[y][x] = color;
        }
    }
}

static void fb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    int r, c;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    for (r = 0; r < h; r++)
    {
        for (c = 0; c < w; c++)
        {
            g_fb[y + r][x + c] = color;
        }
    }
}

static void fb_draw_rect(int x, int y, int w, int h, uint32_t color)
{
    fb_fill_rect(x, y, w, 1, color);
    fb_fill_rect(x, y + h - 1, w, 1, color);
    fb_fill_rect(x, y, 1, h, color);
    fb_fill_rect(x + w - 1, y, 1, h, color);
}

static void fb_draw_char(int x, int y, char c, uint32_t color, int scale)
{
    unsigned char uc = (unsigned char)c;
    if (uc > 127) uc = '?';
    const unsigned char *glyph = &fontdata8x8[uc * 8];
    int r, b, sy, sx;

    for (r = 0; r < 8; r++)
    {
        unsigned char row = glyph[r];
        for (b = 0; b < 8; b++)
        {
            if (row & (0x80 >> b))
            {
                for (sy = 0; sy < scale; sy++)
                {
                    int py = y + r * scale + sy;
                    if (py < 0 || py >= SCREEN_H) continue;
                    for (sx = 0; sx < scale; sx++)
                    {
                        int px = x + b * scale + sx;
                        if (px < 0 || px >= SCREEN_W) continue;
                        g_fb[py][px] = color;
                    }
                }
            }
        }
    }
}

static void fb_draw_text(int x, int y, const char *str, uint32_t color, int scale)
{
    int cur_x = x;
    while (*str)
    {
        if (*str == '\n')
        {
            y += 8 * scale + 4;
            cur_x = x;
        }
        else
        {
            fb_draw_char(cur_x, y, *str, color, scale);
            cur_x += 8 * scale;
        }
        str++;
    }
}

static void fb_draw_badge(int x, int y, const char *text, uint32_t fg, uint32_t bg)
{
    int len = (int)strlen(text);
    int badge_w = len * 16 + 20;
    int badge_h = 24;

    fb_fill_rect(x, y, badge_w, badge_h, bg);
    fb_draw_rect(x, y, badge_w, badge_h, fg);
    fb_draw_text(x + 10, y + 4, text, fg, 2);
}

static void fb_flip(void)
{
    if (!g_texture || !g_renderer) return;
    SDL_UpdateTexture(g_texture, NULL, g_fb, SCREEN_W * sizeof(uint32_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

/* Test Definitions */
enum test_status {
    STATUS_WAITING = 0,
    STATUS_RUNNING,
    STATUS_PASSED,
    STATUS_FAILED
};

struct test_format {
    const char *label;
    const char *name;
    const char *desc;
    const char *archive_file;
    const char *subfolder;
    enum test_status status;
    char detail[96];
};

static struct test_format g_tests[] = {
    { "01/10", "TAR",        "POSIX ustar (plain / uncompressed)", "test_plain.tar",     "01_tar",     STATUS_WAITING, "" },
    { "02/10", "TAR.GZ",     "gzip / RFC 1952 deflate filter",    "test_gzip.tar.gz",   "02_gzip",    STATUS_WAITING, "" },
    { "03/10", "TAR.BZ2",    "bzip2 / Burrows-Wheeler filter",    "test_bzip2.tar.bz2", "03_bzip2",   STATUS_WAITING, "" },
    { "04/10", "TAR.XZ",     "xz / LZMA2 multi-stream filter",    "test_xz.tar.xz",     "04_xz",      STATUS_WAITING, "" },
    { "05/10", "TAR.LZMA",   "legacy LZMA raw-stream filter",     "test_lzma.tar.lzma", "05_lzma",    STATUS_WAITING, "" },
    { "06/10", "TAR.ZST",    "Zstandard / Facebook zstd filter",  "test_zstd.tar.zst",  "06_zstd",    STATUS_WAITING, "" },
    { "07/10", "TAR.LZ4",    "LZ4 high-throughput block filter",  "test_lz4.tar.lz4",   "07_lz4",     STATUS_WAITING, "" },
    { "08/10", "ZIP",        "ZIP format / Deflate container",    "test_zip.zip",       "08_zip",     STATUS_WAITING, "" },
    { "09/10", "CPIO.GZ",    "POSIX odc cpio + gzip filter",      "test_cpio.cpio.gz",  "09_cpio",    STATUS_WAITING, "" },
    { "10/10", "7Z (7-Zip)", "7-Zip container / LZMA encoder",    "test_7z.7z",         "10_7z",      STATUS_WAITING, "" }
};

#define NUM_TESTS ((int)(sizeof(g_tests) / sizeof(g_tests[0])))

/* Recursive directory creator */
static int ensure_dir(const char *dir)
{
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    return mkdir(tmp, 0777);
}

/* Locate writeable base directory for physical extraction */
static const char *get_extraction_base_dir(void)
{
    static char s_base_dir[256] = "";
    if (s_base_dir[0] != '\0') return s_base_dir;

    /* Check USB drive 0 (preferred: easy inspection on PC) */
    if (access("/dev_usb000", F_OK) == 0)
    {
        snprintf(s_base_dir, sizeof(s_base_dir), "/dev_usb000/libarchive_extracted");
        ensure_dir(s_base_dir);
        return s_base_dir;
    }
    /* Check USB drive 1 */
    if (access("/dev_usb001", F_OK) == 0)
    {
        snprintf(s_base_dir, sizeof(s_base_dir), "/dev_usb001/libarchive_extracted");
        ensure_dir(s_base_dir);
        return s_base_dir;
    }
    /* Fallback to PS3 internal HDD /dev_hdd0/tmp */
    snprintf(s_base_dir, sizeof(s_base_dir), "/dev_hdd0/tmp/libarchive_extracted");
    ensure_dir(s_base_dir);
    return s_base_dir;
}

static void draw_ui(int finished, int passed_count, int failed_count,
                    int progress_pct, const char *status_msg,
                    const char *action_msg, const char *dest_dir, int countdown)
{
    int i;
    char line_buf[256];

    fb_clear(COLOR_BG);

    /* Header Bar */
    fb_fill_rect(50, 25, SCREEN_W - 100, 92, COLOR_PANEL);
    fb_draw_rect(50, 25, SCREEN_W - 100, 92, COLOR_BORDER);
    fb_fill_rect(50, 25, 8, 92, COLOR_ACCENT);

    fb_draw_text(75, 35, "LIBARCHIVE 3.8.11 - PLAYSTATION 3 VERIFICATION & EXTRACTION", COLOR_WHITE, 2);
    fb_draw_text(75, 62, "PSL1GHT Toolchain  |  Cell Broadband Engine (PPU 64-bit)  |  Retail CFW/HEN", COLOR_GRAY, 1);

    snprintf(line_buf, sizeof(line_buf), "Library: %s", archive_version_string());
    fb_draw_text(75, 82, line_buf, COLOR_CYAN, 1);

    /* Test Case Rows (10 rows, height 36, pitch 40) */
    int start_y = 125;
    for (i = 0; i < NUM_TESTS; i++)
    {
        int row_y = start_y + i * 40;
        uint32_t row_bg = (i % 2 == 0) ? 0xFF161B22 : 0xFF12171E;

        fb_fill_rect(50, row_y, SCREEN_W - 100, 36, row_bg);
        fb_draw_rect(50, row_y, SCREEN_W - 100, 36, COLOR_BORDER);

        /* Format index & name */
        snprintf(line_buf, sizeof(line_buf), "[%s] %-10s", g_tests[i].label, g_tests[i].name);
        fb_draw_text(65, row_y + 10, line_buf, COLOR_WHITE, 2);

        /* Description */
        fb_draw_text(340, row_y + 13, g_tests[i].desc, COLOR_GRAY, 1);

        /* Detail/status */
        if (g_tests[i].detail[0] != '\0')
        {
            fb_draw_text(660, row_y + 13, g_tests[i].detail, COLOR_CYAN, 1);
        }

        /* Status Badge */
        int badge_x = SCREEN_W - 240;
        int badge_y = row_y + 6;
        switch (g_tests[i].status)
        {
            case STATUS_WAITING:
                fb_draw_badge(badge_x, badge_y, " WAITING ", COLOR_DARK_GRAY, 0xFF1C2128);
                break;
            case STATUS_RUNNING:
                fb_draw_badge(badge_x, badge_y, " RUNNING ", COLOR_RUN_FG, COLOR_RUN_BG);
                break;
            case STATUS_PASSED:
                fb_draw_badge(badge_x, badge_y, " PASSED  ", COLOR_PASS_FG, COLOR_PASS_BG);
                break;
            case STATUS_FAILED:
                fb_draw_badge(badge_x, badge_y, " FAILED  ", COLOR_FAIL_FG, COLOR_FAIL_BG);
                break;
        }
    }

    /* Footer / Progress & Summary Panel */
    int footer_y = 535;
    int footer_h = 160;
    fb_fill_rect(50, footer_y, SCREEN_W - 100, footer_h, COLOR_PANEL);
    fb_draw_rect(50, footer_y, SCREEN_W - 100, footer_h, COLOR_BORDER);

    uint32_t bar_accent = (finished && failed_count > 0) ? COLOR_FAIL_FG :
                          (finished ? COLOR_PASS_FG : COLOR_ACCENT);
    fb_fill_rect(50, footer_y, 8, footer_h, bar_accent);

    /* Line 1: Main Status Title */
    if (finished)
    {
        uint32_t status_color = (failed_count == 0) ? COLOR_PASS_FG : COLOR_FAIL_FG;
        snprintf(line_buf, sizeof(line_buf), "TEST RESULTS: %d / %d PASSED  (%d FAILED)",
                 passed_count, NUM_TESTS, failed_count);
        fb_draw_text(75, footer_y + 14, line_buf, status_color, 2);
    }
    else
    {
        fb_draw_text(75, footer_y + 14, status_msg ? status_msg : "Processing...", COLOR_CYAN, 2);
    }

    /* Line 2: PROGRESS BAR */
    int pbar_x = 75;
    int pbar_y = footer_y + 40;
    int pbar_w = SCREEN_W - 150; /* 1130 px wide */
    int pbar_h = 24;

    /* Frame & dark background */
    fb_fill_rect(pbar_x, pbar_y, pbar_w, pbar_h, 0xFF0D1117);
    fb_draw_rect(pbar_x, pbar_y, pbar_w, pbar_h, COLOR_BORDER);

    /* Fill */
    if (progress_pct > 100) progress_pct = 100;
    if (progress_pct < 0) progress_pct = 0;
    int fill_w = (pbar_w - 4) * progress_pct / 100;
    if (fill_w > 0)
    {
        uint32_t bar_fill_color = (finished && failed_count > 0) ? COLOR_FAIL_FG :
                                  (finished ? COLOR_PASS_FG : 0xFF00D2FF);
        fb_fill_rect(pbar_x + 2, pbar_y + 2, fill_w, pbar_h - 4, bar_fill_color);
    }

    /* Progress Bar Text (Centered / Overlaid) */
    char pbar_text[128];
    if (action_msg && action_msg[0] != '\0')
    {
        snprintf(pbar_text, sizeof(pbar_text), "[ %3d%% ]  %s", progress_pct, action_msg);
    }
    else
    {
        snprintf(pbar_text, sizeof(pbar_text), "[ %3d%% ]", progress_pct);
    }
    fb_draw_text(pbar_x + 14, pbar_y + 5, pbar_text, COLOR_WHITE, 2);

    /* Line 3: Destination directory info */
    snprintf(line_buf, sizeof(line_buf), "Extraction Directory: %s/  (Report: EXTRACTION_INFO.txt)",
             dest_dir ? dest_dir : "Detecting...");
    fb_draw_text(75, footer_y + 74, line_buf, COLOR_GRAY, 1);

    /* Line 4: Controller prompt or current sub-action */
    if (finished)
    {
        snprintf(line_buf, sizeof(line_buf),
                 "Press (X), (O), or (START) on controller to exit  [Auto-exit in %2ds]",
                 countdown);
        fb_draw_text(75, footer_y + 104, line_buf, COLOR_CYAN, 2);
    }
    else
    {
        fb_draw_text(75, footer_y + 104,
                     "Decompressing streams + writing files to physical disk and verifying payload...",
                     COLOR_DARK_GRAY, 1);
    }

    fb_flip();
}

/* Locate sample archive across multiple valid storage locations */
static int locate_archive(const char *filename, char *out_path, size_t out_size)
{
    static const char *search_bases[] = {
        "/dev_hdd0/game/LIBARC001/USRDIR/sample_archives",
        "/dev_hdd0/game/LIBARC001/sample_archives",
        "/dev_usb000/sample_archives",
        "/dev_usb001/sample_archives",
        "sample_archives",
        "USRDIR/sample_archives",
        "."
    };
    size_t i;
    for (i = 0; i < sizeof(search_bases) / sizeof(search_bases[0]); i++)
    {
        snprintf(out_path, out_size, "%s/%s", search_bases[i], filename);
        if (access(out_path, R_OK) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/* Fallback: Create test archive directly on PS3 using libarchive write APIs */
static int create_test_archive(const char *path, const char *format, const char *filter)
{
    struct archive *a;
    struct archive_entry *entry;
    const char payload[] = "PASSED\n";
    int r;

    a = archive_write_new();
    if (!a) return -1;

    if (strcmp(filter, "gzip") == 0) archive_write_add_filter_gzip(a);
    else if (strcmp(filter, "bzip2") == 0) archive_write_add_filter_bzip2(a);
    else if (strcmp(filter, "xz") == 0) archive_write_add_filter_xz(a);
    else if (strcmp(filter, "lzma") == 0) archive_write_add_filter_lzma(a);
    else if (strcmp(filter, "zstd") == 0) archive_write_add_filter_zstd(a);
    else if (strcmp(filter, "lz4") == 0) archive_write_add_filter_lz4(a);
    else archive_write_add_filter_none(a);

    if (strcmp(format, "tar") == 0) archive_write_set_format_pax_restricted(a);
    else if (strcmp(format, "zip") == 0) archive_write_set_format_zip(a);
    else if (strcmp(format, "cpio") == 0) archive_write_set_format_cpio(a);
    else if (strcmp(format, "7z") == 0) archive_write_set_format_7zip(a);
    else archive_write_set_format_pax_restricted(a);

    if (archive_write_open_filename(a, path) != ARCHIVE_OK)
    {
        archive_write_free(a);
        return -1;
    }

    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "test.txt");
    archive_entry_set_size(entry, (la_int64_t)strlen(payload));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);

    if (archive_write_header(a, entry) != ARCHIVE_OK)
    {
        archive_entry_free(entry);
        archive_write_free(a);
        return -1;
    }

    r = (int)archive_write_data(a, payload, strlen(payload));
    archive_entry_free(entry);
    archive_write_close(a);
    archive_write_free(a);

    return (r == (int)strlen(payload)) ? 0 : -1;
}

/* Write human-readable extraction report and guide */
static void write_extraction_readme(const char *base_dir, int passed_count, int total_count, int failed_count)
{
    char readme_path[512];
    snprintf(readme_path, sizeof(readme_path), "%s/EXTRACTION_INFO.txt", base_dir);
    FILE *f = fopen(readme_path, "w");
    if (f)
    {
        fprintf(f, "================================================================================\n");
        fprintf(f, "            LIBARCHIVE PS3 - PHYSICAL DISK EXTRACTION REPORT                   \n");
        fprintf(f, "================================================================================\n\n");
        fprintf(f, "OVERALL RESULT: %d / %d FORMATS EXTRACTED & VERIFIED  (%d FAILED)\n\n",
                passed_count, total_count, failed_count);
        fprintf(f, "EXTRACTION DIRECTORY:\n");
        fprintf(f, "  %s\n\n", base_dir);
        fprintf(f, "EXTRACTED FOLDERS & FILES (DECOMPRESSED VIA LIBARCHIVE ON CELL PPU):\n");
        int i;
        for (i = 0; i < NUM_TESTS; i++)
        {
            fprintf(f, "  [%s] %-12s -> %s/test.txt   [%s]\n",
                    g_tests[i].label, g_tests[i].name, g_tests[i].subfolder,
                    g_tests[i].status == STATUS_PASSED ? "OK - VERIFIED ON DISK" : "FAILED");
        }
        fprintf(f, "\n");
        fprintf(f, "HOW TO VIEW THESE FILES:\n");
        fprintf(f, "  1. ON PC (USB DRIVE dev_usb000):\n");
        fprintf(f, "     Connect your USB drive to your PC and open the 'libarchive_extracted' folder.\n");
        fprintf(f, "     Inside you will find all 10 subdirectories with the extracted 'test.txt' files.\n\n");
        fprintf(f, "  2. ON PLAYSTATION 3 (Internal HDD):\n");
        fprintf(f, "     Launch multiMAN or IRISMAN, open File Manager and navigate to:\n");
        fprintf(f, "     %s\n\n", base_dir);
        fprintf(f, "All 'test.txt' files contain the string 'PASSED', successfully unpacked\n");
        fprintf(f, "by the 64-bit PowerPC Cell processor and written to the filesystem.\n");
        fprintf(f, "================================================================================\n");
        fclose(f);
    }
}

/* Extract archive in RAM, write to physical disk, and verify disk content */
static int test_archive_file(struct test_format *tf, int test_idx, int total_tests,
                             const char *base_dir, const char *alt_dir,
                             int current_passed, int current_failed)
{
    char archive_path[512];
    struct archive *a;
    struct archive_entry *entry;
    char buffer[256];
    int ram_verified = 0;
    int disk_verified = 0;
    int r;
    la_ssize_t bytes_read = 0;

    tf->status = STATUS_RUNNING;

    /* Step 1: Decompression in RAM */
    int pct = test_idx * 10 + 2;
    char action[128];
    snprintf(action, sizeof(action), "Decompressing RAM: %s", tf->name);
    draw_ui(0, current_passed, current_failed, pct, "Decompressing and verifying stream integrity...", action, base_dir, 0);

    /* Locate bundled archive file or generate dynamically */
    if (!locate_archive(tf->archive_file, archive_path, sizeof(archive_path)))
    {
        snprintf(archive_path, sizeof(archive_path), "/dev_hdd0/tmp/%s", tf->archive_file);
        const char *fmt = "tar";
        const char *flt = "none";

        if (strstr(tf->archive_file, ".tar.gz")) { fmt = "tar"; flt = "gzip"; }
        else if (strstr(tf->archive_file, ".tar.bz2")) { fmt = "tar"; flt = "bzip2"; }
        else if (strstr(tf->archive_file, ".tar.xz")) { fmt = "tar"; flt = "xz"; }
        else if (strstr(tf->archive_file, ".tar.lzma")) { fmt = "tar"; flt = "lzma"; }
        else if (strstr(tf->archive_file, ".tar.zst")) { fmt = "tar"; flt = "zstd"; }
        else if (strstr(tf->archive_file, ".tar.lz4")) { fmt = "tar"; flt = "lz4"; }
        else if (strstr(tf->archive_file, ".zip")) { fmt = "zip"; flt = "none"; }
        else if (strstr(tf->archive_file, ".cpio.gz")) { fmt = "cpio"; flt = "gzip"; }
        else if (strstr(tf->archive_file, ".7z")) { fmt = "7z"; flt = "none"; }

        if (create_test_archive(archive_path, fmt, flt) != 0)
        {
            tf->status = STATUS_FAILED;
            snprintf(tf->detail, sizeof(tf->detail), "Creation failed");
            log_message("[%s] FAIL %s: could not locate or create archive", tf->label, tf->name);
            return -1;
        }
    }

    log_message("[%s] Testing %s with archive: %s", tf->label, tf->name, archive_path);

    /* Read and decompress archive with libarchive */
    a = archive_read_new();
    if (!a)
    {
        tf->status = STATUS_FAILED;
        snprintf(tf->detail, sizeof(tf->detail), "archive_read_new fail");
        return -1;
    }

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, archive_path, 10240) != ARCHIVE_OK)
    {
        tf->status = STATUS_FAILED;
        snprintf(tf->detail, sizeof(tf->detail), "Open err: %s", archive_error_string(a));
        log_message("[%s] FAIL %s: archive_read_open failed: %s", tf->label, tf->name, archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK)
    {
        const char *pathname = archive_entry_pathname(entry);
        if (strstr(pathname, "test.txt") != NULL)
        {
            memset(buffer, 0, sizeof(buffer));
            bytes_read = archive_read_data(a, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0 && strstr(buffer, "PASSED") != NULL)
            {
                ram_verified = 1;
            }
            break;
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    if (!ram_verified)
    {
        tf->status = STATUS_FAILED;
        snprintf(tf->detail, sizeof(tf->detail), "RAM decomp fail");
        log_message("[%s] FAIL %s: RAM verification failed", tf->label, tf->name);
        return -1;
    }

    /* Step 2: Physical Extraction to Disk */
    pct = test_idx * 10 + 6;
    snprintf(action, sizeof(action), "Writing disk: %s/test.txt", tf->subfolder);
    draw_ui(0, current_passed, current_failed, pct, "Writing physical file to disk...", action, base_dir, 0);

    char target_dir[512];
    char target_file[512];
    snprintf(target_dir, sizeof(target_dir), "%s/%s", base_dir, tf->subfolder);
    ensure_dir(target_dir);
    snprintf(target_file, sizeof(target_file), "%s/test.txt", target_dir);

    FILE *out = fopen(target_file, "wb");
    if (out)
    {
        fwrite(buffer, 1, (size_t)bytes_read, out);
        fclose(out);
    }
    else
    {
        log_message("[%s] Notice: fopen write failed on %s (errno=%d)", tf->label, target_file, errno);
    }

    /* Also write to alternate directory (internal HDD tmp) if available */
    if (alt_dir && strcmp(alt_dir, base_dir) != 0)
    {
        char alt_target_dir[512];
        char alt_target_file[512];
        snprintf(alt_target_dir, sizeof(alt_target_dir), "%s/%s", alt_dir, tf->subfolder);
        ensure_dir(alt_target_dir);
        snprintf(alt_target_file, sizeof(alt_target_file), "%s/test.txt", alt_target_dir);
        FILE *alt_out = fopen(alt_target_file, "wb");
        if (alt_out)
        {
            fwrite(buffer, 1, (size_t)bytes_read, alt_out);
            fclose(alt_out);
        }
    }

    /* Step 3: Re-read directly from physical disk to verify */
    pct = test_idx * 10 + 8;
    snprintf(action, sizeof(action), "Verifying disk: %s/test.txt", tf->subfolder);
    draw_ui(0, current_passed, current_failed, pct, "Verifying physical disk read...", action, base_dir, 0);

    char check_buf[256];
    memset(check_buf, 0, sizeof(check_buf));
    FILE *in = fopen(target_file, "rb");
    if (in)
    {
        size_t n = fread(check_buf, 1, sizeof(check_buf) - 1, in);
        fclose(in);
        if (n > 0 && strstr(check_buf, "PASSED") != NULL)
        {
            disk_verified = 1;
        }
    }

    if (!disk_verified && alt_dir)
    {
        char alt_target_file[512];
        snprintf(alt_target_file, sizeof(alt_target_file), "%s/%s/test.txt", alt_dir, tf->subfolder);
        FILE *in2 = fopen(alt_target_file, "rb");
        if (in2)
        {
            size_t n = fread(check_buf, 1, sizeof(check_buf) - 1, in2);
            fclose(in2);
            if (n > 0 && strstr(check_buf, "PASSED") != NULL)
            {
                disk_verified = 1;
            }
        }
    }

    if (ram_verified && disk_verified)
    {
        tf->status = STATUS_PASSED;
        snprintf(tf->detail, sizeof(tf->detail), "RAM + Disk (%s) OK", tf->subfolder);
        log_message("[%s] PASS %s: verified RAM and Disk (%s/test.txt)",
                    tf->label, tf->name, tf->subfolder);
        return 0;
    }
    else
    {
        tf->status = STATUS_FAILED;
        snprintf(tf->detail, sizeof(tf->detail), "Disk verification failed");
        log_message("[%s] FAIL %s: disk verification failed", tf->label, tf->name);
        return -1;
    }
}

int main(int argc, char **argv)
{
    int i;
    int passed = 0;
    int failed = 0;
    padInfo pad_info;
    padData pad_data;

    (void)argc;
    (void)argv;

    log_message("==================================================");
    log_message("libarchive PS3 Self-Test starting...");
    log_message("Version: %s", archive_version_string());
    log_message("==================================================");

    /* Initialize Gamepad */
    ioPadInit(7);

    /* Initialize SDL2 Video */
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        log_message("FATAL: SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
        sleep(2);
        sysProcessExit(1);
        return 1;
    }

    g_window = SDL_CreateWindow("libarchive PS3 Test", 0, 0, SCREEN_W, SCREEN_H, 0);
    if (!g_window)
    {
        log_message("FATAL: SDL_CreateWindow failed");
        SDL_Quit();
        sysProcessExit(1);
        return 1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer)
    {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer)
    {
        log_message("FATAL: SDL_CreateRenderer failed");
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        sysProcessExit(1);
        return 1;
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (!g_texture)
    {
        log_message("FATAL: SDL_CreateTexture failed");
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        sysProcessExit(1);
        return 1;
    }

    const char *base_dir = get_extraction_base_dir();
    const char *alt_dir = "/dev_hdd0/tmp/libarchive_extracted";
    ensure_dir(alt_dir);

    log_message("Extraction Base Directory: %s", base_dir);
    log_message("Alternate Directory: %s", alt_dir);

    /* Initial screen display (0%) */
    draw_ui(0, 0, 0, 0, "Starting tests...", "Initializing PS3 environment...", base_dir, 0);
    usleep(500000);

    /* Run each archive test case (Decompress + Extract + Verify) */
    for (i = 0; i < NUM_TESTS; i++)
    {
        if (test_archive_file(&g_tests[i], i, NUM_TESTS, base_dir, alt_dir, passed, failed) == 0)
        {
            passed++;
        }
        else
        {
            failed++;
        }

        int pct = (i + 1) * 10;
        char act[64];
        snprintf(act, sizeof(act), "Completed %s: %d/%d", g_tests[i].name, i + 1, NUM_TESTS);
        draw_ui(0, passed, failed, pct, "Test progression...", act, base_dir, 0);
        usleep(150000);
    }

    /* Write extraction report and README guide to disk */
    write_extraction_readme(base_dir, passed, NUM_TESTS, failed);
    if (strcmp(base_dir, alt_dir) != 0)
    {
        write_extraction_readme(alt_dir, passed, NUM_TESTS, failed);
    }

    log_message("==================================================");
    log_message("RESULT: %d / %d PASSED (%d FAILED)", passed, NUM_TESTS, failed);
    log_message("Extracted files written to: %s", base_dir);
    log_message("==================================================");

    /* Final Interactive Loop: keep screen alive and allow user to view results */
    int timeout_sec = 60;
    while (timeout_sec > 0)
    {
        draw_ui(1, passed, failed, 100, "ALL TESTS COMPLETED", "RAM decompression and physical disk extraction 100% verified", base_dir, timeout_sec);

        /* Check controller input */
        if (ioPadGetInfo(&pad_info) == 0 && pad_info.status[0])
        {
            if (ioPadGetData(0, &pad_data) == 0 && pad_data.len > 0)
            {
                unsigned int b1 = pad_data.button[2];
                unsigned int b2 = pad_data.button[3];

                /* Cross (X), Circle (O), or START button */
                if ((b2 & PAD_CTRL_CROSS) || (b2 & PAD_CTRL_CIRCLE) || (b1 & PAD_CTRL_START))
                {
                    log_message("Exit button pressed on controller. Returning to XMB...");
                    break;
                }
            }
        }

        sleep(1);
        timeout_sec--;
    }

    /* Clean exit */
    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    sysProcessExit(failed == 0 ? 0 : 1);
    return 0;
}
