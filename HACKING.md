Building
========
For build instructions see the README.md

Pull requests
=============
Before filing a pull request run the tests:

```sh
meson test -C _build
```

Use descriptive commit messages, see

   https://wiki.gnome.org/Git/CommitMessages

and check

   https://wiki.openstack.org/wiki/GitCommitMessages

for good examples.

Coding Style
============
We're mostly using [libhandy's Coding Style][1].

API documentation
=================
Chatty relies on [libpurple][2] for IM integration and [libebook][3] from
evolution data server for phone number parsing:

- libpurple: https://developer.pidgin.im/doxygen/
- `e_phone_`: https://developer.gnome.org/eds/stable/eds-e-phone-number.html

[1]: https://source.puri.sm/Librem5/libhandy/blob/master/HACKING.md#coding-style
[2]: https://developer.pidgin.im/wiki/WhatIsLibpurple
[3]: https://developer.gnome.org/eds/stable/
