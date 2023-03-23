# Chatty

A libpurple messaging client


## Build and install

### Getting the source

```sh
git clone https://source.puri.sm/Librem5/chatty
cd chatty/
```

### Install dependencies

On a Debian based system run

``` bash
    sudo apt-get -y install build-essential ccache
    sudo apt-get -y build-dep .
```

For an explicit list of dependencies check the Build-Depends entry in the
[debian/control](https://source.puri.sm/Librem5/chatty/blob/master/debian/control#5)
file.

Plugins are optional and can be often insalled from your distribution sources.
Installation from source is required only if you want to debug the plugin itself,
or you have any reason to do so.

### Build and install the 'carbons' plugin (Optional)
Message synchronization between devices according to XEP-0280.
On Debian and derivates you can install `purple-xmpp-carbons` package.

To build from source, run:

``` bash
git clone https://github.com/gkdr/carbons.git
cd carbons
make
make install
```

### Build and install the 'lurch' plugin (Optional)
lurch plugin implements XEP-0384 (OMEMO Encryption).
On Debian and derivates you can install `purple-lurch` package.

To build from source see [lurch OMEMO plugin](https://github.com/gkdr/lurch)


### Build and install mmsd-tng (Optional)
mmsd-tng provides MMS support.  On debian and derivatives you
can install `mmsd-tng` package.

To build from source see [mmsd-tng](https://gitlab.com/kop316/mmsd)

This can be skipped if MMS is not needed.

### Build Chatty
``` bash
meson build # From chatty source directory
ninja -C build
```

## Running from the source tree
To run Chatty from source tree (without installing) do:

``` bash
build/run
# or
build/run -vvvv # To run with (verbose) logs
# or to start Chatty under `gdb`
CHATTY_GDB=1 build/run -vv
```

## Running over ssh

```sh
ssh -X purism@ip
# kill chatty twice to kill existing instance
killall chatty; killall chatty
build/run -vvv --display=$DISPLAY
```

When running over ssh, if you run into polkit permission issues
like the following:

```sh
Error sending message:
GDBus.Error:org.freedesktop.ModemManager1.Error.Core.Unauthorized:
PolicyKit authorization failed: not authorized for
'org.freedesktop.ModemManager1.Messaging'
```

you can work around the permission issue by running chatty
on device, but displaying the window on appropriate display:

```sh
ssh -X purism@ip
# kill chatty twice to kill existing instance
killall chatty; killall chatty
echo $DISPLAY # Use the value you get here in next step
```

and on the device, open some terminal and do so so that the window
will be shown in a different display:
```sh
# Replace XXX with the value you got in the previous step
build/run -vvvv --display=XXX
```

## Debugging

Chatty stores data in the following locations:
- `~/.purple` - chat history and libpurple config
- `~/.purple/chatty/db/chatty-history.db` - XMPP, SMS and MMS chat history
- `~/.purple/chatty/db/matrix.db` - Matrix chat history and related data
- `~.local/share/chatty/` - Downloaded files and avatars are stored here
- `~/.cache/chatty` - Cached data and temporary files are stored here

Also, Matrix account secrets are stored with `libsecret`, those details can
be retrieved using `GNOME Passwords`.

If you want to modify/delete any of the above data or to debug chatty,
you should first close all running instance of chatty by running
`killall chatty` multiple times until it says `chatty: no process found`.

To run chatty with verbose logs, you can run `chatty -vvvv`
(or `./run -vvvv` from the build directory). In case of crashes,
a `gdb` log is printed if `gdb` debugger is present on the system.

Please note that if you remove `~/.purple/chatty/db/matrix.db`,
your matrix account token will be renewed, resulting in your
account been seen as a new device for others.

To get better logs, you should install debug symbols of chatty and related
packages.  On Debian and derivates, [enable debug repo][0] and run:

```sh
sudo apt-get install chatty-dbgsym libglib2.0-0-dbgsym libgtk-3-0-dbgsym libsoup-3.0-0-dbgsym
```

See [GLib][1] and [Gtk][2] documentations to know more debugging details
related to `GLib` and `Gtk`.

## Translations

You can contribute translations via [GNOME Damned lies][https://l10n.gnome.org/module/chatty/]


## XMPP account

If you don't have an XMPP account yet and want to subscribe to a service then please make sure that the server supports the following XEPs:

- XEP-0237: Roster Versioning
- XEP-0198: Stream Management
- XEP-0280: Message Carbons
- XEP-0352: Client State Indication
- XEP-0313: Message Archive Management
- XEP-0363: HTTP File Upload

[0]: https://wiki.debian.org/HowToGetABacktrace
[1]: https://docs.gtk.org/glib/running.html
[2]: https://docs.gtk.org/gtk3/running.html
