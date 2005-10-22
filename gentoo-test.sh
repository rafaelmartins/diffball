#!/bin/sh
sudo ls && make dist && \
cp diffball-0.7.tar.bz2 /usr/portage/distfiles && \
rm -f /usr/local/portage/dev-util/diffball/files/digest-diffball-0.7 && \
ebuild /usr/local/portage/dev-util/diffball/diffball-0.7.ebuild digest && \
sudo emerge diffball
