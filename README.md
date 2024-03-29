
Copyright (c) 2000-2012 by Laurent Papier.  This program is free software
which I release under the GNU General Public License.

Description
===========
[GKrellDnet](http://gkrelldnet.sourceforge.net/) is a monitoring system for
[Distributed.net](http://www.distributed.net/) client.

Requirements
============
* a Linux system with kernel support for Unix 98 (the wrapper will only work
  on Linux system but can be ported to other Unix system).
* a working [Distributed.net](http://www.distributed.net/) client.
* [GKrellM](http://gkrellm.net/) multi-monitor app (2.0 or better).
* [libmicrohttpd](http://www.gnu.org/software/libmicrohttpd/) (optional) for the dnetw builtin web server.


GKrellDnet provides
===================
* a distibuted.net client wrapper that make possible to write monitoring applets.
  It's possible to monitor the client activity without a wrapper but not to
  monitor the crunch-o-meter. This is because the client doesn't print the
  crunch'o'meter if the client output is not a tty.
  This is why I have written this wrapper.
  The wrapper now has a builtin web server (version 0.15+). This make it easier to read
  current crunch-o-meter value from other apps.

* a GKrellM plugin.
  This plugin features (see also 'Info' in plugin config.)
    - monitor input/output work units
    - monitor current contest
    - monitor the crunch-o-meter (support both relative and absolute style)
    - start/stop the dnet client on left/right mouse button click
    - user configurable text output
    - user configurable command to start on every packet completion
	(like playing a sound)

Limitation
==========
* the plugin only support up to 10 CPU ;-)
* on multi-proc. system, you will get inaccurate contest report if not all
  crunchers work on the same contest. Sorry but the output of the dnet client
  is not very readable in that case even for a computer ;-)
* see TODO file.


Installation
============
First, compile both the dnet wrapper `dnetw` and the GKrellDnet plugin:

    tar xvfz gkrelldnet-X.Y.tar.gz
    cd gkrelldnet-X.Y
    make

If you want to compile the builtin web server (requires libmicrohttpd)

    tar xvfz gkrelldnet-X.Y.tar.gz
    cd gkrelldnet-X.Y
    sed -i "/98912660$/,+2 s/^#//" Makefile
    make

To install the GKrellDnet plugin in `~/.gkrellm/plugins`

    make install

then you need to copy the wrapper `dnetw` somewhere in your path. I choose to
copy it in the same directory as the distributed.net client and add this
directory in my path.

It is very important for the plugin to work, that __BOTH__ dnetc and dnetw are in
your path. Please check your `$PATH` variable.
In fact, gkrellm need that both `dnetc` AND `dnetw` are in his `$PATH` variable.

__NOTE__: you don't need to be root to install this package.


Usage
=====

First you need to setup your dnet client. Please see dnetc documentation.
Now, you must use the wrapper to start the dnet client or you can left click in
the GKrellM plugin (be sure that the `dnetc` executable is in the PATH of
gkrellm, see install section).

Since version 0.15, dnetw includes a builtin web server. You can active it with `--port|-p` option.
dnetw will then listen on port passed as argument. You can provide a format string
parameter to the web server via the `f` cgi parameter.

__IMPORTANT__: the web server only listen on localhost IP for security reason.

### Example:
starting the wrapper with the builtin web server listening on port 1234.

    dnetw -q -p 1234

reading current values (default output)

    curl http://localhost:1234/
    ogr:1:3:351.54Gn:85.06Gn

reading current values (custom output)

    curl 'http://localhost:1234/?f=$c:%20$i/$o%20-%20$p0/$p1'
    ogr: 1/3 - 351.76Gn/85.30Gn

### Possible formating

  + $i is the input work units
  + $o is the output work units
  + $c/$C is the current contest in lowercase/uppercase
  + $pn is the progress indicator value in current block/stub on CPU #n ($p <=> $p0)

VERY IMPORTANT:
---------------
dnetw only works if you __DO NOT__ redirect the client output to a file.
The client output __MUST__ be stdout. So check your client configuration.
The new absolute style crunch-o-meter is supported in v0.7, so you can set
relative or absolute mode in the dnet client config.
Currently, 'live-rate' is not supported. Wait for the next release.

If you want to redirect the dnet client output to a file you must use dnetw
option `-l`.

    dnetw -l dnet.log

this command will start and monitor the dnet client and redirect the client
output to `dnet.log`.

    dnetw

this command will start and monitor the dnet client and print the client
output on your terminal.

Now, it's possible to give the full dnetc command line to dnetw.

    dnetw -c "dnetc -numcpu 2" -l dnet.log

this command will force the client to start 2 crunchers.

__NOTE__: you must enclose the command line with simple or double quotes. (read
your shell man page for more info).

IMPORTANT:
----------

Starting from v0.8, the monitor file is no longer used.
Starting from v0.9, the dnetw command line is no longer compatible with
previous version. You need to remove the <monitor file> argument from your
command line. If you start dnetw from the GKrellDnet plugin no change is need.

 
Open source make it possible
============================
I have start writing a gkrellm plugin to monitor the dnetw output. But you can
write your own monitoring applet (WindowMaker Dock, Gnome/KDE applets or
epplets), I will only work on the wrapper `dnetw` and the gkrellm plugin.

Both the wrapper and the plugin seem stable on my system (Fedora Core 12).
If you have problems, first check installation and usage section of this file.
If you still have problems you can contact me.

Laurent Papier - <papier@tuxfan.net>


Thanks to
=========
Bill Wilson for this really great monitoring tool.

Christoph Hohmann for the multi krell patch.
