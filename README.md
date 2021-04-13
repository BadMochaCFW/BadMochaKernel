# Wii U Linux
**A port of Linux 4.19 to the Wii U baremetal.**

[![pipeline status](https://gitlab.com/linux-wiiu/linux-wiiu/badges/rewrite-4.19/pipeline.svg)](https://gitlab.com/linux-wiiu/linux-wiiu/commits/rewrite-4.19)

### What's this?
This is a port of the Linux kernel to the Wii U, allowing you to use a variety of PowerPC Linux distributions such as [Debian SID](https://wiki.linux-wiiu.org/wiki/Distributions/Debian), [Lubuntu 16.04](https://wiki.linux-wiiu.org/wiki/Distributions/Lubuntu), [Void](https://wiki.linux-wiiu.org/wiki/Distributions/Void) or [Adélie](https://wiki.linux-wiiu.org/wiki/Distributions/Ad%C3%A9lie). It runs without any of Nintendo's Cafe code loaded, instead using [linux-loader](https://gitlab.com/linux-wiiu/linux-loader) on the ARM side and keeping the PowerPC entirely to itself. This branch is a port of version 4.19, an LTS release that should receive updates until the end of 2024.

### Device Support
As of writing; this fork supports:

 - USB OHCI/EHCI (front, back, all known internal ports)
 	- Bluetooth
 	- DRH input driver (Gamepad buttons, touch, motion controls)
 - SD card slot
 - Framebuffer graphics, both TV and Gamepad displays (no acceleration)
 - One PowerPC core (SMP is possible, but would break existing userspaces)
 - 2080MiB RAM (MEM1, MEM2 reclaimed from ARM)
 - Simple IPC to ARM (poweroff/reboot)
 - Both interrupt controllers (listen, it took a lot of work, it's on the list)

More up-to-date information about supported hardware can be found on [the wiki's Kernel/Hardware Support page](https://wiki.linux-wiiu.org/wiki/Kernel/Hardware_Support), along with a comparison against previous versions. Progress relating to hardware support is tracked with GitLab issues, visible on [the boards](https://gitlab.com/linux-wiiu/linux-wiiu/boards).

### Getting Linux
##### Prebuilt Download
For an all-in-one kernel (best if you're a Windows user and can't mount your distribution image) you can [download a prebuilt image](https://gitlab.com/linux-wiiu/linux-wiiu/-/jobs/artifacts/rewrite-4.19/raw/dtbImage.wiiu?job=linux-build) - see [linux-wiiu/linux-loader](https://gitlab.com/linux-wiiu/linux-loader) for your next steps on setting it up. If you're able to access your distribution image after you flash it, we suggest a modular kernel with more drivers for things like USB devices and network adapters available. You can [download a kernel and modules](https://gitlab.com/linux-wiiu/linux-wiiu/-/jobs/artifacts/rewrite-4.19/browse?job=linux-build-modular) - make sure to put the kernel modules in the `/lib/modules` directory of your rootfs.

##### Compiling (Docker)
If you're the type to compile things yourself, you can use our Docker image to sort everything out for you. (Quick warning: the image is ~250MiB on disk)
```sh
#Download linux-wiiu
git clone https://gitlab.com/linux-wiiu/linux-wiiu #or however you want to do that
cd linux-wiiu
#run the docker container
#--rm (clean up after us); -it (use a terminal)
#-v $(pwd):/linux-wiiu (mount the current directory as /linux-wiiu)
docker run --rm -it -v $(pwd):/linux-wiiu quarktheawesome/linux-wiiu-builder
#You should now be in the container - your shell prompt will change
cd linux-wiiu
#Configure Linux - the needed make flags are in $LINUXMK to save typing
make wiiu_defconfig $LINUXMK
make -j4 $LINUXMK
#We're done! Exit the container and get back to the host OS
exit
```
Once this completes, you should find dtbImage.wiiu at `arch/powerpc/boot/dtbImage.wiiu`. Check out [linux-wiiu/linux-loader](https://gitlab.com/linux-wiiu/linux-loader) to get this running on your Wii U.

##### Compiling (from scratch)
See [the wiki's Kernel/Building Kernel page](https://wiki.linux-wiiu.org/wiki/Kernel/Building_kernel).

### Booting
The kernel commandline is hardcoded (for now) with `root=/dev/sda1 rootwait` - check out [linux-wiiu/linux-loader's boot.cfg support](https://gitlab.com/linux-wiiu/linux-loader#advanced-setup-bootcfg) for info on how you can change this. If you stick with the defaults, you'll need to use a USB flash drive as your rootfs. Format it however you'd like (yes, ext4/gpt works) and throw a distro on it. Plug it and a USB keyboard into the Wii U. Run [linux-wiiu/linux-loader](https://gitlab.com/linux-wiiu/linux-loader) (as described in that repo's README) and enjoy your Linux!

### Distributions and Programs
Please see the [linux-wiiu wiki](https://wiki.linux-wiiu.org/) for information about different distributions and how to install them.
