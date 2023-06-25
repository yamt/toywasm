#! /bin/sh

# https://wiki.ubuntu.com/Testing/EnableProposed
# https://help.ubuntu.com/community/PinningHowto
# https://manpages.ubuntu.com/manpages/focal/man5/apt_preferences.5.html

cat <<EOF > /etc/apt/sources.list.d/ubuntu-focal-proposed.list
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ focal-proposed restricted main multiverse universe
EOF

cat <<EOF > /etc/apt/preferences.d/proposed-updates
Package: *
Pin: release a=focal-proposed
Pin-Priority: 400
EOF
