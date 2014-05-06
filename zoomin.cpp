/****************************************************************************/
/*																			*/
/*						 Microsoft Confidential								*/
/*																			*/
/*				 Copyright (c) Microsoft Corp.  1987-1996					*/
/*						   All Rights Reserved								*/
/*																			*/
/****************************************************************************/
/****************************** Module Header *******************************
* Module Name: zoomin.c
*
* Microsoft ZoomIn utility.  This tool magnifies a portion of the screen,
* allowing you to see things at a pixel level.
*
* History:
* @ Microsoft
* 01/01/88								Created.
* 01/01/92								Ported to NT.
* 03/06/92								Cleanup.
* @ Graphisoft R&D Rt by Csaba Tóth
* during 2002							Various grid options
* during 2003							Bug fixes for grid options
* 2004.06.28							Multi monitor capability: get out BOUND,
*										StretchBlt works well on WinNT and WinXP, when the destination DC and the
*										source DC isn't on the same device (monitor). Probably does not work with Win9x
*
****************************************************************************/

#include "zoomin.h"


CHAR szAppName[] = "ZoomIn";			// Aplication name.
HINSTANCE ghInst;						// Instance handle.
HWND ghwndApp;							// Main window handle.
HACCEL ghaccelTable;					// Main accelerator table.
INT gnZoom = 4;							// Zoom (magnification) factor.
HPALETTE ghpalPhysical;					// Handle to the physical palette.
INT gcxScreenMax;						// Width of the screen (less 1).
INT gcyScreenMax;						// Height of the screen (less 1).
INT gcxZoomed;							// Client width in zoomed pixels.
INT gcyZoomed;							// Client height in zoomed pixels.
BOOL gfRefEnable = FALSE;				// TRUE if refresh is enabled.
BOOL gfGridEnabled = TRUE;				// TRUE if show grid is enabled.
BOOL gfLBtnUpIntCaptureEnabled = TRUE;	// TRUE if the release up of the left mouse button interrupts the capture, or other words:
										// should you continously press left button, or the second button interrupts the capture
INT gnRefInterval = 20;					// Refresh interval in 10ths of seconds.
BOOL gfTracking = FALSE;				// TRUE if tracking is in progress.
POINT gptZoom = {100, 100};				// The center of the zoomed area.
HBRUSH gGridPatternBrush = NULL;		// Brush of a grid pattern DC
BOOL gfGridDuringTrack = FALSE;			// Draw grid during mouse tracking (FALSE faster)
BOOL gfLeftBtnGridShot = TRUE;			// Left mouse button during tracking draws grid or switches gfGridDuringTrack
BOOL gfDrawInvColorDots = TRUE;			// Grid dot's has inverse colors of the underlying dot, or simply draw black grid everywhere (FALSE faster)
BOOL gfTrueInvColors = TRUE;			// Really calculate the inverse (FALSE faster)
BOOL gfVideoMemoryOp = TRUE;			// Draw into video memory pixel by pixel or do the whole bunch offscreen
BOOL gfUseDirectX = FALSE;				// Use DirectX (DirectDraw) for grid drawing
BOOL gfDrawLines = FALSE;				// True if grid consist of lines, or else only dot grid id drawn



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

	/*	 * Polling messages from event queue
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

	ghInst = hInst;

	/*
	 * Register a class for the main application window.
	 */
	wc.hCursor			= LoadCursor (NULL, IDC_ARROW);
	wc.hIcon			= LoadIcon (hInst, "zoomin");
	wc.lpszMenuName		= MAKEINTRESOURCE (IDMENU_ZOOMIN);
	wc.lpszClassName	= szAppName;
	wc.hbrBackground	= (HBRUSH) GetStockObject (BLACK_BRUSH);
	wc.hInstance		= hInst;
	wc.style			= CS_BYTEALIGNCLIENT | CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc		= (WNDPROC) AppWndProc;
	wc.cbWndExtra		= 0;
	wc.cbClsExtra		= 0;

	if (!RegisterClass (&wc))
		return FALSE;

	if (!(ghaccelTable = LoadAccelerators (hInst, MAKEINTRESOURCE(IDACCEL_ZOOMIN))))
		return FALSE;

	if (!(ghpalPhysical = CreatePhysicalPalette ()))
		return FALSE;

	gcxScreenMax = GetSystemMetrics (SM_CXSCREEN) - 1;	gcyScreenMax = GetSystemMetrics (SM_CYSCREEN) - 1;

	flStyle = WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_VSCROLL;
	dx = 44 * gnZoom;
	dy = 36 * gnZoom;

	SetRect (&rc, 0, 0, dx, dy);
	AdjustWindowRect (&rc, flStyle, TRUE);

	ghwndApp = CreateWindow (szAppName, szAppName, flStyle,
							 CW_USEDEFAULT, 0, rc.right - rc.left, rc.bottom - rc.top,
							 NULL, NULL, hInst, NULL);

	if (!ghwndApp)
		return FALSE;

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

	ppal = (PLOGPALETTE) LocalAlloc (LPTR, sizeof (LOGPALETTE) + sizeof (PALETTEENTRY) * NPAL);
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
			if (gfLBtnUpIntCaptureEnabled)
				CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_LBTNIRCAPTURE, MF_CHECKED);
			else
				CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_LBTNIRCAPTURE, MF_UNCHECKED);
			if (gfGridEnabled)
				CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_SHOWGRID, MF_CHECKED);
			else
				CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_SHOWGRID, MF_UNCHECKED);
			break;

		case WM_TIMER:
			/*
			 * Update on every timer message.  The cursor will be
			 * flashed to the hourglash for some visual feedback
			 * of when a snapshot is being taken.
			 */
			hcurOld = SetCursor (LoadCursor (NULL, IDC_WAIT));
			DoTheZoomIn (NULL, FALSE);
			SetCursor (hcurOld);
			break;

		case WM_PAINT:
			BeginPaint (hwnd, &ps);
			DoTheZoomIn (ps.hdc, FALSE);
			EndPaint (hwnd, &ps);
			return 0L;

		case WM_SIZE:
			CalcZoomedSizeAndGridPattern ();
			break;

		case WM_LBUTTONDOWN:
			if (gfLBtnUpIntCaptureEnabled || (!gfLBtnUpIntCaptureEnabled && !gfTracking)) {
				((gptZoom).x = (SHORT) LOWORD (lParam), (gptZoom).y = (SHORT)HIWORD(lParam));
				ClientToScreen (hwnd, &gptZoom);
				DrawZoomRect ();
				DoTheZoomIn (NULL, FALSE);
				SetCapture (hwnd);
				gfTracking = TRUE;
			} else if (!gfLBtnUpIntCaptureEnabled && gfTracking) {
				DrawZoomRect ();
				ReleaseCapture ();
				if (!gfGridDuringTrack)
					DoTheZoomIn (NULL, TRUE);
				gfTracking = FALSE;
			}

			break;

		case WM_MOUSEMOVE:
			if (gfTracking) {
				DrawZoomRect ();
				((gptZoom).x = (SHORT) LOWORD (lParam), (gptZoom).y = (SHORT)HIWORD(lParam));
				ClientToScreen (hwnd, &gptZoom);
				DrawZoomRect ();
				DoTheZoomIn (NULL, FALSE);
			}

			break;

		case WM_LBUTTONUP:
			if (gfTracking && gfLBtnUpIntCaptureEnabled) {
				DrawZoomRect ();
				ReleaseCapture ();
				if (!gfGridDuringTrack)
					DoTheZoomIn (NULL, TRUE);
				gfTracking = FALSE;
			}

			break;

		case WM_RBUTTONDOWN:
			if (gfTracking && gfGridEnabled) {
				if (gfLeftBtnGridShot) {
					DoTheZoomIn (NULL, TRUE);
				} else {
					gfGridDuringTrack = !gfGridDuringTrack;
					DoTheZoomIn (NULL, gfGridDuringTrack);
				}
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
			DoTheZoomIn (NULL, FALSE);
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

		case WM_DISPLAYCHANGE:
			// TODO: For multi monitor systems
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case MENU_EDIT_COPY:
					CopyToClipboard ();
					break;

				case MENU_EDIT_REFRESH:
					DoTheZoomIn (NULL, TRUE);
					break;

				case MENU_OPTIONS_REFRESHRATE:
					DialogBox (ghInst, (LPSTR) MAKEINTRESOURCE (DID_REFRESHRATE),
							   hwnd, (DLGPROC) RefreshRateDlgProc);

					break;

				case MENU_OPTIONS_LBTNIRCAPTURE:
					if (gfLBtnUpIntCaptureEnabled) {
						CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_LBTNIRCAPTURE, MF_UNCHECKED);
						gfLBtnUpIntCaptureEnabled = FALSE;
					} else {
						CheckMenuItem (GetMenu (hwnd), MENU_OPTIONS_LBTNIRCAPTURE, MF_CHECKED);
						gfLBtnUpIntCaptureEnabled = TRUE;
					}
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

				case MENU_OPTIONS_GRIDOPTIONS:
					DialogBox (ghInst, (LPSTR) MAKEINTRESOURCE (DID_GRIDOPTIONS),
							   hwnd, (DLGPROC) GridOptionsDlgProc);

					break;

				case MENU_HELP_ABOUT:
					DialogBox (ghInst, (LPSTR) MAKEINTRESOURCE (DID_ABOUT),
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
}



/************************************************************************
* DoTheZoomIn
*
* Does the actual paint of the zoomed image.
*
* Arguments:
*   HDC hdc - If not NULL, this hdc will be used to paint with.
*			 If NULL, a dc for the apps window will be obtained.
*
* History:
*
************************************************************************/

VOID DoTheZoomIn (HDC hdc, BOOL leftButtonReleased)
{
	BOOL fRelease;
	HPALETTE hpalOld = NULL;
	HDC hdcScreen;
	INT x;
	INT y;
	INT i;
	INT j;
	COLORREF pixelColor, invertColor;
//	HBITMAP hBM, hOldBM;
//	HDC hBMDC;
//	DWORD lastError;
//	BITMAPINFO gBMInfo;
//	HLOCAL gBMBitsMem;
//	LPVOID glpBMBits;
//	WORD* glpBMBytes;
//	int scanlines;

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
//	x = BOUND (gptZoom.x, gcxZoomed / 2, gcxScreenMax - (gcxZoomed / 2));
//	y = BOUND (gptZoom.y, gcyZoomed / 2, gcyScreenMax - (gcyZoomed / 2));
	x = gptZoom.x;
	y = gptZoom.y;

	hdcScreen = GetDC (NULL);
	SetStretchBltMode (hdc, COLORONCOLOR);
	StretchBlt (hdc, 0, 0, gnZoom * gcxZoomed, gnZoom * gcyZoomed,
				hdcScreen, x - gcxZoomed / 2,
				y - gcyZoomed / 2, gcxZoomed, gcyZoomed, SRCCOPY);
	if (gfGridEnabled && gnZoom > 3 && (gfGridDuringTrack || (!gfGridDuringTrack && leftButtonReleased))) {
		if (!gfDrawLines) {
			for (i = 1; i < gcyZoomed; i++) {
				for (j = 1; j < gcxZoomed; j++) {
					if (gfDrawInvColorDots) {
						pixelColor = GetPixel (hdc, j * gnZoom, i * gnZoom);
						if (gfTrueInvColors) {
							invertColor = RGB (255 - GetRValue (pixelColor), 255 - GetGValue (pixelColor), 255 - GetBValue (pixelColor));
						} else {
							if (pixelColor == RGB (0, 0, 0)) {
								invertColor = RGB (255, 255, 255);
							} else {
								invertColor = RGB (0, 0, 0);
							}
						}
						SetPixel (hdc, j * gnZoom, i * gnZoom, invertColor);
					} else {
						SetPixel (hdc, j * gnZoom, i * gnZoom, RGB (0, 0, 0));
					}
				}
			}
		} else {
			for (i = 1; i < gcyZoomed; i++) {
				MoveToEx (hdc, 0, i * gnZoom, NULL);
				LineTo (hdc, gcxZoomed * gnZoom - 1, i * gnZoom);
			}
			for (j = 1; j < gcxZoomed; j++) {
				MoveToEx (hdc, j * gnZoom, 0, NULL);
				LineTo (hdc, j * gnZoom, gcyZoomed * gnZoom - 1);
			}
		}

/*		HBRUSH gOldBrush = SelectObject (hdc, gGridPatternBrush);
		StretchBlt(hdc, 0, 0, gnZoom * gcxZoomed, gnZoom * gcyZoomed,
				   NULL, 0, 0, 0, 0, PATINVERT);
		SelectObject (hdc, gOldBrush);*/

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
*						VK_LEFT or VK_RIGHT.
*   BOOL fFast		 - TRUE if the move should jump a larger increment.
*						If FALSE, the move is just one pixel.
*   BOOL fPeg		  - If TRUE, the view will be pegged to the screen
*						boundary in the specified direction.  This overides
*						the fFast parameter.
*
* History:
*
************************************************************************/

VOID MoveView (INT nDirectionCode, BOOL fFast, BOOL fPeg)
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

//			gptZoom.y = BOUND (gptZoom.y, 0, gcyScreenMax);

			break;

		case VK_DOWN:
			if (fPeg)
				gptZoom.y = gcyScreenMax - (gcyZoomed / 2);
			else
				gptZoom.y += delta;

//			gptZoom.y = BOUND (gptZoom.y, 0, gcyScreenMax);

			break;

		case VK_LEFT:
			if (fPeg)
				gptZoom.x = gcxZoomed / 2;
			else
				gptZoom.x -= delta;

//			gptZoom.x = BOUND (gptZoom.x, 0, gcxScreenMax);

			break;

		case VK_RIGHT:
			if (fPeg)
				gptZoom.x = gcxScreenMax - (gcxZoomed / 2);
			else
				gptZoom.x += delta;

//			gptZoom.x = BOUND (gptZoom.x, 0, gcxScreenMax);

			break;
	}

	DoTheZoomIn (NULL, FALSE);
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

//	x = BOUND (gptZoom.x, gcxZoomed / 2, gcxScreenMax - (gcxZoomed / 2));
//	y = BOUND (gptZoom.y, gcyZoomed / 2, gcyScreenMax - (gcyZoomed / 2));
	x = gptZoom.x;
	y = gptZoom.y;

	rc.left   = x - gcxZoomed / 2;
	rc.top	= y - gcyZoomed / 2;
	rc.right  = rc.left + gcxZoomed;
	rc.bottom = rc.top + gcyZoomed;

	InflateRect(&rc, 1, 1);

	hdc = GetDC (NULL);

	PatBlt(hdc, rc.left,	rc.top,			rc.right-rc.left,		1,						DSTINVERT);
	PatBlt(hdc, rc.left,	rc.bottom,		1,						-(rc.bottom-rc.top),	DSTINVERT);
	PatBlt(hdc, rc.right-1, rc.top,			1,						rc.bottom-rc.top,		DSTINVERT);
	PatBlt(hdc, rc.right,   rc.bottom-1,	-(rc.right-rc.left),	1,						DSTINVERT);

	ReleaseDC (NULL, hdc);
}



/************************************************************************
* EnableRefresh
*
* This function turns on or off the auto-refresh feature.
*
* Arguments:
*   BOOL fEnable - TRUE to turn the refresh feature on, FALSE to
*				  turn it off.
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
			if (hbm = CreateCompatibleBitmap (hdcSrc, rc.right - rc.left, rc.bottom - rc.top)) {
				if (hdcDst = CreateCompatibleDC (hdcSrc)) {
					/*
					 * Calculate the dimensions of the bitmap and
					 * convert them to tenths of a millimeter for
					 * setting the size with the SetBitmapDimensionEx
					 * call.  This allows programs like WinWord to
					 * retrieve the bitmap and know what size to
					 * display it as.
					 */
					SetBitmapDimensionEx (
						hbm,
						(DWORD) ((INT) (((DWORD) (rc.right - rc.left) * MM10PERINCH) / (DWORD) GetDeviceCaps (hdcSrc, LOGPIXELSX))),
						(DWORD) ((INT) (((DWORD) (rc.bottom - rc.top) * MM10PERINCH) / (DWORD) GetDeviceCaps (hdcSrc, LOGPIXELSY))),
						NULL
					);

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
			SendDlgItemMessage (hwnd, DID_REFRESHRATEINTERVAL, EM_LIMITTEXT, 3, 0L);
			SetDlgItemInt (hwnd, DID_REFRESHRATEINTERVAL, gnRefInterval, FALSE);
			CheckDlgButton (hwnd, DID_REFRESHRATEENABLE, gfRefEnable ? 1 : 0);
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD (wParam)) {
				case IDOK:
					gnRefInterval = GetDlgItemInt(hwnd, DID_REFRESHRATEINTERVAL, &fTranslated, FALSE);

					/*
					 * Stop any existing timers then start one with the
					 * new interval if requested to.
					 */
					EnableRefresh (FALSE);
					EnableRefresh (IsDlgButtonChecked (hwnd, DID_REFRESHRATEENABLE));

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
			CheckDlgButton (hwnd, IDC_DRAWTRACK,	gfGridDuringTrack ? 1 : 0);
			CheckDlgButton (hwnd, IDC_LEFTMOUSEBTN,	gfLeftBtnGridShot ? 1 : 0);
			CheckDlgButton (hwnd, IDC_INVERSECOLOR,	gfDrawInvColorDots ? 1 : 0);
			CheckDlgButton (hwnd, IDC_DRAWLINES,	gfDrawLines ? 1 : 0);
			CheckDlgButton (hwnd, IDC_VIDEOMEM,		gfVideoMemoryOp ? 1 : 0);
			CheckDlgButton (hwnd, IDC_USEDDRAW,		gfUseDirectX ? 1 : 0);
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD (wParam)) {
				case IDOK:
					gfGridDuringTrack	= IsDlgButtonChecked (hwnd, IDC_DRAWTRACK);
					gfLeftBtnGridShot	= IsDlgButtonChecked (hwnd, IDC_LEFTMOUSEBTN);
					gfDrawInvColorDots	= IsDlgButtonChecked (hwnd, IDC_INVERSECOLOR);
					gfDrawLines			= IsDlgButtonChecked (hwnd, IDC_DRAWLINES);
					gfVideoMemoryOp		= IsDlgButtonChecked (hwnd, IDC_VIDEOMEM);
					gfUseDirectX		= IsDlgButtonChecked (hwnd, IDC_USEDDRAW);
					EndDialog (hwnd, IDOK);
					break;

				case IDCANCEL:
					EndDialog (hwnd, IDCANCEL);
					break;

				case IDDEFAULTS:
					CheckDlgButton (hwnd, IDC_DRAWTRACK, 0);
					CheckDlgButton (hwnd, IDC_LEFTMOUSEBTN, 1);
					CheckDlgButton (hwnd, IDC_DRAWLINES, 0);
					CheckDlgButton (hwnd, IDC_INVERSECOLOR, 1);
					CheckDlgButton (hwnd, IDC_VIDEOMEM, 1);
					CheckDlgButton (hwnd, IDC_USEDDRAW, 0);
					break;
			}
			break;
	}

	return FALSE;
}
