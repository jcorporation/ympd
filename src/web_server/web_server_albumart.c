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
#include "../covercache.h"
#include "web_server_utility.h"
#include "web_server_albumart.h"

//optional includes
#ifdef ENABLE_LIBID3TAG
    #include <id3tag.h>
#endif

#ifdef ENABLE_FLAC
    #include <FLAC/metadata.h>
#endif

//privat definitions
static bool handle_coverextract(struct mg_connection *nc, struct t_config *config, const char *uri, const char *media_file, bool covercache);
static bool handle_coverextract_id3(struct t_config *config, const char *uri, const char *media_file, sds *binary, bool covercache);
static bool handle_coverextract_flac(struct t_config *config, const char *uri, const char *media_file, sds *binary, bool is_ogg, bool covercache);

//public functions
void send_albumart(struct mg_connection *nc, sds data, sds binary) {
    char *p_charbuf1 = NULL;

    int je = json_scanf(data, sdslen(data), "{result: {mime_type:%Q}}", &p_charbuf1);
    if (je == 1) {
        MYMPD_LOG_DEBUG("Serving file from memory (%s - %u bytes)", p_charbuf1, sdslen(binary));
        sds header = sdscatfmt(sdsempty(), "Content-Type: %s\r\n", p_charbuf1);
        header = sdscat(header, EXTRA_HEADERS_CACHE);
        http_send_header_ok(nc, sdslen(binary), header);
        mg_send(nc, binary, sdslen(binary));
        sdsfree(header);
    }
    else {
        //create dummy http message and serve not available image
        struct mg_http_message hm;
        populate_dummy_hm(&hm);
        serve_na_image(nc, &hm);
    }
    FREE_PTR(p_charbuf1);
}

//returns true if an image is served
//returns false if waiting for mpd_client to handle request
bool handle_albumart(struct mg_connection *nc, struct mg_http_message *hm, 
                     struct t_mg_user_data *mg_user_data, struct t_config *config, 
                     long long conn_id)
{
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
    MYMPD_LOG_DEBUG("Handle albumart for uri \"%s\"", uri_decoded);
    //try image in /pics folder, if uri contains ://
    if (is_streamuri(uri_decoded) == true) {
        char *name = strstr(uri_decoded, "://");
        if (strlen(name) < 4) {
            MYMPD_LOG_ERROR("Uri to short");
            serve_na_image(nc, hm);
            sdsfree(uri_decoded);
            return true;
        }
        name += 3;
        uri_to_filename(name);
        sds coverfile = sdscatfmt(sdsempty(), "%s/pics/%s", config->workdir, name);
        MYMPD_LOG_DEBUG("Check for stream cover %s", coverfile);
        coverfile = find_image_file(coverfile);
        
        if (sdslen(coverfile) > 0) {
            sds mime_type = get_mime_type_by_ext(coverfile);
            MYMPD_LOG_DEBUG("Serving file %s (%s)", coverfile, mime_type);
            mg_http_serve_file(nc, hm, coverfile, mime_type, EXTRA_HEADERS_CACHE);
            sdsfree(mime_type);
        }
        else {
            serve_stream_image(nc, hm);
        }
        sdsfree(coverfile);
        sdsfree(uri_decoded);
        return true;
    }
    //remove /albumart/
    sdsrange(uri_decoded, 10, -1);
    
    //check covercache
    if (mg_user_data->covercache == true) {
        sds filename = sdsdup(uri_decoded);
        uri_to_filename(filename);
        sds covercachefile = sdscatfmt(sdsempty(), "%s/covercache/%s", config->workdir, filename);
        sdsfree(filename);
        covercachefile = find_image_file(covercachefile);
        if (sdslen(covercachefile) > 0) {
            sds mime_type = get_mime_type_by_ext(covercachefile);
            MYMPD_LOG_DEBUG("Serving file %s (%s)", covercachefile, mime_type);
            mg_http_serve_file(nc, hm, covercachefile, mime_type, EXTRA_HEADERS_CACHE);
            sdsfree(uri_decoded);
            sdsfree(covercachefile);
            sdsfree(mime_type);
            return true;
        }

        MYMPD_LOG_DEBUG("No covercache file found");
        sdsfree(covercachefile);
    }
    
    //create absolute file
    sds mediafile = sdscatfmt(sdsempty(), "%s/%s", mg_user_data->music_directory, uri_decoded);
    MYMPD_LOG_DEBUG("Absolut media_file: %s", mediafile);
    
    if (mg_user_data->feat_library == true && 
        access(mediafile, F_OK) == 0) /* Flawfinder: ignore */
    {
        //try image in folder under music_directory
        if (mg_user_data->coverimage_names_len > 0) {
            sds path = sdsdup(uri_decoded);
            dirname(path);
            for (int j = 0; j < mg_user_data->coverimage_names_len; j++) {
                sds coverfile = sdscatfmt(sdsempty(), "%s/%s/%s", mg_user_data->music_directory, path, mg_user_data->coverimage_names[j]);
                if (strchr(mg_user_data->coverimage_names[j], '.') == NULL) {
                    //basename, try extensions
                    coverfile = find_image_file(coverfile);
                }
                if (sdslen(coverfile) > 0 && access(coverfile, F_OK ) == 0) { /* Flawfinder: ignore */
                    sds mime_type = get_mime_type_by_ext(coverfile);
                    MYMPD_LOG_DEBUG("Serving file %s (%s)", coverfile, mime_type);
                    mg_http_serve_file(nc, hm, coverfile, mime_type, EXTRA_HEADERS_CACHE);
                    sdsfree(uri_decoded);
                    sdsfree(coverfile);
                    sdsfree(mediafile);
                    sdsfree(mime_type);
                    sdsfree(path); 
                    return true;
                }
                sdsfree(coverfile);
            }
            MYMPD_LOG_DEBUG("No cover file found in music directory");
            sdsfree(path);
        }

        //try to extract albumart from media file
        bool rc = handle_coverextract(nc, config, uri_decoded, mediafile, mg_user_data->covercache);
        if (rc == true) {
            sdsfree(uri_decoded);
            sdsfree(mediafile);
            return true;
        }
    }
    sdsfree(mediafile);

    //ask mpd
    if (mg_user_data->feat_mpd_albumart == true) {
        MYMPD_LOG_DEBUG("Sending getalbumart to mpd_client_queue");
        t_work_request *request = create_request(conn_id, 0, MYMPD_API_ALBUMART, "MYMPD_API_ALBUMART", "");
        request->data = sdscat(request->data, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"MYMPD_API_ALBUMART\",\"params\":{");
        request->data = tojson_char(request->data, "uri", uri_decoded, false);
        request->data = sdscat(request->data, "}}");
        tiny_queue_push(mympd_api_queue, request, 0);
        sdsfree(uri_decoded);
        return false;
    }

    MYMPD_LOG_INFO("No coverimage found for \"%s\"", uri_decoded);
    sdsfree(uri_decoded);
    serve_na_image(nc, hm);
    return true;
}

//privat functions
static bool handle_coverextract(struct mg_connection *nc, struct t_config *config, 
                                const char *uri, const char *media_file, bool covercache)
{
    bool rc = false;
    sds mime_type_media_file = get_mime_type_by_ext(media_file);
    MYMPD_LOG_DEBUG("Handle coverextract for uri \"%s\"", uri);
    MYMPD_LOG_DEBUG("Mimetype of %s is %s", media_file, mime_type_media_file);
    sds binary = sdsempty();
    if (strcmp(mime_type_media_file, "audio/mpeg") == 0) {
        rc = handle_coverextract_id3(config, uri, media_file, &binary, covercache);
    }
    else if (strcmp(mime_type_media_file, "audio/ogg") == 0) {
        rc = handle_coverextract_flac(config, uri, media_file, &binary, true, covercache);
    }
    else if (strcmp(mime_type_media_file, "audio/flac") == 0) {
        rc = handle_coverextract_flac(config, uri, media_file, &binary, false, covercache);
    }
    sdsfree(mime_type_media_file);
    if (rc == true) {
        sds mime_type = get_mime_type_by_magic_stream(binary);
        sds header = sdscatfmt(sdsempty(), "Content-Type: %s", mime_type);
        header = sdscat(header, EXTRA_HEADERS_CACHE);
        http_send_header_ok(nc, sdslen(binary), header);
        mg_send(nc, binary, sdslen(binary));
        sdsfree(header);
        sdsfree(mime_type);
    }
    sdsfree(binary);
    return rc;
}

static bool handle_coverextract_id3(struct t_config *config, const char *uri, const char *media_file, 
                                    sds *binary, bool covercache)
{
    bool rc = false;
    #ifdef ENABLE_LIBID3TAG
    MYMPD_LOG_DEBUG("Exctracting coverimage from %s", media_file);
    struct id3_file *file_struct = id3_file_open(media_file, ID3_FILE_MODE_READONLY);
    if (file_struct == NULL) {
        MYMPD_LOG_ERROR("Can't parse id3_file: %s", media_file);
        return false;
    }
    struct id3_tag *tags = id3_file_tag(file_struct);
    if (tags == NULL) {
        MYMPD_LOG_ERROR("Can't read id3 tags from file: %s", media_file);
        return false;
    }
    struct id3_frame *frame = id3_tag_findframe(tags, "APIC", 0);
    if (frame != NULL) {
        id3_length_t length;
        const id3_byte_t *pic = id3_field_getbinarydata(id3_frame_field(frame, 4), &length);
        *binary = sdscatlen(*binary, pic, length);
        if (covercache == true) {
            write_covercache_file(config->workdir, uri, (char *)id3_field_getlatin1(id3_frame_field(frame, 1)), *binary);
        }
        MYMPD_LOG_DEBUG("Coverimage successfully extracted");
        rc = true;        
    }
    else {
        MYMPD_LOG_DEBUG("No embedded picture detected");
    }
    id3_file_close(file_struct);
    #else
    (void) config;
    (void) uri;
    (void) media_file;
    (void) binary;
    (void) covercache;
    #endif
    return rc;
}

static bool handle_coverextract_flac(struct t_config *config, const char *uri, const char *media_file, 
                                     sds *binary, bool is_ogg, bool covercache)
{
    bool rc = false;
    #ifdef ENABLE_FLAC
    MYMPD_LOG_DEBUG("Exctracting coverimage from %s", media_file);
    FLAC__StreamMetadata *metadata = NULL;

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
    
    if(! (is_ogg? FLAC__metadata_chain_read_ogg(chain, media_file) : FLAC__metadata_chain_read(chain, media_file)) ) {
        MYMPD_LOG_DEBUG("%s: ERROR: reading metadata", media_file);
        FLAC__metadata_chain_delete(chain);
        return false;
    }

    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
    FLAC__metadata_iterator_init(iterator, chain);
    assert(iterator);
    
    do {
        FLAC__StreamMetadata *block = FLAC__metadata_iterator_get_block(iterator);
        if (block->type == FLAC__METADATA_TYPE_PICTURE) {
            metadata = block;
        }
    } while (FLAC__metadata_iterator_next(iterator) && metadata == NULL);
    
    if (metadata == NULL) {
        MYMPD_LOG_DEBUG("No embedded picture detected");
    }
    else {
        *binary = sdscatlen(*binary, metadata->data.picture.data, metadata->data.picture.data_length);
        if (covercache == true) {
            write_covercache_file(config->workdir, uri, metadata->data.picture.mime_type, *binary);
        }
        MYMPD_LOG_DEBUG("Coverimage successfully extracted");
        rc = true;
    }
    FLAC__metadata_iterator_delete(iterator);
    FLAC__metadata_chain_delete(chain);
    #else
    (void) config;
    (void) uri;
    (void) media_file;
    (void) binary;
    (void) is_ogg;
    (void) covercache;
    #endif
    return rc;
}
