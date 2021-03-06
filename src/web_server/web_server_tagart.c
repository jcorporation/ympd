/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <libgen.h>

#include "../../dist/src/sds/sds.h"
#include "../../dist/src/mongoose/mongoose.h"
#include "../../dist/src/frozen/frozen.h"
#include "../sds_extras.h"
#include "../api.h"
#include "../list.h"
#include "mympd_config_defs.h"
#include "../utility.h"
#include "../log.h"
#include "../tiny_queue.h"
#include "../global.h"
#include "web_server_utility.h"
#include "web_server_tagart.h"

bool handle_tagart(struct mg_connection *nc, struct mg_http_message *hm, 
                   struct t_mg_user_data *mg_user_data) {
    //decode uri
    sds uri_decoded = sdsurldecode(sdsempty(), hm->uri.ptr, (int)hm->uri.len, 0);
    if (sdslen(uri_decoded) == 0) {
        MYMPD_LOG_ERROR("Failed to decode uri");
        serve_na_image(nc, hm);
        sdsfree(uri_decoded);
        return true;
    }
    if (validate_uri(uri_decoded) == false) {
        MYMPD_LOG_ERROR("Invalid URI: %s", uri_decoded);
        serve_na_image(nc, hm);
        sdsfree(uri_decoded);
        return true;
    }
    MYMPD_LOG_DEBUG("Handle tagart for uri \"%s\"", uri_decoded);
    //create absolute file
    sdsrange(uri_decoded, 8, -1);
    sds mediafile = sdscatfmt(sdsempty(), "%s/%s", mg_user_data->pics_document_root, uri_decoded);
    MYMPD_LOG_DEBUG("Absolut media_file: %s", mediafile);
    mediafile = find_image_file(mediafile);
    if (sdslen(mediafile) > 0) {
        sds mime_type = get_mime_type_by_ext(mediafile);
        MYMPD_LOG_DEBUG("Serving file %s (%s)", mediafile, mime_type);
        mg_http_serve_file(nc, hm, mediafile, mime_type, EXTRA_HEADERS_CACHE);
        sdsfree(mime_type);
    }
    else {
        MYMPD_LOG_DEBUG("No image for tag found");
        serve_na_image(nc, hm);
    }
    sdsfree(mediafile);
    sdsfree(uri_decoded);
    return true;
}
