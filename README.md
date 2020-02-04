# Wii U Linux
**A port of Linux 4.19 to the Wii U baremetal.**

[![pipeline status](https://gitlab.com/linux-wiiu/linux-wiiu/badges/rewrite-4.19/pipeline.svg)](https://gitlab.com/linux-wiiu/linux-wiiu/commits/rewrite-4.19)

### What's this?
This is a port of the Linux kernel to the Wii U, allowing you to use a variety of PowerPC Linux distributions such as [Debian SID](https://www.debian.org/ports/powerpc/), [Lubuntu 16.04](https://lubuntu.me), [Void](https://voidlinux-ppc.org/) or [Adélie](https://www.adelielinux.org/). It runs without any of Nintendo's Cafe code loaded, instead using [linux-loader](https://gitlab.com/linux-wiiu/linux-loader) on the ARM side and keeping the PowerPC entirely to itself. This branch is a port of version 4.19, an LTS release that should receive updates until the end of 2020.

### Device Support
As of writing; this fork supports:

 - USB OHCI/EHCI (front, back, all known internal ports)
 	- Bluetooth
 	- Very basic DRH input driver (Gamepad buttons!)
 - SD card slot
 - Framebuffer graphics, both TV and Gamepad displays (no acceleration)
 - One PowerPC core (SMP is possible, but would break existing userspaces)
 - 2080MiB RAM (MEM1, MEM2 reclaimed from ARM)
 - Simple IPC to ARM (poweroff/reboot)
 - Both interrupt controllers (listen, it took a lot of work, it's on the list)

Progress relating to hardware support is tracked with GitLab issues, visible on [the boards](https://gitlab.com/linux-wiiu/linux-wiiu/boards).

### Getting Linux
##### Prebuilt Download
You can download a prebuilt image [here](https://gitlab.com/linux-wiiu/linux-wiiu/-/jobs/artifacts/rewrite-4.19/raw/dtbImage.wiiu?job=linux-build). See [linux-wiiu/linux-loader](https://gitlab.com/linux-wiiu/linux-loader) for your next steps on setting it up.

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
You'll need a PowerPC toolchain. It seems devkitPPC *will not* work. Assuming your chain is `powerpc-linux-gnu-`:
```sh
#clone repo however you'd like
make wiiu_defconfig ARCH=powerpc CROSS_COMPILE=powerpc-linux-gnu- CROSS32_COMPILE=powerpc-linux-gnu-
#if you'd like to make some changes, do menuconfig here
#add -j4 if you'd like
make ARCH=powerpc CROSS_COMPILE=powerpc-linux-gnu- CROSS32_COMPILE=powerpc-linux-gnu-
```
This'll build `arch/powerpc/boot/dtbImage.wiiu`. From here, check on [linux-wiiu/linux-loader](https://gitlab.com/linux-wiiu/linux-loader) for your next steps. There are no kernel modules to worry about, unless you build your own.

### Booting
The kernel commandline is hardcoded (for now) with `root=sda1 rootwait` - check out [linux-wiiu/linux-loader's boot.cfg support](https://gitlab.com/linux-wiiu/linux-loader#advanced-setup-bootcfg) for info on how you can change this. If you stick with the defaults, you'll need to use a USB flash drive as your rootfs. Format it however you'd like (yes, ext4/gpt works) and throw a distro on it - see below for Debian instructions. Plug it and a USB keyboard into the Wii U. Run [linux-wiiu/linux-loader](https://gitlab.com/linux-wiiu/linux-loader) (as described in that repo's README) and enjoy your Linux!

### Distributions and Programs
***Note: These instructions haven't been updated for a good while! We're currently working on improving our documentation for distributions. For now, [here's an updated tarball for Debian](https://mega.nz/#!IPYkhIzJ!Ov5bzbnspmccCcPzZHJjHn75-EvZ8b-7ckZaXgLmqiI) (`root:wiiu`/`wiiu:wiiu`), and [a post about Lubuntu 16.04](https://gbatemp.net/threads/wii-u-linux.495888/page-19#post-8171762). rootfs tarballs provided by [Adélie](https://distfiles.adelielinux.org/adelie/) and [Void](https://repo.voidlinux-ppc.org/live/current/) should work, following the Debian instructions below.***

While we started off developing with Gentoo, we swapped to Debian unstable (everything's precompiled) so that's what we recommend you do too. Debain stable/testing is unbearably outdated on PowerPC so yes, you should use sid/unstable. To make a system, you can use debootstrap or our prebuilt option:

1. Be on Linux. Get a USB (512mb bare minimum) and format it with a single ext4 partition. This will be your rootfs, so make sure it's a decent quality one (speed is important)
2. Download [this archive](https://mega.nz/#!la52GDSS!Y9TnuFmvbWRbFZww7LPvVsyh2egz4CTDyxC2R5r62r4), we'll call it "debian.tar.xz"
3. Mount and cd into your new USB.
4. Run `tar -xvpf <path/to/debian.tar.xz>`. The p is important, you could skip the v.
5. Run `sync` to ensure that the files have been written to your drive.
6. Eject the drive and plug it into your Wii U. With any luck, it'll boot Debian! Log in with username root and password root.

For Windows users, we've created an image as of 2018-02-28 that you can load with Win32DiskImager (or equivalent). For now, it's a fixed 2 GB image, but partition resizing might be implemented eventually. Instructions:

1. Grab a USB (2 GB minimum) and plug it in. Note the drive letter; you'll need this in step 3.
2. Download [this](https://mega.nz/#!PvhwlaYT!iVGqf7W7dm4XATwJFiCeTzgvaPXBhVLIhzuiUjPF6JQ) disk image and a copy of [Win32DiskImager](https://sourceforge.net/projects/win32diskimager/files/).
3. In Win32DiskImager, load the disk image, select the drive letter of your USB, and click Write. **Be very careful to pick the right drive letter; you don't want to write to your main hard disk!**
4. After Win32DiskImager finishes writing, eject the drive and plug it into your Wii U. With any luck, it'll boot Debian! Log in with username root and password root.

(This comes with a small bonus: you can also use Win32DiskImager to back up your root filesystem by reading the drive to a new file.)

##### Notes
 - This version of Debian is set up to keep the kernel up to date - it'll mount the SD whenever it does this. Remove `deb.heyquark.com` from the apt sources to disable this. (***Update: you should definitely do this***)
 - X.org works (w/ software rendering).
 - If you can come up with a better guide (esp. one including steps for Windows users) feel free to PR it in.
