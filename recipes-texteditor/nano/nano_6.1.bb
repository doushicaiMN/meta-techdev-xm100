SUMMARY = "nano recipe"
DESCRIPTION = "nano GNU editor"
LICENSE = "CLOSED"
DEPENDS = "ncurses file"
RDEPENDS_${PN} = "ncurses-terminfo"

# Where to find source files
SRC_URI = "file://src/${BPN}-${PV}.tar.gz"

inherit autotools gettext pkgconfig



