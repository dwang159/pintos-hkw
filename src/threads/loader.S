#include "threads/loader.h"

#### Kernel loader.

#### This code should be stored in the first sector of a hard disk.
#### When the BIOS runs, it loads this code at physical address
#### 0x7c00-0x7e00 (512 bytes) and jumps to the beginning of it,
#### in real mode.  The loader loads the kernel into memory and jumps
#### to its entry point, which is the start function in start.S.
####
#### The BIOS passes in the drive that the loader was read from as
#### DL, with floppy drives numbered 0x00, 0x01, ... and hard drives
#### numbered 0x80, 0x81, ...  We want to support booting a kernel on
#### a different drive from the loader, so we don't take advantage of
#### this.

# Runs in real mode, which is a 16-bit segment.
	.code16

# Set up segment registers.
# Set stack to grow downward from 60 kB (after boot, the kernel
# continues to use this stack for its initial thread).

	sub %ax, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov $0xf000, %esp

# Configure serial port so we can report progress without connected VGA.
# See [IntrList] for details.
	sub %dx, %dx			# Serial port 0.
	mov $0xe3, %al			# 9600 bps, N-8-1.
					# AH is already 0 (Initialize Port).
	int $0x14			# Destroys AX.

	call puts
	.string "PiLo"

#### TODO:  ITERATE THROUGH ALL HARD DISKS IN THE SYSTEM, LOOKING FOR A
####        BOOTABLE PINTOS PARTITION TO LOAD AND START.
####
####        Note that there are only ~300 bytes available for the entire
####        Pintos bootloader, as Pintos commands are stored in the
####        region immediately before the partition table in the sector.
####        Sooo... be concise!
####
####        If you want to create helper routines, there is a TODO toward
####        the end of this file that shows where they should go.

# Psuedo code:
#main:
#    for hd in hard_drives {
#        mbr = load_sector(hd, 0)
#        if not boot_flag(mbr):
#            continue
#        print "Booting from hd
#        for part in partitions(mbr):
#            if (bootable(part) and type(part) == PINTOS):
#                try:
#                    hdname = "hd%c%d" %(hd - 'a', indexof(part, partitions(mbr)))
#                    print "Attempting to copy kernel from hd"
#                    copy(hard_drive = hd, src = lba(part), tgt=0x20000, size=512k)
#                    print "booting from " + hdname
#                    goto start
#                catch:
#                    print "Copy failed, finding next partition"
#                    continue
#        print("Could not find boot part on bootable disk")
#        diskless_boot_hook()
#   print("Could not find bootable pintos part")

#diskless_boot_hook:
#   int $0x13
#load_sector: hd, lba
#   Extended read: AH = 0x42, DL = hd, DS:SI = DAP address
#                 DAP = {0: 0x10, 1: 0, 2-3: 1, 4-7:mbr_address, 8-15:0}
#copy: lba(4 , target, size, hd
#   Extended read: AH = 0x42, DL = hk, DS:SI = DAP address
#                (little endian)
#                 DAP = {0: 0x10, 1: 0, 2-3: size (in sectors)
#                        4-7: lba}
#boot_flag: mbr
#   return mbr[0x1fe] == 0x55 && mbr[0x1ff] == 0xaa (both 1 byte)
#partitions: mbr
#   return mbr + 0x1BE (*(16 byte))
#type: part
#   return part[0x04] (1 byte)
#PINTOS:
#   0x20
#lba: part
#   return part[0x08] (4 bytes) 
#bootable: part
#   return part[0] == 0x80 (1 byte)

	call puts
	.string "\r\rTODO:  IMPLEMENT THE BOOTLOADER\r"

	# Stop the processor.
	hlt

#### TODO:  LOADING CODE ENDS HERE!

#### Transfer control to the kernel that we loaded.  We read the start
#### address out of the ELF header (see [ELF1]) and convert it from a
#### 32-bit linear address into a 16:16 segment:offset address for
#### real mode, then jump to the converted address.  The 80x86 doesn't
#### have an instruction to jump to an absolute segment:offset kept in
#### registers, so in fact we store the address in a temporary memory
#### location, then jump indirectly through that location.  To save 4
#### bytes in the loader, we reuse 4 bytes of the loader's code for
#### this temporary pointer.

	mov $0x2000, %ax
	mov %ax, %es
	mov %es:0x18, %dx
	mov %dx, start
	movw $0x2000, start + 2
	ljmp *start

#### The following label is a placeholder for the preceding code to use
#### to set up for the jump to the Pintos kernel.  Of course, the start
#### label also points to the puts function; when the previous code runs,
#### it will clobber a few of the bytes at the start of the puts function,
#### but hey we're done bootloading, so we don't need it anymore.

start:

#### Print string subroutine.  To save space in the loader, this
#### subroutine takes its null-terminated string argument from the
#### code stream just after the call, and then returns to the byte
#### just after the terminating null.  This subroutine preserves all
#### general-purpose registers.

puts:	xchg %si, %ss:(%esp)
	push %ax
next_char:
	mov %cs:(%si), %al
	inc %si
	test %al, %al
	jz 1f
	call putc
	jmp next_char
1:	pop %ax
	xchg %si, %ss:(%esp)
	ret

#### Character output subroutine.  Prints the character in AL to the
#### VGA display and serial port 0, using BIOS services (see
#### [IntrList]).  Preserves all general-purpose registers.
####
#### If called upon to output a carriage return, this subroutine
#### automatically supplies the following line feed.

putc:	pusha

1:	sub %bh, %bh			# Page 0.
	mov $0x0e, %ah			# Teletype output service.
	int $0x10

	mov $0x01, %ah			# Serial port output service.
	sub %dx, %dx			# Serial port 0.
2:	int $0x14			# Destroys AH.
	test $0x80, %ah			# Output timed out?
	jz 3f
	movw $0x9090, 2b		# Turn "int $0x14" above into NOPs.

3:
	cmp $'\r', %al
	jne popa_ret
	mov $'\n', %al
	jmp 1b

popa_ret:
	popa
	ret				# Error code still in CF


#### TODO:  YOU CAN PUT HELPER FUNCTIONS HERE IF YOU WISH.


#### Command-line arguments and their count.
#### This is written by the `pintos' utility and read by the kernel.
#### The loader itself does not do anything with the command line.
	.org LOADER_ARG_CNT - LOADER_BASE
	.fill LOADER_ARG_CNT_LEN, 1, 0

	.org LOADER_ARGS - LOADER_BASE
	.fill LOADER_ARGS_LEN, 1, 0

#### Partition table.
	.org LOADER_PARTS - LOADER_BASE
	.fill LOADER_PARTS_LEN, 1, 0

#### Boot-sector signature for BIOS inspection.
	.org LOADER_SIG - LOADER_BASE
	.word 0xaa55
