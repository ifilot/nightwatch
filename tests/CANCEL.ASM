; SPDX-License-Identifier: GPL-3.0-only
; Test-only INT 21h observer: inject BIOS keys at the first copy-block write.
; Preserve all registers and chain directly to the original DOS handler.
.8086
.MODEL SMALL
.CODE
PUBLIC _cancel_hook, _cancel_set, _cancel_count
old_vector DD 0
injected DW 0
; Small-model C passes old vector offset/segment at BP+4/BP+6. Store in CS
; because an interrupt arrives with the interrupted program's DS, not ours.
_cancel_set PROC NEAR
    push bp
    mov bp,sp
    mov ax,[bp+4]
    mov WORD PTR cs:old_vector,ax
    mov ax,[bp+6]
    mov WORD PTR cs:old_vector+2,ax
    pop bp
    ret
_cancel_set ENDP
_cancel_count PROC NEAR
    mov ax,cs:injected
    ret
_cancel_count ENDP
; INT 21h AH=40h, CX=2048 or 16384 identifies a transfer-block write. Inject once
; without calling DOS recursively; tail-chain with the original entry flags.
_cancel_hook PROC FAR
    pushf
    cmp ah,40h
    jne chain
    cmp cx,2048
    je transfer
    cmp cx,16384
    jne chain
transfer:
    cmp WORD PTR cs:injected,0
    jne chain
    mov WORD PTR cs:injected,1
    push ax
    push bx
    push es
    mov ax,40h
    mov es,ax
; The fixture ensures room for three words. Queue unrelated A, Escape, then
; F10: Escape must be detected behind A while F10 remains for the main loop.
    mov bx,es:[1ch]
    mov WORD PTR es:[bx],1e61h
    call advance
    mov WORD PTR es:[bx],011bh
    call advance
    mov WORD PTR es:[bx],4400h
    call advance
    mov es:[1ch],bx
    pop es
    pop bx
    pop ax
chain:
    popf
    jmp DWORD PTR cs:old_vector
_cancel_hook ENDP
; Advance the IBM BIOS data-area ring offset, wrapping 003Eh to 001Eh.
advance PROC NEAR
    add bx,2
    cmp bx,3eh
    jb advanced
    mov bx,1eh
advanced:
    ret
advance ENDP
END
