SUMMARY = "Ublox F9 recipe"
DESCRIPTION = "Build and install Ublox F9 firmware update tool"
LICENSE = "CLOSED"

# Where to find source files
SRC_URI = "file://src/firmwareUpdateTool_v21.05"

# Where to keep downloaded source files (in tmp/work/....)
S = "${WORKDIR}/src"

# Pass arguments to linker (this is necessary)
TARGET_CC_ARCH += "${LDFLAGS}"

# Cross-compile source code
do_compile() {
    cd firmwareUpdateTool_v21.05
    oe_runmake
}

# Create /usr/bin in rootfs and copy program to it
do_install() {
    install -d ${D}${bindir}
    install -m 0555 firmwareUpdateTool_v21.05/bin/ubxfwupdate ${D}${bindir}
}


