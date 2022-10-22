DESCRIPTION = "hrc test driver"
LICENSE = "GPLv3"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-3.0;md5=c79ff39f19dfec6d293b95dea7b07891"
inherit module
PR = "r0"
SRC_URI = "file://Makefile file://${BPN}.c"
S = "${WORKDIR}"
