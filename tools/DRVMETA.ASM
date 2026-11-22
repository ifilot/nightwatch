; SPDX-License-Identifier: GPL-3.0-only
; Demo/test drive metadata. No ISA hardware is accessed.
.8086
.model medium,c
.code
previous dd 0
previous_dos dd 0
number dw 4
online dw 1
queries dw 0
public drv_install,drv_fake,drv_calls
public drv_dos_install,drv_dos_fake
drv_dos_install proc far
    push bp
    mov bp,sp
    mov ax,[bp+6]
    mov word ptr cs:previous_dos,ax
    mov ax,[bp+8]
    mov word ptr cs:previous_dos+2,ax
    pop bp
    ret
drv_dos_install endp
; Classic DOSBox supplies incomplete floppy device parameters. Supply known
; 1.44 MB / 360 KB metadata for the mounted demo floppy images.
drv_dos_fake proc far
    cmp ax,0440dh
    jne dos_chain
    cmp cx,00860h
    jne dos_chain
    cmp bl,1
    je floppy
    cmp bl,2
    jne dos_chain
floppy:
    mov al,bl
    push bx
    mov bx,dx
    mov byte ptr [bx+1],0
    cmp al,1
    jne floppy_done
    mov byte ptr [bx+1],7
    mov word ptr [bx+4],80
    mov word ptr [bx+7],512
    mov word ptr [bx+20],18
    mov word ptr [bx+22],2
floppy_done:
    pop bx
    xor ax,ax
    push bp
    mov bp,sp
    and word ptr ss:[bp+6],0fffeh
    pop bp
    iret
dos_chain:
    jmp dword ptr cs:previous_dos
drv_dos_fake endp
drv_install proc far
    push bp
    mov bp,sp
    mov ax,[bp+6]
    mov word ptr cs:previous,ax
    mov ax,[bp+8]
    mov word ptr cs:previous+2,ax
    mov ax,[bp+10]
    mov cs:number,ax
    mov ax,[bp+12]
    mov cs:online,ax
    mov cs:queries,0
    pop bp
    ret
drv_install endp
drv_fake proc far
    cmp ax,0d74fh
    jne chain
    ; Only answer discovery, never Slot-otter control requests.
    cmp bx,0
    jne chain
    cmp dx,0
    jne chain
    inc cs:queries
    mov ax,04f54h
    mov bx,0524fh
    mov cx,cs:number
    mov dx,cs:online
    mov bp,3
    ; int86 reads carry from the interrupt frame after IRET.
    push bp
    mov bp,sp
    and word ptr ss:[bp+6],0fffeh
    pop bp
    iret
chain:
    jmp dword ptr cs:previous
drv_fake endp
drv_calls proc far
    mov ax,cs:queries
    ret
drv_calls endp
end
