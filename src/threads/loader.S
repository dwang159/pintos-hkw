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
	int $0x14			    # Destroys AX.

	call puts
	.string "PiLo\r"

####        ITERATE THROUGH ALL HARD DISKS IN THE SYSTEM, LOOKING FOR A
####        BOOTABLE PINTOS PARTITION TO LOAD AND START.
####
####        Note that there are only ~300 bytes available for the entire
####        Pintos bootloader, as Pintos commands are stored in the
####        region immediately before the partition table in the sector.
####        Sooo... be concise!

# We use 0x20000 as the location for the buffer when reading from the hard
# drives.
# When we want to provide a segement:offset pointer to the buffer, use:
#
# segment: 0x2000 (in %es)
# offset: 0x0
initialize_regs:
    # Move segment register into %es
    mov $0x2000, %cx
    mov %cx, %es
    # Begin looping through all hard drives.
    mov $0x80, %dl # DL = starting_harddrive_number
    mov $DAP, %si  # *SI = disk access packet
    jmp check_mbr

DAP:
    .byte 0x10        # DAP size

    .byte 0x00        # unused

    .word 0x1        # 1 sector to read

    .word 0x0000      # buffer offset is 0

    .word 0x2000      # buffer segment is 0x2000

    .long 0x0000      # LBA of sector (last 8 bytes)
    .long 0x0000      # is just 0
  
check_mbr:
    # Read the MBR sector from the drive
    mov $0x42, %ah  # 0x42 is the argument for Extended Read Sectors
    int $0x13       # Execute a Low Level Disk Service (extended read)
   
    # Check status of disk read. The carry flag (CF) is set if
    # there was an error.
    jnc read_successful
    # jc read_unsuccessful

read_unsuccessful:
    call puts
    .string "0" # Print '0' in case of disk read error
    int $0x18   # boot failure in

read_successful:
    # Check if MBR has boot flag. To do this, check if the
    # last two bytes of the MBR have values 0x55 and 0xAA.
    movw %es:0x1fe, %cx # move boot-sector signature into CX
    cmpw $0xaa55, %cx   # Check that it has the correct values
    je read_p_table     # If it's a bootable sector, read the part table
    # jne try_next_disk   Else move to the next hard drive

try_next_disk:
    # Increment %dl to check the next hard drive
    inc %dl
    jmp check_mbr

read_p_table:
    # Set %si to start of partition entries.
    mov $0x1be, %si
p_table_loop:
    # Iterate through all 4 partitions of this hard drive
    mov $0x80, %cl       # Move boot flag to CL
    cmpb %es:0(%si), %cl     # Check for a boot flag on this partition
    je type_check        # if bootable, check that it has a PINTOS type
    # jne next_partition # Otherwise, increment and test for end condition

next_partition:
    # If %esi is at the last partition already, then there are no
    # pintos partitions we can boot from, so we exit with an error
    # message.
    call puts
    .string "hi\r"
    add $0x10, %si    # Move to next ptable entry
    cmp $0x1ee, %si # Compare to end of ptable
    jle p_table_loop   # Keep going through them
    # jg no_boot_part 

no_boot_part:
    # We have iterated through the partition table with no luck, 
    # so quitting.
    call puts
    .string "1"
    int $0x18

type_check:
    # Check to see if the drive has Pintos type (0x20). If so, start
    # copying. Otherwise, move to next drive.
    movb $0x20, %al        # AL = Pintos type
    cmpb %es:0x4(%si), %al # Compare the 5th byte (off = 4) in partition 
                           # entry to the type
    jne read_p_table  # If the type is incorrect, keep looking for a part.
    # je copy kernel    # Else start moving the kernel from this partition

copy_kernel:
    call puts 
    .string "chose part"
    # %esi points to the start of the partition entry
    # %dl has the hard disk number
  
    movl 0x08(%si), %ebx # Save the LBA from the partition entry
    mov $DAP, %si        # *ESI = DAP

    # We read 64 sectors at a time (since some platforms have
    # a limit to number of sectors read at a time). We need
    # to read a total of 1024 sectors to load all 512 KiB of
    # the kernel.
    movw $0x40, 0x2(%si)        # Move size to DAP[2]

    # There are 512 bytes in a sector, and 512 KiB to copy 
    # Thus, there are 2048 sectors to copy.
    # Copying 64 at a time, this means that the loop will take 
    # 32 iterations
    movb 0x20, %al

    # Store the target offset in EDI
    movw $0x2000, %di
    movw $0x0000, 0x4(%si)
copy_loop:
    # Invariants: 
    #   AL is the loop counter
    #   EDI is the address to write to
    #   EBX holds the LBA to read the first sector from
    #   DS:SI points to the DAP
    #   AH holds the argument for an extended read (0x42)

    movw %di, 0x6(%si)   #Update target in DAP
    addw $0x0800, %di     # There are 0x100 bytes in each sector,
                          # and 0x40 sectors read so the target
                          # should increment by 0x4000

    movl %ebx, 0x8(%si)  # Change start of sector reads
    movl $0x0, 0xc(%si)  # Keep high bytes 0 still  
    add $0x40, %ebx       # 0x40 sectors read in each iteration,
                          # so increment the sector to start 
                          # reading from by 0x40.

    # Read the next 64 sectors from the drive and write
    # them to .
    int $0x13
    jc read_unsuccessful   # IF carry flag, wasn't able to copy
    
    dec %cx       # One more batch of sectors copied
    jnz copy_loop # End the loop if all 32 batches have been copied
    
    call puts
    .string "load successful\r"
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
