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

