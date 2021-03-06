/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include <mpd/client.h>

#include "../../dist/src/sds/sds.h"
#include "../dist/src/rax/rax.h"
#include "../log.h"
#include "../list.h"
#include "../tiny_queue.h"
#include "../api.h"
#include "../global.h"
#include "mympd_config_defs.h"
#include "../mympd_state.h"
#include "../utility.h"
#include "../covercache.h"
#include "mympd_api_utility.h"
#include "mympd_api_timer.h"
#include "mympd_api_timer_handlers.h"

//timer_id 1
void timer_handler_covercache(struct t_timer_definition *definition, void *user_data) {
    MYMPD_LOG_INFO("Start timer_handler_covercache");
    (void) definition;
    struct t_mympd_state *mympd_state = (struct t_mympd_state *) user_data;
    clear_covercache(mympd_state->config->workdir, mympd_state->covercache_keep_days);
}

//timer_id 2
void timer_handler_smartpls_update(struct t_timer_definition *definition, void *user_data) {
    MYMPD_LOG_INFO("Start timer_handler_smartpls_update");
    (void) definition;
    (void) user_data;
    t_work_request *request = create_request(-1, 0, MYMPD_API_SMARTPLS_UPDATE_ALL, "MYMPD_API_SMARTPLS_UPDATE_ALL", "");
    request->data = sdscat(request->data, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"MYMPD_API_SMARTPLS_UPDATE_ALL\",\"params\":{\"force\":false}}");
    tiny_queue_push(mympd_api_queue, request, 0);
}

void timer_handler_select(struct t_timer_definition *definition, void *user_data) {
    MYMPD_LOG_INFO("Start timer_handler_select for timer \"%s\"", definition->name);
    if (strcmp(definition->action, "player") == 0 && strcmp(definition->subaction, "stopplay") == 0) {
        t_work_request *request = create_request(-1, 0, MYMPD_API_PLAYER_STOP, "MYMPD_API_PLAYER_STOP", "");
        request->data = sdscat(request->data, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"MYMPD_API_PLAYER_STOP\",\"params\":{}}");
        tiny_queue_push(mympd_api_queue, request, 0);
    }
    else if (strcmp(definition->action, "player") == 0 && strcmp(definition->subaction, "startplay") == 0) {
        t_work_request *request = create_request(-1, 0, MYMPD_API_TIMER_STARTPLAY, "MYMPD_API_TIMER_STARTPLAY", "");
        request->data = sdscat(request->data, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"MYMPD_API_TIMER_STARTPLAY\",\"params\":{");
        request->data = tojson_long(request->data, "volume", definition->volume, true);
        request->data = tojson_char(request->data, "playlist", definition->playlist, true);
        request->data = tojson_long(request->data, "jukeboxMode", definition->jukebox_mode, false);
        request->data = sdscat(request->data, "}}");
        tiny_queue_push(mympd_api_queue, request, 0);
    }
    else if (strcmp(definition->action, "script") == 0) {
        t_work_request *request = create_request(-1, 0, MYMPD_API_SCRIPT_EXECUTE, "MYMPD_API_SCRIPT_EXECUTE", "");
        request->data = sdscat(request->data, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"MYMPD_API_SCRIPT_EXECUTE\",\"params\":{");
        request->data = tojson_char(request->data, "script", definition->subaction, true);
        request->data = sdscat(request->data, "arguments: {");
        struct list_node *argument = definition->arguments.head;
        int i = 0;
        while (argument != NULL) {
            if (i++) {
                request->data = sdscatlen(request->data, ",", 1);
            }
            request->data = tojson_char(request->data, argument->key, argument->value_p, false);
            argument = argument->next;
        }
        request->data = sdscat(request->data, "}}}");
        tiny_queue_push(mympd_api_queue, request, 0);
    }
    else {
        MYMPD_LOG_ERROR("Unknown script action: %s - %s", definition->action, definition->subaction);
    }
    (void) user_data;
}
