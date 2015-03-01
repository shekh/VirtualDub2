dynamicVar	macro	name
		@CatStr(dynamic_var_,name) = dynamic_vars
		dynamic_vars = dynamic_vars + 1
		endm

dynamicEntryPoint	macro	lab
		@CatStr(dynamic_entrypt_,%dynamic_entrypts) = lab - dynamic_start
		dynamic_entrypts = dynamic_entrypts + 1
		endm

dynamicEntry	macro	offs, statement, expr, filler
		local addend, multiplier, pos

		pos = @instr(1,<&statement&>,<#>)

		if pos
			@catstr(@catstr(@substr(<&statement&>,1,pos-1), filler), @substr(@catstr(<&statement&>,< >),pos+1))
		else
			statement
		endif

		lab textequ <&expr&>

		pos = @instr(1,%lab,<+>)
		if pos
			@CatStr(dynamic_source_add,%dynamic_index) = @substr(%lab,pos+1)
			lab textequ @substr(%lab,1,pos-1)
		else
			@CatStr(dynamic_source_add,%dynamic_index) = 0
		endif

		pos = @instr(,%lab,<*>)
		if pos
			@CatStr(dynamic_source_mult,%dynamic_index) = @substr(%lab,pos+1)
			lab textequ @substr(%lab,1,pos-1)
		else
			@CatStr(dynamic_source_mult,%dynamic_index) = 1
		endif

		@CatStr(dynamic_ptr,%dynamic_index) = ($-offs) - dynamic_start
		@CatStr(dynamic_source,%dynamic_index) = @CatStr(dynamic_var_,%lab)

		dynamic_index = dynamic_index + 1
		endm

;d8		macro	statement:req, expr:req
;		dynamicEntry 1, <&statement&>, <&expr&>, 12h
;		endm
;d16		macro	statement:req, expr:req
;		dynamicEntry 2, <&statement&>, <&expr&>, 1234h
;		endm
d32		macro	statement:req, expr:req
		dynamicEntry 4, <&statement&>, <&expr&>, 12345678h
		endm

dbr		macro	statement:req
		local before, after

		before = $
		d32<&statement&>, reloc_special
		after = $

		if after-before < 5
			.err dbr can only be used with long branches
		endif

		endm

dr32		macro	statement:req
		d32<statement>, reloc_special
		endm

dptr		macro	target:req
		d32<dd target>, reloc_special
		endm
		

dynamicStart	macro
		dynamic_index = 0
		dynamic_vars = 0
		dynamic_entrypts = 0
		dynamic_start = $
		dynamicVar reloc_special
		endm

dynamicEnd	macro
		dynamic_length = $-dynamic_start
		endm



dynamicDump	macro
		local i, src

		dd dynamic_start
		dd dynamic_length
		dw dynamic_entrypts
		dw dynamic_index

		i=0
		repeat dynamic_entrypts
			dd @CatStr(dynamic_entrypt_,%i)
			i=i+1y
		endm

		i=0
		REPEAT dynamic_index
			src = @CatStr(dynamic_source,%i)

			IF @CatStr(dynamic_source_mult,%i) NE 1
				src = src + 4000h
			ENDIF

			IF @CatStr(dynamic_source_add,%i) NE 0
				src = src + 8000h
			ENDIF

			dw src
			dw @CatStr(dynamic_ptr,%i)

			IF @CatStr(dynamic_source_add,%i) NE 0
				dd @CatStr(dynamic_source_add,%i)
			ENDIF

			IF @CatStr(dynamic_source_mult,%i) NE 1
				dd @CatStr(dynamic_source_mult,%i)
			ENDIF
			i = i+1
		endm
		endm
