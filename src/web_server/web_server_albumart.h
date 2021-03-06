/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __WEB_SERVER_ALBUMART_H__
#define __WEB_SERVER_ALBUMART_H__
void send_albumart(struct mg_connection *nc, sds data, sds binary);
bool handle_albumart(struct mg_connection *nc, struct mg_http_message *hm, struct t_mg_user_data *mg_user_data, struct t_config *config, long long conn_id);
#endif
