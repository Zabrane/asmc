
include windows.inc
include gdiplus.inc
include tchar.inc

    .code

WndProc proc hWnd:HWND, message:UINT, wParam:WPARAM, lParam:LPARAM

  local ps:PAINTSTRUCT

    .switch edx

    .case WM_PAINT

        BeginPaint(rcx, &ps)
        OnPaint(rax)
        EndPaint(hWnd, &ps)
        .endc

    .case WM_DESTROY
        PostQuitMessage(0)
        .endc
    .case WM_CHAR
        .gotosw(WM_DESTROY) .if r8d == VK_ESCAPE
        .endc
    .default
        .return DefWindowProc(rcx, edx, r8, r9)
    .endsw
    xor eax,eax
    ret

WndProc endp

_tWinMain proc hInstance:HINSTANCE, hPrevInstance:HINSTANCE, lpCmdLine:LPTSTR, nShowCmd:SINT

  local wc:WNDCLASSEX, msg:MSG, hwnd:HANDLE

    xor eax,eax
    mov wc.cbSize,          WNDCLASSEX
    mov wc.style,           CS_HREDRAW or CS_VREDRAW
    mov wc.cbClsExtra,      eax
    mov wc.cbWndExtra,      eax
    mov wc.hbrBackground,   COLOR_ACTIVEBORDER
    mov wc.lpszMenuName,    rax
    mov wc.hIcon,           rax
    mov wc.hIconSm,         rax
    mov wc.hInstance,       rcx
    mov wc.lpfnWndProc,     &WndProc
    mov wc.lpszClassName,   &@CStr(CLASSNAME)
    mov wc.hCursor,         LoadCursor(0, IDC_ARROW)

    .ifd RegisterClassEx(&wc)

        .if CreateWindowEx(0, CLASSNAME, "gdiplus.Graphics(" CLASSNAME ")", WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, 600,400, NULL, NULL, hInstance, 0)

            mov hwnd,rax

           ;; Initialize GDI+.
           .new gdiplus:GdiPlus()

            ShowWindow(hwnd, SW_SHOWNORMAL)
            UpdateWindow(hwnd)
            .while GetMessage(&msg, 0, 0, 0)
                TranslateMessage(&msg)
                DispatchMessage(&msg)
            .endw
            gdiplus.Release()
            mov rax,msg.wParam
        .endif
    .endif
    ret

_tWinMain endp

    end _tstart
