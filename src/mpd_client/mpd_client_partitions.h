/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __MPD_CLIENT_PARTITIONS_H__
#define __MPD_CLIENT_PARTITIONS_H__
sds mpd_client_put_partitions(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id);
#endif
