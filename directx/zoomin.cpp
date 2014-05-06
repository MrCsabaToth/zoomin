/****************************************************************************/
/*                                                                          */
/*                         Microsoft Confidential                           */
/*                                                                          */
/*                 Copyright (c) Microsoft Corp.  1987-1996                 */
/*                           All Rights Reserved                            */
/*                                                                          */
/****************************************************************************/
/****************************** Module Header *******************************
* Module Name: zoomin.c
*
* Microsoft ZoomIn utility.  This tool magnifies a portion of the screen,
* allowing you to see things at a pixel level.
*
* History:
* 01/01/88                              Created.
* 01/01/92                              Ported to NT.
* 03/06/92                              Cleanup.
*
****************************************************************************/

#include "zoomin.h"
#include <ddraw.h>                      // we need directdraw functionality


CHAR szAppName[] = "ZoomIn";            // Aplication name.
HINSTANCE ghInst;                       // Instance handle.
HWND ghwndApp;                          // Main window handle.
HACCEL ghaccelTable;                    // Main accelerator table.
INT gnZoom = 4;                         // Zoom (magnification) factor.
HPALETTE ghpalPhysical;                 // Handle to the physical palette.
INT gcxScreenMax;                       // Width of the screen (less 1).
INT gcyScreenMax;                       // Height of the screen (less 1).
INT gcxZoomed;                          // Client width in zoomed pixels.
INT gcyZoomed;                          // Client height in zoomed pixels.
BOOL gfRefEnable = FALSE;               // TRUE if refresh is enabled.
BOOL gfGridEnabled = FALSE;             // TRUE if show grid is enabled.
INT gnRefInterval = 20;                 // Refresh interval in 10ths of seconds.
BOOL gfTracking = FALSE;                // TRUE if tracking is in progress.
POINT gptZoom = {100, 100};             // The center of the zoomed area.
HBRUSH gGridPatternBrush = NULL;        // Brush of a grid pattern DC
BOOL gfGridDuringTrack = FALSE;         // Draw grid during mouse tracking (FALSE faster)
BOOL gfDrawInvColorDots = TRUE;         // Grid dot's has inverse colors of the underlying dot, or simply draw black grid everywhere (FALSE faster)
BOOL gfVideoMemoryOp = TRUE;            // Draw into video memory pixel by pixel or do the whole bunch offscreen
BOOL gfUseDirectX = FALSE;              // Use DirectX (DirectDraw) for grid drawing

LPDIRECTDRAW                g_pDD = NULL;           // DirectDraw object
LPDIRECTDRAWSURFACE         g_pDDSPrimary = NULL;   // DirectDraw primary surface
LPDIRECTDRAWSURFACE         g_pDDSOff = NULL;       // Offscreen surface
BOOL                        g_bActive = FALSE;      // Is application active?
DDSURFACEDESC               ddsdo;
BYTE*                       ddsdop;



/************************************************************************
* WinMain
*
* Main entry point for the application.
*
* Arguments:
*
* History:
*
************************************************************************/

INT WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow)
{
    MSG msg;

    if (!InitInstance (hInst, nCmdShow))
        return FALSE;

    /*     * Polling messages from event queue
     */

    while (GetMessage (&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator (ghwndApp, ghaccelTable, &msg)) {
            TranslateMessage (&msg);
            DispatchMessage (&msg);
        }
    }

    return msg.wParam;
}



/************************************************************************
* InitInstance
*
* Instance initialization for the app.
*
* Arguments:
*
* History:
*
************************************************************************/

BOOL InitInstance (HINSTANCE hInst, INT cmdShow)
{
    WNDCLASS wc;
    INT dx;
    INT dy;
    DWORD flStyle;
    RECT rc;

    DDSURFACEDESC       ddsd;              // DirectDraw surface descriptor
    HRESULT             hRet;
    LPDIRECTDRAW        pDD;

    ghInst = hInst;

    /*
     * Register a class for the main application window.
     */
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = LoadIcon (hInst, "zoomin");
    wc.lpszMenuName   = MAKEINTRESOURCE (IDMENU_ZOOMIN);
    wc.lpszClassName  = szAppName;
    wc.hbrBackground  = (HBRUSH) GetStockObject (BLACK_BRUSH);
    wc.hInstance      = hInst;
    wc.style          = CS_BYTEALIGNCLIENT | CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc    = (WNDPROC) AppWndProc;
    wc.cbWndExtra     = 0;
    wc.cbClsExtra     = 0;

    if (!RegisterClass (&wc))
        return FALSE;

    if (!(ghaccelTable = LoadAccelerators (hInst, MAKEINTRESOURCE(IDACCEL_ZOOMIN))))
        return FALSE;

    if (!(ghpalPhysical = CreatePhysicalPalette ()))
        return FALSE;

    gcxScreenMax = GetSystemMetrics (SM_CXSCREEN) - 1;    gcyScreenMax = GetSystemMetrics (SM_CYSCREEN) - 1;

    flStyle = WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_THICKFRAME |
            WS_MINIMIZEBOX | WS_VSCROLL;
    dx = 44 * gnZoom;
    dy = 36 * gnZoom;

    SetRect (&rc, 0, 0, dx, dy);
    AdjustWindowRect (&rc, flStyle, TRUE);

    ghwndApp = CreateWindow (szAppName, szAppName, flStyle,
            CW_USEDEFAULT, 0, rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, hInst, NULL);

    if (!ghwndApp)
        return FALSE;

    ///////////////////////////////////////////////////////////////////////////
    // Create the main DirectDraw object
    ///////////////////////////////////////////////////////////////////////////
    hRet = DirectDrawCreate (NULL, &pDD, NULL);
    if (hRet != DD_OK)
        return FALSE;

    // Fetch DirectDraw4 interface
    hRet = pDD->QueryInterface (IID_IDirectDraw, (LPVOID *) &g_pDD);
    if (hRet != DD_OK)
        return FALSE;

    // Get normal mode
    hRet = g_pDD->SetCooperativeLevel (ghwndApp, DDSCL_NORMAL);
    if (hRet != DD_OK)
        return FALSE;

    // Create the primary surface with 1 back buffer
    ZeroMemory (&ddsd, sizeof (ddsd));
    ddsd.dwSize = sizeof (ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    ddsd.dwBackBufferCount = 1;
    hRet = g_pDD->CreateSurface (&ddsd, &g_pDDSPrimary, NULL);
    if (hRet != DD_OK)
		return FALSE;
// errorcode: result - 0x8876

    ShowWindow (ghwndApp, cmdShow);

    return TRUE;
}



/************************************************************************
* CreatePhysicalPalette
*
* Creates a palette for the app to use.  The palette references the
* physical palette, so that it can properly display images grabbed
* from palette managed apps.
*
* History:
*
************************************************************************/

HPALETTE CreatePhysicalPalette (VOID)
{
    PLOGPALETTE ppal;
    HPALETTE hpal = NULL;
    INT i;

    ppal = (PLOGPALETTE) LocalAlloc (LPTR,
            sizeof (LOGPALETTE) + sizeof (PALETTEENTRY) * NPAL);
    if (ppal) {
        ppal->palVersion = 0x300;
        ppal->palNumEntries = NPAL;

        for (i = 0; i < NPAL; i++) {
            ppal->palPalEntry[i].peFlags = (BYTE)PC_EXPLICIT;
            ppal->palPalEntry[i].peRed   = (BYTE)i;
            ppal->palPalEntry[i].peGreen = (BYTE)0;
            ppal->palPalEntry[i].peBlue  = (BYTE)0;
        }

        hpal = CreatePalette (ppal);
        LocalFree (ppal);
    }

    return hpal;
}



/************************************************************************
* AppWndProc
*
* Main window proc for the zoomin utility.
*
* Arguments:
*   Standard window proc args.
*
* History:
*
************************************************************************/

LONG APIENTRY AppWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HCURSOR hcurOld;

    switch (msg) {
        case WM_CREATE:
            SetScrollRange (hwnd, SB_VERT, MIN_ZOOM, MAX_ZOOM, FALSE);
            SetScrollPos (hwnd, SB_VERT, gnZoom, FALSE);
            break;

        case WM_TIMER:
            /*
             * Update on every timer message.  The cursor will be
             * flashed to the hourglash for some visual feedback
             * of when a snapshot is being taken.
             */
            hcurOld = SetCursor (LoadCursor (NULL, IDC_WAIT));
            DoTheZoomIn (NULL);
            SetCursor (hcurOld);
            break;

        case WM_PAINT:
            BeginPaint (hwnd, &ps);
            DoTheZoomIn (ps.hdc);
            EndPaint (hwnd, &ps);
            return 0L;

        case WM_SIZE:
            CalcZoomedSizeAndGridPattern ();
            break;

        case WM_LBUTTONDOWN:
            ((gptZoom).x = (SHORT) LOWORD (lParam), (gptZoom).y = (SHORT)HIWORD(lParam));
            ClientToScreen (hwnd, &gptZoom);
            DrawZoomRect ();
            DoTheZoomIn (NULL);

            SetCapture (hwnd);
            gfTracking = TRUE;

            break;

        case WM_MOUSEMOVE:
            if (gfTracking) {
                DrawZoomRect ();
                ((gptZoom).x = (SHORT) LOWORD (lParam), (gptZoom).y = (SHORT)HIWORD(lParam));
                ClientToScreen (hwnd, &gptZoom);
                DrawZoomRect ();
                DoTheZoomIn (NULL);
            }

            break;

        case WM_LBUTTONUP:
            if (gfTracking) {
                DrawZoomRect ();
                ReleaseCapture ();
                gfTracking = FALSE;
            }

            break;

        case WM_VSCROLL:
            switch (LOWORD (wParam)) {
                case SB_LINEDOWN:
                    gnZoom++;
                    break;

                case SB_LINEUP:
                    gnZoom--;
                    break;

                case SB_PAGEUP:
                    gnZoom -= 2;
                    break;

                case SB_PAGEDOWN:
                    gnZoom += 2;
                    break;

                case SB_THUMBPOSITION:
                case SB_THUMBTRACK:
                    gnZoom = HIWORD (wParam);
                    break;
            }

            gnZoom = BOUND (gnZoom, MIN_ZOOM, MAX_ZOOM);
            SetScrollPos (hwnd, SB_VERT, gnZoom, TRUE);
            CalcZoomedSizeAndGridPattern ();
            DoTheZoomIn (NULL);
            break;

        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                    MoveView ((INT) wParam, GetKeyState (VK_SHIFT) & 0x8000,
                            GetKeyState (VK_CONTROL) & 0x8000);
                    break;
            }

            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case MENU_EDIT_COPY:
                    CopyToClipboard ();
                    break;

                case MENU_EDIT_REFRESH:
                    DoTheZoomIn (NULL);
                    break;

                case MENU_OPTIONS_REFRESHRATE:
                    DialogBox (ghInst, (LPSTR) MAKEINTRESOURCE (DID_REFRESHRATE),
                            hwnd, (DLGPROC) RefreshRateDlgProc);

                    break;

                case MENU_OPTIONS_SHOWGRID:
					if (gfGridEnabled) {
						CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_SHOWGRID, MF_UNCHECKED);
						gfGridEnabled = FALSE;
					} else {
						CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_SHOWGRID, MF_CHECKED);
						gfGridEnabled = TRUE;
					}
                    break;

                case MENU_HELP_ABOUT:
                    DialogBox(ghInst, (LPSTR) MAKEINTRESOURCE (DID_ABOUT),
                            hwnd, (DLGPROC) AboutDlgProc);

                    break;

                default:
                     break;
            }

            break;

        case WM_CLOSE:
            if (ghpalPhysical)
                DeleteObject (ghpalPhysical);
			DestroyGridPatternObjects ();
			if (g_pDD != NULL)
			{
				if (g_pDDSPrimary != NULL)
				{
					g_pDDSPrimary->Release ();
					g_pDDSPrimary = NULL;
				}
				g_pDD->Release();
				g_pDD = NULL;
			}
            DestroyWindow (hwnd);

            break;

        case WM_DESTROY:
            PostQuitMessage (0);
            break;

        default:
            return DefWindowProc (hwnd, msg, wParam, lParam);
    }

    return 0L;
}



/************************************************************************
* CalcZoomedSizeAndGridPattern
*
* Calculates some globals.  This routine needs to be called any
* time that the size of the app or the zoom factor changes.
*
* History:
*
************************************************************************/

VOID CalcZoomedSizeAndGridPattern (VOID)
{
    RECT rc;
    DDSURFACEDESC       ddsd;              // DirectDraw surface descriptor
    HRESULT             hRet;

    GetClientRect(ghwndApp, &rc);

    gcxZoomed = (rc.right / gnZoom) + 1;
    gcyZoomed = (rc.bottom / gnZoom) + 1;

/*	if (gfGridEnabled && gnZoom > 3) {
		DWORD lastError;
		BITMAPINFO gBMInfo;
		HLOCAL gBMBitsMem;
		LPVOID glpBMBits;
		int scanlines;

		HDC hdcScreen = GetDC (NULL);
		HBITMAP gGridBM = CreateCompatibleBitmap (hdcScreen, gnZoom, gnZoom);
		HDC gGridMemDC = CreateCompatibleDC (hdcScreen);
		HBITMAP gOldBM = SelectObject (gGridMemDC, gGridBM);
		BOOL blt = BitBlt (gGridMemDC, 0, 0, gnZoom, gnZoom, NULL, 0, 0, BLACKNESS);
		COLORREF col = SetPixel (gGridMemDC, gnZoom - 1, gnZoom - 1, RGB (255, 255, 255));
		HBITMAP gTempGridBM = CreateCompatibleBitmap (hdcScreen, gnZoom, gnZoom);
		GetDIBits (gGridMemDC, gTempGridBM, 0, 0, NULL, &gBMInfo, DIB_RGB_COLORS);
		lastError = GetLastError ();
		gBMBitsMem = LocalAlloc (LMEM_MOVEABLE, gBMInfo.bmiHeader.biSizeImage);
		glpBMBits = LocalLock (gBMBitsMem);
		scanlines = GetDIBits (hdcScreen, gGridBM, 0, gnZoom, glpBMBits, &gBMInfo, DIB_RGB_COLORS);

		DestroyGridPatternObjects ();
		gGridPatternBrush = CreateDIBPatternBrushPt (glpBMBits, DIB_RGB_COLORS);

		LocalUnlock (gBMBitsMem);
		LocalFree (gBMBitsMem);
		SelectObject (gGridMemDC, gOldBM);
		DeleteDC (gGridMemDC);
		DeleteObject (gGridBM);
		ReleaseDC (NULL, hdcScreen);
	}*/

	if (gfGridEnabled && gnZoom > 3) {
		// Create the offscreen surfaces
		ZeroMemory (&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof (ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH /*| DDSD_PITCH*/;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		ddsd.dwHeight = gnZoom * gcyZoomed;
		ddsd.dwWidth = gnZoom * gcxZoomed;
//      ddsd.lPitch = ddsd.dwWidth * depth; // Optimized grid use this property !

		hRet = g_pDD->CreateSurface (&ddsd, &g_pDDSOff, NULL);
		if (hRet != DD_OK)
			return;
	}
}



/************************************************************************
* DestroyGridPatternObjects
*
* Destroy grid pattern objects.
*
* History:
*
************************************************************************/

VOID DestroyGridPatternObjects (VOID)
{
	if (gGridPatternBrush != NULL) {
		DeleteObject (gGridPatternBrush);
		gGridPatternBrush = NULL;
	}

    if (g_pDDSOff != NULL)
    {
        g_pDDSOff->Release();
        g_pDDSOff = NULL;
    }
}



/************************************************************************
* DoTheZoomIn
*
* Does the actual paint of the zoomed image.
*
* Arguments:
*   HDC hdc - If not NULL, this hdc will be used to paint with.
*             If NULL, a dc for the apps window will be obtained.
*
* History:
*
************************************************************************/

VOID DoTheZoomIn(
    HDC hdc)
{
    BOOL fRelease;
    HPALETTE hpalOld = NULL;
    HDC hdcScreen;
    INT x;
    INT y;
	INT i;
	INT j;
//	COLORREF pixelColor, invertColor;
//	HBITMAP hBM, hOldBM;
//	HDC hBMDC;
//	DWORD lastError;
//	BITMAPINFO gBMInfo;
//	HLOCAL gBMBitsMem;
//	LPVOID glpBMBits;
//	WORD* glpBMBytes;
//	int scanlines;
	LONG lPitch;
	DWORD bitDepth;
	WORD* offScreenWords;
	DWORD* offScreenDWords;
    HRESULT hRet;

    if (!hdc) {
        hdc = GetDC (ghwndApp);
        fRelease = TRUE;
    }
    else {
        fRelease = FALSE;
    }

    if (ghpalPhysical) {
        hpalOld = SelectPalette (hdc, ghpalPhysical, FALSE);
        RealizePalette (hdc);
    }

    /*
     * The point must not include areas outside the screen dimensions.
     */
    x = BOUND(gptZoom.x, gcxZoomed / 2, gcxScreenMax - (gcxZoomed / 2));
    y = BOUND(gptZoom.y, gcyZoomed / 2, gcyScreenMax - (gcyZoomed / 2));

    hdcScreen = GetDC (NULL);
    SetStretchBltMode (hdc, COLORONCOLOR);
	StretchBlt (hdc, 0, 0, gnZoom * gcxZoomed, gnZoom * gcyZoomed,
			hdcScreen, x - gcxZoomed / 2,
			y - gcyZoomed / 2, gcxZoomed, gcyZoomed, SRCCOPY);
	if (gfGridEnabled && gnZoom > 3) {

/*		HBRUSH gOldBrush = SelectObject (hdc, gGridPatternBrush);
		StretchBlt(hdc, 0, 0, gnZoom * gcxZoomed, gnZoom * gcyZoomed,
					NULL, 0, 0, 0, 0, PATINVERT);
		SelectObject (hdc, gOldBrush);*/
/*		for (i = 1; i < gcyZoomed; i++) {
			for (j = 1; j < gcxZoomed; j++) {
				pixelColor = GetPixel (hdc, j * gnZoom, i * gnZoom);*/
/*				invertColor = RGB (255 - GetRValue (pixelColor), 255 - GetGValue (pixelColor), 255 - GetBValue (pixelColor));*/
/*				if (pixelColor == RGB (255, 255, 255)) {
					invertColor = RGB (0, 0, 0);
				} else {
					invertColor = pixelColor;
				}
				SetPixel (hdc, j * gnZoom, i * gnZoom, invertColor);
				SetPixel (hdc, j * gnZoom, i * gnZoom, RGB (0, 0, 0));
			}
		}*/

/*		hBM = CreateCompatibleBitmap (hdc, gnZoom * gcxZoomed, gnZoom * gcyZoomed);
		memset (&gBMInfo, 0, sizeof (BITMAPINFO));
		ZeroMemory (&gBMInfo, sizeof (BITMAPINFO));
		gBMInfo.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
		gBMInfo.bmiHeader.biWidth = gnZoom * gcxZoomed;
		gBMInfo.bmiHeader.biHeight = gnZoom * gcyZoomed;
		gBMInfo.bmiHeader.biPlanes = 1;
		gBMInfo.bmiHeader.biBitCount = 16;
		gBMInfo.bmiHeader.biCompression = BI_RGB;
		scanlines = GetDIBits (hdc, hBM, 0, gnZoom * gcyZoomed, NULL, &gBMInfo, DIB_RGB_COLORS);
		gBMBitsMem = GlobalAlloc (0, sizeof (BITMAPINFOHEADER) + gBMInfo.bmiHeader.biSizeImage);
		glpBMBits = GlobalLock (gBMBitsMem);
		scanlines = GetDIBits (hdc, hBM, 0, gnZoom * gcyZoomed, glpBMBits, &gBMInfo, DIB_RGB_COLORS);
		glpBMBytes = (WORD*) glpBMBits;

		for (i = 1; i < gcyZoomed * gcxZoomed; i++) {
			*glpBMBytes = 0;
			glpBMBytes += gnZoom;
		}
		scanlines = SetDIBits (hdc, hBM, 0, gnZoom * gcyZoomed, glpBMBits, &gBMInfo, DIB_RGB_COLORS);
		hBMDC = CreateCompatibleDC (hdc);
		hOldBM = SelectObject (hBMDC, hBM);
		BitBlt (hdc, 0, 0, gnZoom * gcxZoomed, gnZoom * gcyZoomed, hBMDC, 0, 0, SRCCOPY);

		GlobalUnlock (glpBMBits);
		GlobalFree (gBMBitsMem);
		SelectObject (hBMDC, hOldBM);
		DeleteDC (hBMDC);
		DeleteObject (hBM);*/

		if (g_pDDSOff == NULL)
			CalcZoomedSizeAndGridPattern ();
		hRet = g_pDDSOff->BltFast (
			0, 0,                                   // destination upper left xy
			g_pDDSPrimary,                          // source surface
			NULL,                                   // entire surface (else &rcRect)
			DDBLTFAST_NOCOLORKEY                    // WAIT used only for test (you could also DDBLT_ASYNC)
		); /* DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY | DDBLTFAST_SRCCOLORKEY can also be used in special cases */

		ZeroMemory (&ddsdo, sizeof (ddsdo));
		ddsdo.dwSize = sizeof(ddsdo); // size of the first offscreen buffer's structure


		while ((hRet = (g_pDDSOff->Lock (NULL, &ddsdo, DDLOCK_WAIT, NULL))) == DDERR_WASSTILLDRAWING)
			Sleep (1);
		if (hRet == DD_OK)
		{
			ddsdop = (BYTE*) ddsdo.lpSurface; // screen pointer of offscreen surface
			lPitch = ddsdo.lPitch;
			bitDepth = ddsdo.ddpfPixelFormat.dwRGBBitCount;

			switch (bitDepth) {
				case 16:
					for (i = 0; i < gnZoom; i++) {
						offScreenWords = (WORD*) ddsdop;
						for (j = 0; j < gnZoom; i++) {
							*offScreenWords = 0;
							offScreenWords += gnZoom;
						}
						ddsdop += gnZoom * lPitch;
					}
					break;
				case 32:
					for (i = 0; i < gnZoom; i++) {
						offScreenDWords = (DWORD*) ddsdop;
						for (j = 0; j < gnZoom; i++) {
							*offScreenWords = 0;
							offScreenWords += gnZoom;
						}
						ddsdop += gnZoom * lPitch;
					}
					break;
				default:
					break;
			}

			g_pDDSOff->Unlock(NULL);
		} else {
			return;
		}

		hRet = g_pDDSPrimary->BltFast (
			0, 0,                                   // destination upper left xy
			g_pDDSOff,                              // source surface (an offscreen)
			NULL,                                   // entire surface (else &rcRect)
			DDBLTFAST_NOCOLORKEY                    // WAIT used only for test (you could also DDBLT_ASYNC)
		); /* DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY | DDBLTFAST_SRCCOLORKEY can also be used in special cases */
	}


    ReleaseDC (NULL, hdcScreen);

    if (hpalOld)
        SelectPalette (hdc, hpalOld, FALSE);

    if (fRelease)
        ReleaseDC (ghwndApp, hdc);
}



/************************************************************************
* MoveView
*
* This function moves the current view around.
*
* Arguments:
*   INT nDirectionCode - Direction to move.  Must be VK_UP, VK_DOWN,
*                        VK_LEFT or VK_RIGHT.
*   BOOL fFast         - TRUE if the move should jump a larger increment.
*                        If FALSE, the move is just one pixel.
*   BOOL fPeg          - If TRUE, the view will be pegged to the screen
*                        boundary in the specified direction.  This overides
*                        the fFast parameter.
*
* History:
*
************************************************************************/

VOID MoveView (
    INT nDirectionCode,
    BOOL fFast,
    BOOL fPeg)
{
    INT delta;

    if (fFast)
        delta = FASTDELTA;
    else
        delta = 1;

    switch (nDirectionCode) {
        case VK_UP:
            if (fPeg)
                gptZoom.y = gcyZoomed / 2;
            else
                gptZoom.y -= delta;

            gptZoom.y = BOUND (gptZoom.y, 0, gcyScreenMax);

            break;

        case VK_DOWN:
            if (fPeg)
                gptZoom.y = gcyScreenMax - (gcyZoomed / 2);
            else
                gptZoom.y += delta;

            gptZoom.y = BOUND (gptZoom.y, 0, gcyScreenMax);

            break;

        case VK_LEFT:
            if (fPeg)
                gptZoom.x = gcxZoomed / 2;
            else
                gptZoom.x -= delta;

            gptZoom.x = BOUND (gptZoom.x, 0, gcxScreenMax);

            break;

        case VK_RIGHT:
            if (fPeg)
                gptZoom.x = gcxScreenMax - (gcxZoomed / 2);
            else
                gptZoom.x += delta;

            gptZoom.x = BOUND (gptZoom.x, 0, gcxScreenMax);

            break;
    }

    DoTheZoomIn (NULL);
}



/************************************************************************
* DrawZoomRect
*
* This function draws the tracking rectangle.  The size and shape of
* the rectangle will be proportional to the size and shape of the
* app's client, and will be affected by the zoom factor as well.
*
* History:
*
************************************************************************/

VOID DrawZoomRect (VOID)
{
    HDC hdc;
    RECT rc;
    INT x;
    INT y;

    x = BOUND(gptZoom.x, gcxZoomed / 2, gcxScreenMax - (gcxZoomed / 2));
    y = BOUND(gptZoom.y, gcyZoomed / 2, gcyScreenMax - (gcyZoomed / 2));

    rc.left   = x - gcxZoomed / 2;
    rc.top    = y - gcyZoomed / 2;
    rc.right  = rc.left + gcxZoomed;
    rc.bottom = rc.top + gcyZoomed;

    InflateRect(&rc, 1, 1);

    hdc = GetDC (NULL);

    PatBlt(hdc, rc.left,    rc.top,     rc.right-rc.left, 1,  DSTINVERT);
    PatBlt(hdc, rc.left,    rc.bottom,  1, -(rc.bottom-rc.top),   DSTINVERT);
    PatBlt(hdc, rc.right-1, rc.top,     1,   rc.bottom-rc.top,   DSTINVERT);
    PatBlt(hdc, rc.right,   rc.bottom-1, -(rc.right-rc.left), 1, DSTINVERT);

    ReleaseDC (NULL, hdc);
}



/************************************************************************
* EnableRefresh
*
* This function turns on or off the auto-refresh feature.
*
* Arguments:
*   BOOL fEnable - TRUE to turn the refresh feature on, FALSE to
*                  turn it off.
*
* History:
*
************************************************************************/

VOID EnableRefresh (BOOL fEnable)
{
    if (fEnable) {
        /*
         * Already enabled.  Do nothing.
         */
        if (gfRefEnable)
            return;

        if (SetTimer (ghwndApp, IDTIMER_ZOOMIN, gnRefInterval * 100, NULL))
            gfRefEnable = TRUE;
    }
    else {
        /*
         * Not enabled yet.  Do nothing.
         */
        if (!gfRefEnable)
            return;

        KillTimer (ghwndApp, IDTIMER_ZOOMIN);
        gfRefEnable = FALSE;
    }
}



/************************************************************************
* CopyToClipboard
*
* This function copies the client area image of the app into the
* clipboard.
*
* History:
*
************************************************************************/

VOID CopyToClipboard (VOID)
{
    HDC hdcSrc;
    HDC hdcDst;
    RECT rc;
    HBITMAP hbm;

    if (OpenClipboard (ghwndApp)) {
        EmptyClipboard ();

        if (hdcSrc = GetDC (ghwndApp)) {
            GetClientRect (ghwndApp, &rc);
            if (hbm = CreateCompatibleBitmap (hdcSrc,
                    rc.right - rc.left, rc.bottom - rc.top)) {
                if (hdcDst = CreateCompatibleDC (hdcSrc)) {
                    /*
                     * Calculate the dimensions of the bitmap and
                     * convert them to tenths of a millimeter for
                     * setting the size with the SetBitmapDimensionEx
                     * call.  This allows programs like WinWord to
                     * retrieve the bitmap and know what size to
                     * display it as.
                     */
                    





SetBitmapDimensionEx (hbm, (DWORD) ((INT) (((DWORD) (rc.right - rc.left) * MM10PERINCH) / (DWORD) GetDeviceCaps (hdcSrc, LOGPIXELSX))), (DWORD) ((INT) (((DWORD) (rc.bottom - rc.top) * MM10PERINCH) / (DWORD) GetDeviceCaps (hdcSrc, LOGPIXELSY))), NULL);

                    SelectObject (hdcDst, hbm);
                    BitBlt (hdcDst, 0, 0,
                            rc.right - rc.left, rc.bottom - rc.top,
                            hdcSrc, rc.left, rc.top, SRCCOPY);
                    DeleteDC (hdcDst);
                    SetClipboardData (CF_BITMAP, hbm);
                }
                else {
                    DeleteObject (hbm);
                }
            }

            ReleaseDC (ghwndApp, hdcSrc);
        }

        CloseClipboard ();
    }
    else {
        MessageBeep(0);
    }
}



/************************************************************************
* AboutDlgProc
*
* This is the About Box dialog procedure.
*
* History:
*
************************************************************************/

LRESULT CALLBACK AboutDlgProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            EndDialog (hwnd, IDOK);
            return TRUE;

        default:
            return FALSE;
    }
}



/************************************************************************
* RefreshRateDlgProc
*
* This is the Refresh Rate dialog procedure.
*
* History:
*
************************************************************************/

LRESULT CALLBACK RefreshRateDlgProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BOOL fTranslated;

    switch (msg) {
        case WM_INITDIALOG:
            SendDlgItemMessage (hwnd, DID_REFRESHRATEINTERVAL, EM_LIMITTEXT,
                    3, 0L);
            SetDlgItemInt (hwnd, DID_REFRESHRATEINTERVAL, gnRefInterval, FALSE);
            CheckDlgButton (hwnd, DID_REFRESHRATEENABLE, gfRefEnable ? 1 : 0);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD (wParam)) {
                case IDOK:
                    gnRefInterval = GetDlgItemInt(hwnd, DID_REFRESHRATEINTERVAL,
                            &fTranslated, FALSE);

                    /*
                     * Stop any existing timers then start one with the
                     * new interval if requested to.
                     */
                    EnableRefresh (FALSE);
                    EnableRefresh (
                            IsDlgButtonChecked (hwnd, DID_REFRESHRATEENABLE));

                    EndDialog (hwnd, IDOK);
                    break;

                case IDCANCEL:
                    EndDialog (hwnd, IDCANCEL);
                    break;
            }

            break;
    }

    return FALSE;
}



/************************************************************************
* GridOptionsDlgProc
*
* This is the Grid Options dialog procedure.
*
* History:
*
************************************************************************/

LRESULT CALLBACK GridOptionsDlgProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_INITDIALOG:
            CheckDlgButton (hwnd, DID_REFRESHRATEENABLE, gfRefEnable ? 1 : 0);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD (wParam)) {
                case IDOK:
                    EndDialog (hwnd, IDOK);
                    break;

                case IDCANCEL:
                    EndDialog (hwnd, IDCANCEL);
                    break;
            }

            break;
    }

    return FALSE;
}
