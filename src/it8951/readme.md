

basic idea: kernel-mode driver abstracts away the quirks of the SPI interface (preambles, waiting on HRDY), user-mode driver speaks plain I80 protocol on top via read/write/ioctl.

IT8951_IOCTL_RESET
IT8951_IOCTL_COMMAND __u16
