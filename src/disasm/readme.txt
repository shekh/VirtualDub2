disasm - Disassembly module compiler for VirtualDub
Copyright (C) 2002 Avery Lee, All Rights Reserved

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


=============================================================================
Changelog
=============================================================================
V1.2      Released with VirtualDub 1.5.2

- Fixed bad PREFETCH opcodes.

V1.1      Released October 31, 2002 with VirtualDub 1.4.11

- Fixes several bad instruction rules, most notably all the floating-
  point instructions that took memory arguments that were crashing the
  disassembler.  Thanks to Glenn Maynard for noticing this.

- Compiler checks format strings for string/byte mismatches.

=============================================================================
Explanation
=============================================================================
When VirtualDub crashes, it takes a snapshot of the current stack and code
around EIP, and produces a crash dump that I can use to diagnose the cause
of the failure.  Part of this process is producing a disassembly.  IA-32
(x86) is non-trivial to parse, and the current version of VirtualDub
(1.4.10) requires a table to drive the disassembler.  disasm's job is to
compile that table.

The table itself is a list of rules, which are sequentially applied against
the code bytes.  When one matches, the rule's result string is expanded to
produce the disassembly for that instruction.  Rules may embed other sets
of rules as part of their pattern; this functionality avoids having to
replicate rules for the mod-reg-mem (modrm) and scale-index-base (sib) bytes.
Full documentation for the pattern matching and result languages is included
in the ia32.txt file.

There are two ways you can run disasm:
    disasm
    disasm <source.txt> <dst.bin>

In the first case, the source is assumed to be 'ia32.txt' and the destination
'ia32.bin'.  Disasm will read the source file and attempt to compile it.  If
the compilation fails, an error will be printed.  Otherwise, disasm will write
out the compiled module, and then attempt to test the module against some of
its own code.  If you have trouble compiling disasm, it may be due to
P4/Athlon instructions in test1(); you can safely delete the offending
instructions from this function as it is never executed.

Once the disassembler module (ia32.bin) is produced, you can do two things
with it.  The usual use is to feed it as input to the 1.1 version of mapconv
to fold it into the VirtualDub.vdi file, but you can also copy the ia32.bin
file to the VirtualDub directory as ia32.vdi.  VirtualDub looks for this
file on a crash and will use the disassembler table in it before using the
table in VirtualDub.vdi; this allows the table to be updated without having
to recompile the debug module.  You can also rename the .vdi file from a
newer version of VirtualDub to ia32.vdi to use it on an older version,
provided that the table format in the newer file is not incompatible.

VirtualDub has a special command-line option to test the disassembler:
    virtualdub /fsck

This will execute a rather strange instruction and manually trigger the
crash dialog.  Note that the disassembler in disasm.exe is slightly older
than the module in 1.4.10 -- the 1.4.10 disassembler fixes a couple of
cosmetic bugs (most notably symbol lookup syntax).  The table formats are
the same, however.

If you have any questions or comments, I'd like to hear them.  Let me know
if any instructions are missing or decoded incorrectly.

-- Avery Lee <phaeron@virtualdub.org>
   October 31, 2002
