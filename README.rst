Workshop
========

Workshop is a daemon which executes jobs from a queue stored in a
PostgreSQL database.  Multiple instances can run in parallel on
different hosts.

For more information, read the manual in the `doc` directory.


Building Workshop
-----------------

You need:

- a C++20 compliant compiler (e.g. gcc or clang)
- `libcap2 <https://sites.google.com/site/fullycapable/>`__
- `libfmt <https://fmt.dev/>`__
- `libpq <https://www.postgresql.org/>`__
- `Boost <http://www.boost.org/>`__
- `CURL <https://curl.haxx.se/>`__
- `D-Bus <https://www.freedesktop.org/wiki/Software/dbus/>`__
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `Meson 0.56 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Run ``meson``::

 meson . output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us
