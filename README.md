# Chatty

A simple to use messaging app for 1:1 communication and small groups supporting
SMS, MMS, matrix and XMPP through libpurple.

## Build and install

In case you want to disable the libpurple plugin support
have a look at [meson_options](meson_options)

### Getting the source

```sh
git clone https://gitlab.gnome.org/World/Chatty
cd Chatty/
```

### Install dependencies

On a Debian based system run

``` bash
    sudo apt-get -y install build-essential ccache
    sudo apt-get -y build-dep .
```

For an explicit list of dependencies check the Build-Depends entry in the
[debian/control](https://gitlab.gnome.org/World/Chatty/blob/main/debian/control#5)
file.

Plugins are optional and can often be installed from your distribution sources.
Installation from source is required only if you want to debug the plugin itself,
or you have any reason to do so.

### Build and install the 'carbons' plugin (Optional)
Message synchronization between devices according to XEP-0280.
On Debian and derivates you can install the `purple-xmpp-carbons` package.

Alternativelly, to build from source, run:

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
mmsd-tng provides MMS support.  On Debian and derivatives you
can install `mmsd-tng` package.

To build from source see [mmsd-tng](https://gitlab.com/kop316/mmsd)

This can be skipped if MMS is not needed.

### Build Chatty
``` bash
meson setup build # From chatty source directory
meson compile -C build
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

## Backup and Restore

Chatty stores data in the following locations:
- `~/.purple` - chat history and libpurple config
- `~/.purple/chatty/db/chatty-history.db` - XMPP, SMS and MMS chat history
- `~/.purple/chatty/db/matrix.db` - Matrix chat history and related data
- `~/.local/share/chatty/` - Downloaded files and avatars are stored here
- `~/.cache/chatty` - Cached data and temporary files are stored here

Also, Matrix account secrets are stored with `libsecret`, those details can
be retrieved using `GNOME Passwords`.

To backup, simply copy all of these locations. To restore, place all of the
contents back in those respective folders.

If you want to modify/delete any of the above data, you should first close
all running instance of chatty by running `killall chatty` multiple times
until it says `chatty: no process found`.

## Debugging

If you want to debug chatty, you should first close all running instance
of chatty by running `killall chatty` multiple times until it says
`chatty: no process found`.

To run chatty with verbose logs, you can run `chatty -vvvv`
(or `./run -vvvv` from the build directory). In case of crashes,
a `gdb` log is printed if `gdb` debugger is present on the system.

Please note that if you remove `~/.purple/chatty/db/matrix.db`,
your matrix account token will be renewed, resulting in your
account been seen as a new device for others.

To get better logs,
you should use debug symbols for chatty and related libraries.
You can do this either by setting `DEBUGINFOD_URLS=https://debuginfod.debian.net`
(adjust URL for your distribution)
which should result in `gdb` prompting you to download the symbols **or**
on Debian and derivates, [enable debug repo][0] and run:

```sh
sudo apt-get install chatty-dbgsym libglib2.0-0-dbgsym libgtk-4-1-dbgsym libsoup-3.0-0-dbgsym
```

See [GLib][1] and [Gtk][2] documentations to know more debugging details
related to `GLib` and `Gtk`.

## Translations

You can contribute translations via [GNOME Damned Lies](https://l10n.gnome.org/module/chatty/).

## Guidelines for Maintainers

We follow [Phosh's Guidelines for Maintainers](https://gitlab.gnome.org/World/Phosh/phosh/-/wikis/Guidelines-for-maintainers).

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
[2]: https://docs.gtk.org/gtk4/running.html
