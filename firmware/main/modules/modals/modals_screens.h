#pragma once
#include <stdio.h>

#include "modals_module.h"

void modals_screens_default_list_options_cb(modal_get_user_selection_t* ctx);
void modals_screens_show_info(char* head, char* body, size_t time_ms);
void modals_screens_list_y_n_options_cb(modal_get_user_selection_t* ctx);
void modals_screens_default_list_radio_options_cb(
    modal_get_radio_selection_t* ctx);
void modals_screens_show_banner(char* text);