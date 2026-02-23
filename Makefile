ARCH := x86_64
BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/isofiles

CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-red-zone -mcmodel=kernel -Wall -Wextra -Werror -Iinclude -std=c11 -O2
ASFLAGS := -ffreestanding -fno-pic -m64 -Iinclude
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T linker.ld

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_MAP := $(BUILD_DIR)/kernel.map
ISO_IMAGE := $(BUILD_DIR)/nevermind-m1.iso

BOOT_SRCS := boot/entry.S
PROC_ASM_SRCS := kernel/proc/switch.S
KERNEL_SRCS := \
	kernel/kmain.c \
	kernel/console.c \
	kernel/gdt.c \
	kernel/idt.c \
	kernel/tss.c \
	kernel/string.c \
	kernel/mm/pmm.c \
	kernel/mm/vmm.c \
	kernel/mm/kheap.c \
	kernel/proc/task.c \
	kernel/proc/sched.c \
	kernel/syscall/syscall.c \
	kernel/fs/vfs.c \
	kernel/fs/tmpfs.c \
	kernel/fs/ext2.c \
	kernel/drivers/irq.c \
	kernel/drivers/pit.c \
	kernel/drivers/keyboard.c \
	kernel/drivers/pci.c \
	kernel/drivers/rtl8139.c \
	kernel/net/net.c \
	kernel/net/arp.c \
	kernel/net/ipv4.c \
	kernel/net/icmp.c \
	kernel/net/udp.c \
	kernel/net/tcp.c \
	kernel/net/socket.c

OBJS := $(BOOT_SRCS:%.S=$(BUILD_DIR)/%.o) $(PROC_ASM_SRCS:%.S=$(BUILD_DIR)/%.o) $(KERNEL_SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean iso run-bios run-uefi smoke test

all: $(KERNEL_ELF) iso

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -Map $(KERNEL_MAP) -o $@ $(OBJS)

iso: $(KERNEL_ELF) grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR) >/dev/null

run-bios: all
	./scripts/run_qemu_bios.sh $(ISO_IMAGE)

run-uefi: all
	./scripts/run_qemu_uefi.sh $(ISO_IMAGE)

smoke: all
	./tests/smoke_m1.sh $(ISO_IMAGE)

test:
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 \
	  tests/unit/test_pmm_kheap.c kernel/mm/pmm.c kernel/mm/kheap.c kernel/string.c \
	  -Iinclude -DNEVERMIND_HOST_TEST -o $(BUILD_DIR)/test_pmm_kheap
	$(BUILD_DIR)/test_pmm_kheap
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 \
	  tests/unit/test_sched.c kernel/proc/task.c kernel/proc/sched.c \
	  -Iinclude -DNEVERMIND_HOST_TEST -o $(BUILD_DIR)/test_sched
	$(BUILD_DIR)/test_sched
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 \
	  tests/unit/test_vfs.c kernel/fs/vfs.c kernel/fs/tmpfs.c kernel/fs/ext2.c kernel/string.c \
	  -Iinclude -DNEVERMIND_HOST_TEST -o $(BUILD_DIR)/test_vfs
	$(BUILD_DIR)/test_vfs
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 \
	  tests/unit/test_irq_pci.c kernel/drivers/irq.c kernel/drivers/pci.c kernel/string.c \
	  -Iinclude -DNEVERMIND_HOST_TEST -o $(BUILD_DIR)/test_irq_pci
	$(BUILD_DIR)/test_irq_pci
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 \
	  tests/unit/test_net.c kernel/net/net.c kernel/net/arp.c kernel/net/ipv4.c kernel/net/icmp.c \
	  kernel/net/udp.c kernel/net/tcp.c kernel/net/socket.c kernel/string.c \
	  -Iinclude -DNEVERMIND_HOST_TEST -o $(BUILD_DIR)/test_net
	$(BUILD_DIR)/test_net



clean:
	rm -rf $(BUILD_DIR)
