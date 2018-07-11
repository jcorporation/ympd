myMPD
====

myMPD is a lightweight MPD web client that runs without a dedicated webserver or interpreter. 
It's tuned for minimal resource usage and requires only very litte dependencies.
myMPD is a fork of ympd (https://github.com/notandy/ympd).

This fork provides a reworked ui based on Bootstrap 4 and a modernized backend.

Design principles:
 - Keep it small and simple
 - Uses only mpd as source of truth
 - Mobile first UI
 - Keep security in mind

Featurelist:
 - Browse mpd database by artist
 - Browse filesystem and playlists
 - Queue management
 - Advanced search
 - Progressiv Web App enabled
 - Local coverart support

myMPD is work in progress. Bugreportes and feature requests are very welcome. Please use the issues function from github (https://github.com/jcorporation/myMPD/issues).
Planed functions are managed in a github project (https://github.com/jcorporation/myMPD/projects/1).

![image](https://jcgames.de/stuff/myMPD/screenshots.gif)

UI Components
-------------
 - Bootstrap 4: https://getbootstrap.com/
 - Material Design Icons: https://material.io/tools/icons/
 - Bootstrap Native: http://thednp.github.io/bootstrap.native/

Backend
-------
 - Mongoose: https://github.com/cesanta/mongoose
 - Frozen: https://github.com/cesanta/frozen

Dependencies
------------
 - libmpdclient 2: http://www.musicpd.org/libs/libmpdclient/
 - cmake 2.6: http://cmake.org/
 - OpenSSL: https://www.openssl.org/

Unix Build Instructions
-----------------------

1. install dependencies. cmake, libmpdclient (dev) and OpenSSL (dev) are available from all major distributions.
2. build and install it ```cd /path/to/src; ./mkrelease.sh```
3. Link your mpd music directory to ```/usr/share/mympd/htdocs/library``` and put ```folder.jpg``` files in your album directories (mkrelease.sh tries to do this for you)
4. Configure your mpd with http stream output to use the local player

Run flags
---------
```
Usage: ./mympd [OPTION]...

 -h, --host <host>             connect to mpd at host [localhost]
 -p, --port <port>             connect to mpd at port [6600]
 -w, --webport <port>          listen port for webserver [80]
 -S, --ssl		       enable ssl
 -W, --sslport		       listen port for ssl webserver [443]
 -C, --sslcert		       filename for ssl certificate [/etc/mympd/ssl/server.pem]
 -K, --sslkey		       filename for ssl key [/etc/mympd/ssl/server.key]
 -s, --streamport <port>       connect to mpd http stream at port [8000]
 -u, --user <username>         drop priviliges to user after socket bind
 -m, --mpdpass <password>      specifies the password to use when connecting to mpd
 -i, --coverimage <filename>   filename for coverimage [folder.jpg]
 -t, --statefile <filename>    filename for mympd state [/var/lib/mympd/mympd.state]
 -v, --version                 get version
 --help                        this help
```

SSL
---

1. Create ca and certificate ```/path/to/src/contrib/crcert.sh``` (mkrelease.sh do this for you)
2. Start mympd with ```-S``` (use default certificate under ```/etc/mympd/ssl/```)
3. Import ```/etc/mympd/ssl/ca/ca.pem``` in your browser to trust the certificate

Copyright
---------
ympd: 2013-2014 <andy@ndyk.de>

myMPD: 2018 <mail@jcgames.de>
