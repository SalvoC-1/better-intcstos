# Welcome to intcstOS's repository
## What is this?

intcstOS is my own, 32-bits(and 64 bits later) OS made from scratch using GRUB as a bootloader and C as programming language.

## Who made this? 

sondaproject is the team of people that should have built intcst, but i had no one to help me, so i decided to carry sondaproject alone with some help

# How to test it?

you can download the ISO in the release tab OR you can compile it yourself! here is how to do it

the boot.s:

```
i686-elf-as boot/boot.s -o boot/boot.o
```

the main kernel
```
i686-elf-gcc -c kernel/main.c -o kernel/main.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
```

link everything up
```
i686-elf-gcc -T linker/linker.ld -o iso/boot/kernel.bin -ffreestanding -O2 -nostdlib boot/boot.o kernel/kernel.o -lgcc
```

now finally to compile the iso:
```
grub-mkrescue -o intcst.iso iso/
```

## tools you need

you must be on arch linux and you can install(or compile if you prefer) these:
```
yay -S gcc i686-elf-bin grub
```

and qemu if you want to virtual machine it
```
sudo pacman -S qemu-full
```
just n case you dont know how to boot it
```
qemu-system-i386 -cdrom intcst.iso
