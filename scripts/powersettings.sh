
#emacs /etc/tlp.conf

#DISK_APM_LEVEL_ON_AC="254 254"
#DISK_APM_LEVEL_ON_BAT="128 128"
#DISK_APM_LEVEL_ON_BAT="254 254"

#AHCI_RUNTIME_PM_ON_AC=on
#AHCI_RUNTIME_PM_ON_BAT=auto
#AHCI_RUNTIME_PM_ON_BAT=on


# kernel options in
# emacs /etc/default/grub
#GRUB_CMDLINE_LINUX_DEFAULT="quiet apparmor=1 security=apparmor udev.log_priority=3 intel_pstate=disable"

#disables pstate driver...or use it?

#performance mode?
sudo cpupower frequency-info
sudo cpupower frequency-get -g performance
