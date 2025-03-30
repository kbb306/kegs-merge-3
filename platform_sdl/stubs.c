#include <stdint.h>
void x_hide_pointer() {}
void x_auto_repeat_on() {}
void x_show_alert(const char *s) {}
void change_display_mode() {}
void video_update_status_line() {}
void video_update_event_line() {}
void video_reset() {}
void sound_reset() {}
void end_screen() {}
void x_dialog_create_kegs_conf() {}
void float_bus() {}
void doc_handle_event() {}
void doc_read_c030() {}
void doc_read_c03c() {}
void doc_read_c03d() {}
void doc_write_c03c() {}
void doc_write_c03d() {}
void doc_show_ensoniq_state() {}
void show_a2_line_stuff() {}
void show_xcolor_array() {}
int g_screen_index = 0;
int g_audio_rate = 44100;
int g_audio_enable = 1;
int g_use_bw_hires = 0;
int g_use_shmem = 0;
int g_use_dhr140 = 0;
int g_status_refresh_needed = 0;
int g_full_refresh_needed = 0;
int g_a2_screen_buffer_changed = 0;
int g_num_recalc_snd_parms = 0;
int g_num_start_sounds = 0;
int g_num_doc_events = 0;
int g_num_snd_plays = 0;
int g_cycs_in_40col = 0;
int g_cycs_in_xredraw = 0;
int g_cycs_in_sound1 = 0;
int g_cycs_in_sound2 = 0;
int g_cycs_in_sound3 = 0;
int g_cycs_in_sound4 = 0;
int g_cycs_in_est_sound = 0;
int g_cycs_in_start_sound = 0;
int g_cycs_in_refresh_ximage = 0;
int g_cycs_in_refresh_line = 0;
int g_cycs_in_check_input = 0;
int g_refresh_bytes_xfer = 0;
int g_sim65816_running = 1;
void engine_init(int argc, char **argv) {}
void engine_execute(void) {}
void engine_shutdown(void) {}
int g_num_lines_prev_superhires = 0;
int g_num_lines_prev_superhires640 = 0;
int g_video_extra_check_inputs = 0;

int g_screen_redraw_skip_amt = 0;
int g_doc_vol = 0;
int g_a2_new_all_stat = 0;
int g_new_a2_stat_cur_line = 0;
int g_num_scan_osc = 0;

int g_cur_a2_stat = 0;
int g_a2vid_palette = 0;
int g_doc_saved_ctl = 0;
int g_fvoices = 0;

void change_a2vid_palette() {}
void sound_update() {}

//int a2_key_to_ascii[256]; // only if it’s NOT already in adb.c (check first)
const char rcsid_adb_h[] = "sdl";

void x_full_screen() {}
void video_show_debug_info() {}
void slow_mem_changed() {}

uint8_t doc_ram[65536];  // simulated DOC RAM

void change_border_color() {}
