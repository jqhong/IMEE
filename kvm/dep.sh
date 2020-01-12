#compile the KVM module and deploy it

ver=3.13.11-ckt39

make M=arch/x86/kvm
sudo cp arch/x86/kvm/{kvm.ko,kvm-intel.ko} /lib/modules/$ver/kernel/arch/x86/kvm/
sync
sudo rmmod kvm-intel && sudo rmmod kvm
sudo modprobe kvm && sudo modprobe kvm-intel
# sudo modprobe kvm
