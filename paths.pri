isEmpty(PREFIX) {
	PREFIX = /usr/local
}

debug {
	ICON_PATH = $$PWD/icons
} else {
	ICON_PATH = $$PREFIX/share/peerdrive/icons
}
