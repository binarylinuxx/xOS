kernel_source_files := $(shell find src/impl/kernel -name '*.c')
kernel_object_files := $(patsubst src/impl/kernel/%.c, build/kernel/%.o, $(kernel_source_files))

libc_source_files := $(shell find src/impl/libc -name '*.c')
libc_object_files := $(patsubst src/impl/libc/%.c, build/libc/%.o, $(libc_source_files))

x86_64_c_source_files := $(shell find src/impl/x86_64 -name '*.c')
x86_64_c_object_files := $(patsubst src/impl/x86_64/%.c, build/x86_64/%.o, $(x86_64_c_source_files))

x86_64_asm_source_files := $(shell find src/impl/x86_64 -name '*.asm')
x86_64_asm_object_files := $(patsubst src/impl/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(kernel_object_files) $(libc_object_files) build/fs/bfs.o $(x86_64_c_object_files) $(x86_64_asm_object_files)

$(kernel_object_files): build/kernel/%.o : src/impl/kernel/%.c
	mkdir -p $(dir $@)
	gcc -c -I src/intf -ffreestanding -fno-stack-protector $< -o $@

$(libc_object_files): build/libc/%.o : src/impl/libc/%.c
	mkdir -p $(dir $@)
	gcc -c -I src/intf -ffreestanding -fno-stack-protector $< -o $@

build/fs/bfs.o: src/impl/fs/bfs.zig src/intf/bfs.h
	mkdir -p $(dir $@)
	zig build-obj $< -I src/intf -O ReleaseSmall -fno-stack-protector -target x86_64-freestanding-none -femit-bin=$@

$(x86_64_c_object_files): build/x86_64/%.o : src/impl/x86_64/%.c
	mkdir -p $(dir $@)
	gcc -c -I src/intf -ffreestanding -fno-stack-protector $< -o $@

$(x86_64_asm_object_files): build/x86_64/%.o : src/impl/x86_64/%.asm
	mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

EFI_BOOTLOADER := targets/x86_64/iso/efi/boot/bootx64.efi

$(EFI_BOOTLOADER):
	mkdir -p $(dir $@)
	grub-mkimage -o $@ -p /boot/grub -O x86_64-efi part_gpt part_msdos fat ext2 normal configfile loopback chain efifwsetup efi_gop efi_uga ls search search_label search_fs_uuid search_fs_file gfxterm gfxmenu all_video loadenv exfat multiboot multiboot2

.PHONY: build-x86_64
build-x86_64: $(x86_64_object_files) $(EFI_BOOTLOADER)
	mkdir -p dist/x86_64
	ld -m elf_x86_64 -n -o dist/x86_64/kernel.bin -T targets/x86_64/linker.ld build/x86_64/boot/header.o $(filter-out build/x86_64/boot/header.o, $(x86_64_object_files))
	cp dist/x86_64/kernel.bin targets/x86_64/iso/boot/kernel.bin
	grub-mkrescue -o /tmp/bOS.iso targets/x86_64/iso
	mv /tmp/bOS.iso dist/x86_64/xOS.iso

.PHONY: clean
clean:
	rm -rf build dist
	rm -f $(EFI_BOOTLOADER)

