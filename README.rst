Workshop
========

Workshop is a daemon which executes jobs from a queue stored in a
PostgreSQL database.  Multiple instances can run in parallel on
different hosts.

For more information, `read the manual
<https://cm4all-workshop.readthedocs.io/en/latest/>`__ in the ``doc``
directory.


Building Workshop
-----------------

You need:

- Linux kernel 5.12 or later
- a C++20 compliant compiler (e.g. gcc or clang)
- `libfmt <https://fmt.dev/>`__
- `libpq <https://www.postgresql.org/>`__
- `CURL <https://curl.haxx.se/>`__
- `Meson 0.56 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Optional dependencies:

- `D-Bus <https://www.freedesktop.org/wiki/Software/dbus/>`__
- `libcap2 <https://sites.google.com/site/fullycapable/>`__ for
  dropping unnecessary Linux capabilities
- `libseccomp <https://github.com/seccomp/libseccomp>`__ for system
  call filter support
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__

Run ``meson``::

 meson setup output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies (``dpkg-checkbuilddeps``),
run::

 dpkg-buildpackage -rfakeroot -b -uc -us
