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

####        ITERATE THROUGH ALL HARD DISKS IN THE SYSTEM, LOOKING FOR A
####        BOOTABLE PINTOS PARTITION TO LOAD AND START.
####
####        Note that there are only ~300 bytes available for the entire
####        Pintos bootloader, as Pintos commands are stored in the
####        region immediately before the partition table in the sector.
####        Sooo... be concise!

# We use 0x20000 as the location for the buffer when reading from the hard
# drives, and 0x1fff0 (16 bytes before the buffer) as the location for the DAP.
# When we want to provide a segement:offset pointer to the buffer, use:
#
# segment: 0x2000
# offset: 0x0
#
# And to provide a segment:offset pointer to the DAP, use:
#
# segment: 0x1fff
# offset: 0x0

# Loop through all hard drives.
  mov $0x80, %dl

# Read the first sector (the MBR)
check_mbr:
  # Set up the DAP
  mov $0x1fff0, %ebx
  # DAP[0] = 0x10
  movb $0x10, (%ebx)
  # DAP[1] = 0
  movb $0x00, 1(%ebx)
  # DAP[2] = number of sectors (2 bytes)
  movw $0x01, 2(%ebx)
  # DAP[4] = sector:offset to buffer = 0x2000:0x0000
  movl $0x20000000, 4(%ebx)
  # DAP[8] = start of sectors to be read = 0
  movl $0x00, 8(%ebx)
  movl $0x00, 12(%ebx)

  # Read the MBR from the drive.
  mov $0x42, %ah
  mov $0x1fff, %ds
  mov $0x0000, %si
  int $0x13

  # Check status of disk read. The carry flag (CF) is set
  # if there was an error.
  jc unable_to_boot

  # Check if MBR has boot flag. To do this, check if the
  # last two bytes of the MBR have values 0x55 and 0xAA.
  movl $0x20000, %ebx
  cmpw $0x55aa, 0x1fe(%ebx)
  jne next_hd

  # Set %ecx to start of partition entries.
  leal 0x1be(%ebx), %ecx

check_part:
  # Iterate through all 4 partitions of this hard drive
  # Check if drive is bootable. If so, check type.
  # Otherwise, move on.
  cmpb (%ecx), $0x80
  je type_check

  # Jump to next partition unless we've checked all of them;
  # go to an error handler in that case.
  cmpl %ecx,
  jne next_part
  jmp no_boot_part

type_check:
  # Check to see if the drive has Pintos type (0x20). If so, start
  # copying. Otherwise, move to next drive.
  cmpb 0x04(%ecx), $0x20
  je copy_kernel
  jmp next_part

copy_kernel:
  # %ecx points to the start of the partition table
  # %dl has the hard disk number

# Set up the DAP
  mov $0x1fff0, %ebx
  # DAP[0] = 0x10
  movb $0x10, (%ebx)
  # DAP[1] = 0
  movb $0x00, 1(%ebx)
  # DAP[2] = number of sectors (2 bytes)
  # We read 64 sectors at a time (since some platforms have
  # a limit to number of sectors read at a time). We need
  # to read a total of 1024 sectors to load all 512 KiB of
  # the kernel.
  movw $0x40, 2(%ebx)

  # Let %edi point to the starting sector we want to read from.
  movl 0x08(%ecx), %edi

  # Now let %ecx point to the location in memory to be written to.
  movl $0x20000, %ecx

copy_loop:
  # DAP[4] = sector:offset to buffer. %ecx points to where the data
  # should be written to.
  movl %ecx, 4(%ebx)
  # DAP[8] = start address of sectors to be read. %edi points to where
  # the data should be read from in the hard drive.
  movl %edi, 8(%ebx)
  movl $0x00, 12(%ebx)

  # Read the next 64 sectors from the drive and write
  # them to (%ecx).
  mov $0x42, %ah
  mov $0x1fff, %ds
  mov $0x0000, %si
  int $0x13

  # Increment %ecx (where the data is written to) and %edi
  # (where data is read from) by 64 sectors (32 KiB).
  addl $0x8000, %ecx
  addl $0x8000, %edi

  # Test if %ecx is at 0x20000 + 512 KiB. If yes, then we are done
  # copying kernal code and we start running the kernel. Otherwise,
  # we continue copying over kernel code
  cmpl $0xa0000, %ecx
  jne copy_loop

  # Start running the kernel.
  jmp $0x20000


next_part:
  # If %ecx is at the last partition already, then there are no
  # pintos partitions we can boot from, so we exit with an error
  # message.
  cmpl $0x201ee, %ecx
  je no_boot_part

  # Add 0x10 to %ecx so it points to the next partition table.
  add $0x10, %ecx
  jmp check_part

next_hd:
  # Increment %dl to check the next hard drive
  inc %dl
  jmp check_mbr

no_boot_part:
  call puts
  .string "no bootable partition in boot drive. aborting."
  int $0x18

unable_to_boot:
  call puts
  .string "could not find pintos OS. aborting\r"
  int $0x18

# Use 0x20000 (where the kernel will eventually go) as a temporary buffer
# for storing what we read from the hard disk.

	call puts
	.string "\r\rTODO:  IMPLEMENT THE BOOTLOADER\r"

	# Stop the processor.
	hlt

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
