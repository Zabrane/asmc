; DEFWINDOWPROC.ASM--
;
; Copyright (c) The Asmc Contributors. All rights reserved.
; Consult your license regarding permissions and restrictions.
;

include twindow.inc

    .code

    option proc:private

    assume rcx:window_t

GetItemRect proto hwnd:window_t
ContextRect proto hwnd:window_t

GetItemRect macro hwnd
    mov     rax,[hwnd].Prev
    movzx   eax,word ptr [rax].TWindow.rc
    add     eax,[hwnd].rc
    retm    <eax>
    endm

ContextRect macro hwnd
    mov     [hwnd].Context.rc,GetItemRect(hwnd)
    retm    <eax>
    endm

OnEnterIdle proc uses rcx hwnd:window_t

    Sleep(4)
    xor eax,eax
    ret

OnEnterIdle endp

Inside proc hwnd:window_t, pos:COORD

  local rc:TRect

    mov eax,[rcx].rc
    .if ( [rcx].Flags & W_CHILD )
        mov r10,[rcx].Prev
        add ax,word ptr [r10].TWindow.rc
    .endif
    mov rc,eax
    xor eax,eax
    mov dh,rc.x
    .if dl >= dh
        add dh,rc.col
        .if dl < dh
            shr edx,16
            mov dh,rc.y
            .if dl >= dh
                add dh,rc.row
                .if dl < dh
                    mov al,dl
                    sub al,rc.y
                    inc al
                .endif
            .endif
        .endif
    .endif
    ret

Inside endp

    assume rbx:window_t
    assume rsi:context_t

OnLButtonDown proc uses rsi rdi rbx rcx hwnd:window_t, uiMsg:uint_t, wParam:size_t, lParam:ptr

    .ifd Inside(rcx, r8d) == 0

        .return [rcx].PostQuit(0) .if !( [rcx].Flags & W_CHILD )
        .return TRUE
    .endif
    lea rsi,[rcx].Context
    .switch [rcx].Type
      .case T_NORMAL
        .if ( eax > 1 )
            mov rcx,[rcx].Child
            .return [rcx].Send(WM_LBUTTONDOWN, r8, r9) .if rcx
            .return TRUE
        .endif
        mov [rsi].State,1
        mov [rsi].Flags,0
        mov [rsi].rc.x,r8b
        sub r8b,[rcx].rc.x
        mov [rsi].x,r8b
        shr r8d,16
        mov [rsi].rc.y,r8b
        sub r8b,[rcx].rc.y
        mov [rsi].y,r8b
        [rcx].CursorOff()
        .endc .if !( [rcx].Flags & W_SHADE )
        mov [rsi].Flags,1
        [rcx].ClrShade()
        and [rcx].Flags,not W_SHADE
        .endc
      .case T_PUSHBUTTON
        mov [rsi].State,1
        ContextRect(rcx)
        movzx ebx,al
        movzx edi,ah
        [rcx].CPutChar( ebx, edi, 1, ' ' )
        add bl,[rcx].rc.col
        dec bl
        [rcx].CPutChar( ebx, edi, 2, ' ' )
        movzx edx,[rcx].Context.rc.x
        inc edx
        inc edi
        [rcx].CPutChar( edx, edi, [rsi].rc.col, ' ' )
        [rcx].SetFocus( [rcx].Index )
        .endc
      .case T_RADIOBUTTON
      .case T_CHECKBOX
        [rcx].SetFocus( [rcx].Index )
        .endc
      .case T_XCELL
        [rcx].SetFocus([rcx].Index)
        .endc
      .case T_EDIT
      .case T_MENU
      .case T_XHTML
      .case T_MOUSE
      .case T_SCROLLUP
      .case T_SCROLLDOWN
      .case T_TEXTBUTTON
        .endc
    .endsw
    xor eax,eax
    ret

OnLButtonDown endp

OnLButtonUp proc uses rsi rdi rbx rcx hwnd:window_t, uiMsg:uint_t, wParam:size_t, lParam:ptr

    lea rsi,[rcx].Context
    .switch [rcx].Type
      .case T_NORMAL
        .if ( [rsi].State == 0 )
            mov rcx,[rcx].Child
            .return [rcx].Send(edx, r8, r9) .if rcx
            .return 1
        .endif
        mov [rsi].State,0
        .if ( [rsi].Flags )
            or [rcx].Flags,W_SHADE
            [rcx].SetShade()
        .endif
        [rcx].SetFocus( [rcx].Index )
        .endc
      .case T_PUSHBUTTON
        .return 1 .if ( [rsi].State == 0 )
        mov [rsi].State,0
        ContextRect(rcx)
        movzx edi,ah
        movzx edx,al
        inc edx
        [rcx].CPutChar( edx, edi, 1, '�' )
        movzx edx,[rsi].rc.x
        inc edx
        inc edi
        [rcx].CPutChar( edx, edi, [rsi].rc.col, '�' )
        xor edx,edx
        test [rcx].Flags,O_DEXIT
        cmovz edx,[rcx].Index
        .return [rcx].PostQuit(edx)
      .case T_RADIOBUTTON
        .return 1 .if ( [rsi].State == 0 )
        mov [rsi].State,0
        .endc
      .case T_CHECKBOX
      .case T_XCELL
        ;[rcx].SetFocus( [rcx].Index )
        ;.endc
      .case T_EDIT
      .case T_MENU
      .case T_XHTML
      .case T_MOUSE
      .case T_SCROLLUP
      .case T_SCROLLDOWN
      .case T_TEXTBUTTON
        .return 1
    .endsw
    xor eax,eax
    ret

OnLButtonUp endp


OnMouseMove proc uses rsi hwnd:window_t, uiMsg:uint_t, wParam:size_t, lParam:ptr

    lea rsi,[rcx].Context
    xor eax,eax
    .return .if [rsi].State == 0
    .return .if [rcx].Flags & W_CHILD
    movzx edx,[rsi].rc.y
    shl edx,16
    mov dl,[rsi].rc.x
    .return .if edx == eax
    movsx eax,[rsi].x
    movzx edx,r8b
    .if edx >= eax
        sub edx,eax
    .else
        xor edx,edx
    .endif
    shr r8d,16
    movsx eax,[rsi].y
    .if r8d >= eax
        sub r8d,eax
    .else
        xor r8d,r8d
    .endif
    [rcx].Move(edx, r8d)
    xor eax,eax
    ret

OnMouseMove endp

    assume rsi:nothing


OnSetFocus proc uses rsi rdi rbx rcx hwnd:window_t

    [rcx].CursorGet()
    [rcx].CursorOn()

    .switch [rcx].Type
      .case T_PUSHBUTTON
        [rcx].CursorOff()
        ContextRect(rcx)
        movzx edi,ah
        movzx edx,al
        mov ebx,' '
        .if [rcx].Context.State == 0
            mov ebx,0x10
        .endif
        [rcx].CPutChar(edx, edi, 1, ebx)
        .if [rcx].Context.State == 0
            mov ebx,0x11
        .endif
        movzx edx,[rcx].Context.rc.x
        add dl,[rcx].Context.rc.col
        dec dl
        [rcx].CPutChar(edx, edi, 1, ebx)
        .endc
      .case T_RADIOBUTTON
      .case T_CHECKBOX
        ContextRect(rcx)
        movzx edx,al
        movzx eax,ah
        inc edx
        [rcx].CursorMove(edx, eax)
        .endc
      .case T_XCELL
        mov edx,[rcx].rc
        mov rax,[rcx].Prev
        add dx,word ptr [rax].TWindow.rc
        mov [rcx].Window,[rcx].Open(edx, 0)
        .endc .if !rax
        mov rcx,rax
        [rcx].Read(&[rcx].rc, [rcx].Window)
        or [rcx].Flags,W_VISIBLE
        mov rdx,[rcx].Color
        mov al,[rdx+BG_INVERSE]
        shr al,4
        movzx edx,[rcx].rc.x
        movzx r8d,[rcx].rc.y
        movzx r9d,[rcx].rc.col
        [rcx].CPutBackground(edx, r8d, r9d, al)
        .endc
      .case T_EDIT
      .case T_MENU
      .case T_XHTML
      .case T_MOUSE
      .case T_SCROLLUP
      .case T_SCROLLDOWN
      .case T_TEXTBUTTON
        .endc
    .endsw

    xor eax,eax
    ret

OnSetFocus endp


OnKillFocus proc uses rsi rdi rbx rcx hwnd:window_t

    [rcx].CursorSet()

    .switch [rcx].Type
      .case T_PUSHBUTTON
        ContextRect(rcx)
        movzx edi,ah
        movzx esi,al
        movzx edx,[rcx].rc.col
        lea rdx,[rdx+rsi-1]
        [rcx].CPutChar(edx, edi, 1, ' ')
        [rcx].CPutChar(esi, edi, 1, ' ')
        .endc
      .case T_RADIOBUTTON
      .case T_CHECKBOX
        .endc
      .case T_XCELL
        mov rax,[rcx].Window
        .endc .if !rax
        mov edx,[rcx].rc
        mov r10,[rcx].Prev
        add dx,word ptr [r10].TWindow.rc
        mov rbx,rcx
        mov rcx,rax
        mov [rcx].rc,edx
        [rcx].Release()
        xor eax,eax
        mov [rbx].Window,rax
        .endc
      .case T_EDIT
      .case T_MENU
      .case T_XHTML
      .case T_MOUSE
      .case T_SCROLLUP
      .case T_SCROLLDOWN
      .case T_TEXTBUTTON
        .endc
    .endsw

    xor eax,eax
    ret

OnKillFocus endp


NextItem proc uses rcx hwnd:window_t

    .return .if ![rcx].GetFocus()
    mov rcx,[rax].TWindow.Child
    .if rcx == NULL
        mov rcx,[rax].TWindow.Prev
        mov rcx,[rcx].Child
        .return .if !rcx
    .endif
    [rcx].SetFocus([rcx].Index)
    ret

NextItem endp


PrevItem proc uses rbx rcx hwnd:window_t

    test [rcx].Flags,W_CHILD
    cmovnz rcx,[rcx].Prev
    mov eax,[rcx].Index
    .for ( rbx = [rcx].Child : rbx : rcx = rbx, rbx = [rbx].Child )
        .break .if ( eax == [rbx].Index )
    .endf
    .if rbx
        .if !( [rcx].Flags & W_CHILD )
            .for : [rcx].Child : rcx = [rcx].Child
            .endf
        .endif
        [rcx].SetFocus([rcx].Index)
    .endif
    ret

PrevItem endp


ItemRight proc uses rcx hwnd:window_t

    .if [rcx].GetFocus()
        mov rcx,rax
        mov edx,[rcx].rc
        .while 1
            .for ( rax = [rcx].Child : rax : rax = [rax].TWindow.Child )
                .break .if !( [rax].TWindow.Flags & O_STATE )
            .endf
            .break .if !rax
            mov rcx,rax
            .if ( dl < [rcx].rc.x && dh == [rcx].rc.y )
                [rcx].SetFocus([rcx].Index)
                .return 0
            .endif
        .endw
    .endif
    mov eax,1
    ret

ItemRight endp


ItemLeft proc uses rcx hwnd:window_t

    .if [rcx].GetFocus()
        mov rcx,rax
        mov edx,[rcx].rc
        .while 1
            xor eax,eax
            mov r11,[rcx].Prev
            mov r11,[r11].TWindow.Child
            .for ( : r11 && rcx != r11 : r11 = [r11].TWindow.Child )
                .if !( [r11].TWindow.Flags & O_STATE )
                    mov rax,r11
                .endif
            .endf
            .break .if !rax
            mov rcx,rax
            .if ( dl > [rcx].rc.x && dh == [rcx].rc.y )
                [rcx].SetFocus([rcx].Index)
                .return 0
            .endif
        .endw
    .endif
    mov eax,1
    ret

ItemLeft endp


OnChar proc uses rbx rcx hwnd:window_t, uiMsg:uint_t, wParam:size_t, lParam:ptr

    .if ( [rcx].Flags & W_CHILD )
        mov rax,[rcx].Prev
        mov eax,[rax].TWindow.Index
        .if ( eax == [rcx].Index )
            .switch [rcx].Type
            .case T_EDIT
                .return 1
            .case T_RADIOBUTTON
                .if r8d == VK_SPACE
                    mov rcx,[rcx].Prev
                    .for ( rcx = [rcx].Child : rcx : rcx = [rcx].Child )
                        .if [rcx].Flags & O_RADIO
                            GetItemRect(rcx)
                            movzx edx,al
                            movzx eax,ah
                            inc edx
                            and [rcx].Flags,not O_RADIO
                            [rcx].CPutChar(edx, eax, 1, ' ')
                            .break
                        .endif
                    .endf
                    GetItemRect(hwnd)
                    movzx edx,al
                    movzx eax,ah
                    inc edx
                    or  [rcx].Flags,O_RADIO
                    [rcx].CPutChar(edx, eax, 1, 7)
                    .return 0
                .endif
                .endc
            .case T_CHECKBOX
                .if r8d == VK_SPACE
                    GetItemRect(rcx)
                    movzx edx,al
                    movzx eax,ah
                    inc edx
                    xor [rcx].Flags,O_CHECK
                    mov r8d,' '
                    .if [rcx].Flags & O_CHECK
                        mov r8d,'x'
                    .endif
                    [rcx].CPutChar(edx, eax, 1, r8d)
                    .return 0
                .endif
                .endc
            .case T_PUSHBUTTON
            .case T_XCELL
            .case T_MENU
            .case T_XHTML
            .case T_MOUSE
            .case T_SCROLLUP
            .case T_SCROLLDOWN
            .case T_TEXTBUTTON
                .endc
            .endsw
            .if r8d == VK_RETURN
                xor     edx,edx
                test    [rcx].Flags,O_DEXIT
                cmovz   edx,eax
                .return [rcx].PostQuit(edx)
            .endif
        .endif
        .return 1
    .endif
    .switch r8d
      .case VK_UP     : .return PrevItem(rcx)
      .case VK_DOWN
      .case VK_TAB    : .return NextItem(rcx)
      .case VK_LEFT   : .return ItemLeft(rcx)
      .case VK_RIGHT  : .return ItemRight(rcx)
      .case VK_ESCAPE : .return [rcx].PostQuit(0)
    .endsw
    mov rcx,[rcx].Child
    .return [rcx].Send(edx, r8, r9) .if rcx
    ret

OnChar endp


OnSysChar proc uses rcx hwnd:window_t, uiMsg:uint_t, wParam:size_t, lParam:ptr

    .if ( [rcx].Flags & W_CHILD )
        .if ( r8d == [rcx].SysKey )
            [rcx].SetFocus([rcx].Index)
            .return 0
        .endif
    .else
        mov rcx,[rcx].Child
        .return [rcx].Send(edx, r8, r9) .if rcx
    .endif
    mov eax,1
    ret

OnSysChar endp


    option proc:public


TWindow::GetFocus proc uses rcx

    test [rcx].Flags,W_CHILD
    cmovnz rcx,[rcx].Prev
    mov eax,[rcx].Index
    .for ( rcx = [rcx].Child : rcx : rcx = [rcx].Child )
        .return rcx .if ( eax == [rcx].Index )
    .endf
    xor eax,eax
    ret

TWindow::GetFocus endp


TWindow::SetFocus proc uses rbx id:uint_t

    mov rbx,rcx
    .if [rcx].GetFocus()
        mov rcx,rax
        [rcx].Send(WM_KILLFOCUS, 0, 0)
        mov rcx,rbx
    .endif
    test [rcx].Flags,W_CHILD
    cmovnz rcx,[rcx].Prev
    mov [rcx].Index,id
    .if [rcx].GetFocus()
        mov rcx,rax
        [rcx].Send(WM_SETFOCUS, 0, 0)
    .endif
    mov rcx,rbx
    xor eax,eax
    ret

TWindow::SetFocus endp


TWindow::GetItem proc uses rcx id:uint_t

    test  [rcx].Flags,W_CHILD
    cmovnz rcx,[rcx].Prev
    .for ( rcx = [rcx].Child : rcx : rcx = [rcx].Child )
        .return rcx .if ( edx == [rcx].Index )
    .endf
    xor eax,eax
    ret

TWindow::GetItem endp


TWindow::DefWindowProc proc uiMsg:uint_t, wParam:size_t, lParam:ptr

    mov eax,1
    .switch pascal edx
      .case WM_ENTERIDLE:   OnEnterIdle(rcx)
      .case WM_SETFOCUS:    OnSetFocus(rcx)
      .case WM_KILLFOCUS:   OnKillFocus(rcx)
      .case WM_LBUTTONDOWN: OnLButtonDown(rcx, edx, r8, r9)
      .case WM_LBUTTONUP:   OnLButtonUp(rcx, edx, r8, r9)
      .case WM_MOUSEMOVE:   OnMouseMove(rcx, edx, r8, r9)
      .case WM_SYSCHAR:     OnSysChar(rcx, edx, r8, r9)
      .case WM_CHAR:        OnChar(rcx, edx, r8, r9)
    .endsw
    ret

TWindow::DefWindowProc endp

    end
