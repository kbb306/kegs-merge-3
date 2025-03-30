/****************************************************************/
/*    Apple IIgs emulator                                       */
/*    Copyright 1996 Kent Dickey                                */
/*                                                              */
/*    This code may not be used in a commercial product         */
/*    without prior written permission of the author.           */
/*                                                              */
/*    Windows Code by akilgard@yahoo.com                        */
/*    You may freely distribute this code.                      */ 
/*                                                              */
/****************************************************************/
#include "sim65816.h"
#include "video.h"
#include "videodriver.h"
#include "joystick.h"
#include "paddles.h"
#include "sound.h"

#ifdef HAVE_VIDEO_WIN32

//#define KEGS_DIRECTDRAW
//#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <winsock.h>
#include <commctrl.h>
#include <Commdlg.h>
#include <process.h>
#if defined(KEGS_DIRECTDRAW)
#include <ddraw.h>
#endif
#include <conio.h>
#include "../kegs_win32/resource.h"
#include "iwm.h"
#include "adb.h"
#include "dis.h"
#include "engine.h"

static void win_refresh_lines(HBITMAP xim, int start_line, int end_line, int left_pix, int right_pix);
static void win_redraw_border_sides_lines(int end_x, int width, int start_line, int end_line);
static void win_refresh_border_sides(void);
static void win_refresh_border_special(void);
static unsigned int __stdcall dev_video_init_ex(void *param); 
static int win_keysym_to_a2code(int keysym, int is_up);
static void win_refresh_image (RGBQUAD* palette,HDC hdc,HBITMAP hbm, int srcx, int srcy,
                               int destx,int desty, int width,int height);

static int    g_x_shift_control_state = 0;
static int    g_num_a2_keycodes = 0;
static int g_fullscreen=0;

#define STATUS_HEIGHT       ((BASE_MARGIN_TOP)+(A2_WINDOW_HEIGHT)+\
                            (BASE_MARGIN_BOTTOM)+\
                            (win_toolHeight.bottom-win_toolHeight.top)+\
                            (win_statHeight.bottom-win_statHeight.top)+\
                            ((win_stat_debug>0) ? MAX_STATUS_LINES*15:0)\
                            )

DWORD   dwchary;
// char    g_video_status_buf[MAX_STATUS_LINES][X_LINE_LENGTH + 1];

// Windows specific stuff
	 HWND hwndMain;

static HWND hwndHidden;
static HDC  hcdc;
static HDC hdcRef;
static RGBQUAD a2v_palette[256];
static RGBQUAD shires_palette[256];
static RGBQUAD *win_palette;
static RGBQUAD dummy_color;
static HBITMAP video_image_border_special;
static HBITMAP video_image_border_sides;
		int     win_pause=0;
static int     win_stat_debug=0;
static HMENU   win_menu=NULL;
static HWND    win_status=NULL;
static HWND    win_toolbar=NULL;
static RECT    win_rect_oldpos;
static RECT    win_toolHeight;
static RECT    win_statHeight;


#ifdef KEGS_DIRECTDRAW 
static LPDIRECTDRAW lpDD=NULL;
static LPDIRECTDRAWSURFACE primsurf=NULL;
static LPDIRECTDRAWSURFACE backsurf=NULL; 
static LPDIRECTDRAWCLIPPER lpClipper=NULL; 
#endif

#define VK_ENTER	0x3A		// Reusing undefined value!

static const int a2_key_to_wsym[][3] = {
    { 0x35,    VK_ESCAPE,    VK_F1 },
//    { 0x7a,    VK_F1,    0 },
//    { 0x7b,    VK_F2,    0 },
//    { 0x63,    VK_F3,    0 },
//    { 0x76,    VK_F4,    0 },
//    { 0x60,    VK_F5,    0 },
//    { 0x61,    VK_F6,    0 },
    { 0x7f,    VK_CANCEL, 0 },
    { 0x32,    0xc0, 0 },        /* Key number 18? */
    { 0x12,    '1', 0 },
    { 0x13,    '2', 0 },
    { 0x14,    '3', 0 },
    { 0x15,    '4', 0 },
    { 0x17,    '5', 0 },
    { 0x16,    '6', 0 },
    { 0x1a,    '7', 0 },
    { 0x1c,    '8', 0 },
    { 0x19,    '9', 0 },
    { 0x1d,    '0', 0 },
    { 0x1b,    0xbd, 0 },    /* Key minus */
    { 0x18,    0xbb, 0 },   /* Key equals */
    { 0x33,    VK_BACK, 0 },
//    { 0x72,    VK_INSERT, 0 },        /* Help? */
//    { 0x74,    VK_PRIOR, 0 },
//    { 0x47,    VK_NUMLOCK, 0 },    /* Clear */
    { 0x51,    VK_HOME, 0 },        /* Note VK_Home alias! */
    { 0x4b,    VK_DIVIDE, 0 },
    { 0x43,    VK_MULTIPLY, 0 },
    { 0x30,    VK_TAB, 0 },
    { 0x0c,    'Q' ,0 },
    { 0x0d,    'W' ,0 },
    { 0x0e,    'E' ,0 },
    { 0x0f,    'R' ,0 },
    { 0x11,    'T' ,0 },
    { 0x10,    'Y' ,0 },
    { 0x20,    'U' ,0 },
    { 0x22,    'I' ,0 },
    { 0x1f,    'O' ,0 },
    { 0x23,    'P' ,0 },
    { 0x21,    0xdb, 0 },    /* [{ */
    { 0x1e,    0xdd, 0 },    /* ]} */
    { 0x2a,    0xdc, 0 },    /* backslash, bar */
//    { 0x75,    VK_DELETE, 0 },
//    { 0x77,    VK_END, 0 },
//    { 0x79,    VK_PGDN, 0 },
    { 0x59,    VK_NUMPAD7, 0 },
    { 0x5b,    VK_NUMPAD8, 0 },
    { 0x5c,    VK_NUMPAD9, 0 },
    { 0x4e,    VK_SUBTRACT, 0 },
//    { 0x39,    VK_CAPITAL, 0 },
    { 0x00,    'A', 0 },
    { 0x01,    'S', 0 },
    { 0x02,    'D', 0 },
    { 0x03,    'F', 0 },
    { 0x05,    'G', 0 },
    { 0x04,    'H', 0 },
    { 0x26,    'J', 0 },
    { 0x28,    'K', 0 },
    { 0x25,    'L', 0 },
    { 0x29,    0xba, 0 },    /* semicolon */
    { 0x27,    0xde, 0 },    /* single quote */
    { 0x24,    VK_RETURN, 0 },
    { 0x56,    VK_NUMPAD4, 0},
    { 0x57,    VK_NUMPAD5, 0 },
    { 0x58,    VK_NUMPAD6, 0},
    { 0x45,    VK_ADD, 0 },
//    { 0x38,    VK_Shift_L, VK_Shift_R },
    { 0x06,    'Z',0 },
    { 0x07,    'X',0 },
    { 0x08,    'C',0 },
    { 0x09,    'V',0 },
    { 0x0b,    'B',0 },
    { 0x2d,    'N',0 },
    { 0x2e,    'M',0 },
    { 0x2b,    0xbc, 0 },    /* comma */
    { 0x2f,    0xbe, 0 },    /* dot */
    { 0x2c,    0xbf, 0 },
    { 0x3e,    VK_UP, 0 },
    { 0x53,    VK_NUMPAD1, 0 },
    { 0x54,    VK_NUMPAD2, 0 },
    { 0x55,    VK_NUMPAD3, 0 },
//    { 0x36,    VK_CONTROL_L, VK_CONTORL_R },
    { 0x3a,    0x5b, VK_F2 },         /* Option */
    { 0x37,    VK_MENU, VK_F3 },      /* Command */
    { 0x31,    ' ', 0 },
    { 0x3b,    VK_LEFT, 0 },
    { 0x3d,    VK_DOWN, 0 },
    { 0x3c,    VK_RIGHT, 0 },
    { 0x52,    VK_NUMPAD0, 0 },
    { 0x41,    VK_DECIMAL, VK_SEPARATOR },
    { 0x4c,    VK_ENTER, 0 },
    { -1, -1, -1 }
};

#ifdef KEGS_FULLINTERFACE
//
// read kegs config into disk config dialog
// entry. The Entry control ID are specially
// organized
//
int
read_disk_entry(HWND hDlg)
{
    char    buf[1024], *buf2;
    FILE    *fconf;
    char    *ptr;
    int    line, slot, drive;
    HWND hwnd;

    fconf = fopen(g_kegs_conf_name, "rt");
    if(fconf == 0) {
        return 1;
    }
    line = 0;
    while(1) {
        ptr = fgets(buf, 1024, fconf);
        if(ptr == 0) {
            /* done */
            break;
        }
        line++;
    
        if((buf[0] == 's') && (buf[2] == 'd')) {
           slot = (buf[1]- 0x30);
           drive = (buf[3] -0x30);
        
           if (drive>=1 && drive<=2 && slot>=5 && slot<=7) {
               hwnd = GetDlgItem(hDlg,10000+slot*10+drive );
               if (hwnd) {
				   /*
                   buf2[0]='\0';
                   sscanf(strstr(buf+4,"=")+1,"%s",buf2);
				   */
				   buf2 = buf+4;
				   while(*buf2==' ' || *buf2=='=') buf2++;
                   SetWindowText(hwnd,buf2);
               }
           }
        }
    };
    return 0;
}

int
write_disk_entry(HWND hDlg)
{
    char    buf[1024];
    FILE    *fconf;
    int    slot, drive;
    HWND hwnd;

    ki_printf ("Writing disk configuration to %s\n",g_kegs_conf_name);
    fconf = fopen(g_kegs_conf_name, "wt");
    if(fconf == 0) {
        return 1;
    }
    for (slot=1;slot<=7;slot++) {
        for (drive=1;drive<=2;drive++) {
            if (slot<5) continue;
            hwnd = GetDlgItem(hDlg,10000+slot*10+drive);
            if (!hwnd) continue;
            ZeroMemory(buf,1024);
            GetWindowText(hwnd,buf,1024);
            if (lstrlen(buf)>0) {
                fprintf(fconf,"s%dd%d = %s\n",slot,drive,buf);   
            } else {
                fprintf(fconf,"#s%dd%d = <nofile>\n",slot,drive);
            }
        }
    };
    fclose(fconf);
    return 0;
}

#endif // KEGS FULL INTERFACE


void win_update_modifier_state(int state)
{
    int    state_xor;
    int    is_up;

    #define CAPITAL  4  
    #define SHIFT    2
    #define CONTROL  1 

    state = state & (CONTROL | CAPITAL | SHIFT);
    state_xor = g_x_shift_control_state ^ state;
    is_up = 0;
    if(state_xor & CONTROL) {
        is_up = ((state & CONTROL) == 0);
        adb_physical_key_update(0x36, is_up);
    }
    if(state_xor & CAPITAL) {
        is_up = ((state & CAPITAL) == 0);
        adb_physical_key_update(0x39, is_up);
    }
    if(state_xor & SHIFT) {
        is_up = ((state & SHIFT) == 0);
        adb_physical_key_update(0x38, is_up);
    }

    g_x_shift_control_state = state;
}

// Use the way keys update_mouse works
// to get this to work
void win_warp_cursor(HWND hwnd)
{
    POINT p;
    p.x = X_A2_WINDOW_WIDTH/2;
    p.y = X_A2_WINDOW_HEIGHT/2;
    ClientToScreen(hwnd,&p);
    SetCursorPos(p.x,p.y);
}

void
video_warp_pointer_win32(void)
{
    if(g_warp_pointer) {
		RECT rect;
        SetCapture(hwndMain);
        ShowCursor(FALSE);
        GetWindowRect(hwndMain,&rect);
        ClipCursor(&rect);
        win_warp_cursor(hwndMain);
        ki_printf("Mouse Pointer grabbed\n");
    } else {
	    ClipCursor(NULL);
        ShowCursor(TRUE);
	    ReleaseCapture();
        ki_printf("Mouse Pointer released\n");
    }
}

int
win_toggle_mouse_cursor(HWND hwnd)
{
    g_warp_pointer = !g_warp_pointer;
    video_warp_pointer_win32();
	return g_warp_pointer;
}

void 
handle_vk_keys(UINT vk)
{
#ifdef KEGS_FULLINTERFACE
    int    style;
    RECT   wrect;
    int    adjx,adjy;
	RECT   rect;
#endif

    // Check the state for caps lock,shift and control
    // This checking must be done in the main window and not in the hidden
    // window.  Some configuration of windows doesn't work in the hidden
    // window
    win_update_modifier_state(
        ((GetKeyState(VK_CAPITAL)&0x1) ?1:0) << 2 | 
        ((GetKeyState(VK_SHIFT)&0x80)?1:0) << 1 | 
        ((GetKeyState(VK_CONTROL)&0x80)?1:0) << 0); 

    // Toggle fast disk
    if (vk == VK_F7) {
        g_fast_disk_emul = !g_fast_disk_emul;
        ki_printf("g_fast_disk_emul is now %d\n", g_fast_disk_emul);
    }

    // Handle mouse pointer display toggling
    if (vk == VK_F9 || vk == VK_F8) {
        win_toggle_mouse_cursor(hwndMain);
    }

#ifdef KEGS_FULLINTERFACE
    // Set full screen 
    if (vk == VK_F11) {
        g_fullscreen = !g_fullscreen;
        if (g_fullscreen) {
            GetWindowRect(hwndMain,&win_rect_oldpos);
            style=GetWindowLong(hwndMain,GWL_STYLE);
            style &= ~WS_CAPTION;
            SetWindowLong(hwndMain,GWL_STYLE,style);
            win_menu=GetMenu(hwndMain);
            SetMenu(hwndMain,NULL);
            ShowWindow(win_status,FALSE);
            ShowWindow(win_toolbar,FALSE);
           
            SetWindowPos(hwndMain,HWND_TOPMOST,-4,-4,
                         GetSystemMetrics(SM_CXSCREEN)+8,
                         GetSystemMetrics(SM_CYSCREEN)+8,
                         SWP_SHOWWINDOW);
           
            PatBlt(hdcRef,
                   -4,-4,
                   GetSystemMetrics(SM_CXSCREEN)+8,
                   GetSystemMetrics(SM_CYSCREEN)+8,
                   BLACKNESS
            );
            
            if (g_warp_pointer) {
        		GetWindowRect(hwndMain,&rect);
		        ClipCursor(&rect);
                win_warp_cursor(hwndMain);
	        }             
            a2_screen_buffer_changed = -1;
            g_full_refresh_needed = -1;
            g_border_sides_refresh_needed = 1;
            g_border_special_refresh_needed = 1;
            g_status_refresh_needed = 1;
            video_refresh_image_win32();
            video_redraw_status_lines_win32();
        } else {
            style=GetWindowLong(hwndMain,GWL_STYLE);
            style |= WS_CAPTION;
            SetWindowLong(hwndMain,GWL_STYLE,style);
            SetMenu(hwndMain,win_menu);
            ShowWindow(win_status,TRUE);
            ShowWindow(win_toolbar,TRUE);
            win_menu=NULL;
            GetClientRect(hwndMain,&rect);
            GetWindowRect(hwndMain,&wrect);
            adjx=(wrect.right-wrect.left)-(rect.right-rect.left);
            adjy=(wrect.bottom-wrect.top)-(rect.bottom-rect.top);
            SetWindowPos(hwndMain,HWND_NOTOPMOST,
                 win_rect_oldpos.left,win_rect_oldpos.top,
                 BASE_WINDOW_WIDTH+adjx,
                 //BASE_WINDOW_HEIGHT+adjy,
                 //BASE_MARGIN_TOP+A2_WINDOW_HEIGHT+BASE_MARGIN_BOTTOM+adjy+
                 //MAX_STATUS_LINES*15+48,
                 STATUS_HEIGHT+adjy,
                 SWP_SHOWWINDOW );
            ZeroMemory(&win_rect_oldpos,sizeof(RECT));
            if (g_warp_pointer) {
		        GetWindowRect(hwndMain,&rect);
		        ClipCursor(&rect);
	        }      
        }
    }
#endif
}

int
handle_keysym(UINT vk,BOOL fdown, int cRepeat,UINT flags)
{
    int    a2code;


    // Fixup the numlock & other keys
    if (!(flags & 0x100)) {
        switch(vk) {
        case VK_UP:
            vk=VK_NUMPAD8;
            break;
        case VK_LEFT:
            vk=VK_NUMPAD4;
            break;
        case VK_RIGHT:
            vk=VK_NUMPAD6;
            break;
        case VK_DOWN:
            vk=VK_NUMPAD2;
            break;
        case VK_HOME:
            vk=VK_NUMPAD7;
            break;
        case VK_PRIOR:
            vk=VK_NUMPAD9;
            break;
        case VK_END:
            vk=VK_NUMPAD1;
            break;
        case VK_NEXT:
            vk=VK_NUMPAD3;
            break;
        case VK_CLEAR:
            vk=VK_NUMPAD5;
            break;
        }
    } else {
        if (vk == VK_MENU) {
            vk=0x5b;
        } 
		else
		/*
		if (vk == VK_RETURN) {
            vk=VK_HOME;
        }
		else
		*/
		if (vk == VK_RETURN) 
            vk=VK_ENTER;
    }


    a2code = win_keysym_to_a2code(vk, !fdown);
    if (fdown) {
        //ki_printf ("Keysym=0x%x a2code=0x%x cRepeat=0x%x flag=0x%x\n",
        //        vk,a2code,cRepeat,flags);
    }
    if(a2code >= 0) {
        adb_physical_key_update(a2code, !fdown);
        return 0;    
    } 
    return -1;
}

int
win_keysym_to_a2code(int keysym, int is_up)
{
    int    i;

    if(keysym == 0) {
        return -1;
    }

    /* Look up Apple 2 keycode */
    for(i = g_num_a2_keycodes - 1; i >= 0; i--) {
        if((keysym == a2_key_to_wsym[i][1]) ||
                    (keysym == a2_key_to_wsym[i][2])) {

            vid_printf("Found keysym:%04x = a[%d] = %04x or %04x\n",
                (int)keysym, i, a2_key_to_wsym[i][1],
                a2_key_to_wsym[i][2]);

            return a2_key_to_wsym[i][0];
        }
    }

    return -1;
}

void
video_update_color_win32(int col_num, int a2_color)
{
    byte r,g,b;
    int palette;

    if(col_num >= 256 || col_num < 0) {
        halt_printf("update_color_array called: col: %03x\n", col_num);
        return;
    }

    r = ((a2_color >> 8) & 0xf)<<4;
    g = ((a2_color >> 4) & 0xf)<<4;
    b = ((a2_color) & 0xf)<<4;
	if (!g_installed_full_superhires_colormap)
	{
		a2v_palette[col_num].rgbBlue       = b;
		a2v_palette[col_num].rgbGreen      = g;
		a2v_palette[col_num].rgbRed        = r;
		a2v_palette[col_num].rgbReserved   = 0;
	}

    palette = col_num >> 4;

    shires_palette[col_num].rgbBlue       = b;
    shires_palette[col_num].rgbGreen      = g;
    shires_palette[col_num].rgbRed        = r;
    shires_palette[col_num].rgbReserved   = 0;

    g_full_refresh_needed = -1;
}

void
video_update_physical_colormap_win32(void)
{
    int    palette;
    int    full;
    int    i;
    byte r,g,b;
    int value;

    full = g_installed_full_superhires_colormap;

    if(!full)
	{
        palette = g_a2vid_palette << 4;
        for(i = 0; i < 16; i++) {
            value=lores_colors[i];

            b = (lores_colors[i%16]    & 0xf) << 4;
            g = (lores_colors[i%16]>>4 & 0xf) << 4;
            r = (lores_colors[i%16]>>8 & 0xf) << 4;

            a2v_palette[palette+i].rgbBlue       = b;
            a2v_palette[palette+i].rgbGreen      = g;
            a2v_palette[palette+i].rgbRed        = r;
            a2v_palette[palette+i].rgbReserved   = 0;
        }
    }

    if(full) {
        win_palette=(RGBQUAD *)&shires_palette;
    } else {
        win_palette=(RGBQUAD *)&a2v_palette;
    }
	

    a2_screen_buffer_changed = -1;
    g_full_refresh_needed = -1;
    g_border_sides_refresh_needed = 1;
    g_border_special_refresh_needed = 1;
    g_status_refresh_needed = 1;

}

void
video_shutdown_win32(void)
{
    win_pause=1;
    ki_printf ("Close Window.\n");

    // Wait for display thread to finish and stall
    // so we can free resources
    Sleep(300);  

    // Cleanup direct draw objects
    #if defined(KEGS_DIRECTDRAW)
    if (lpClipper)
        IDirectDrawClipper_Release(lpClipper);
    if (backsurf)
        IDirectDrawSurface_Release(backsurf);
    if (primsurf)
        IDirectDrawSurface_Release(primsurf);
    if (lpDD)
        IDirectDraw_Release(lpDD);
    #endif

    DeleteObject((HBITMAP)video_image_text[0]);
    video_image_text[0]=NULL;
    DeleteObject((HBITMAP)video_image_text[1]);
    video_image_text[1]=NULL;
    DeleteObject((HBITMAP)video_image_hires[0]);
    video_image_hires[0]=NULL;
    DeleteObject((HBITMAP)video_image_hires[1]);
    video_image_hires[1]=NULL;
    DeleteObject((HBITMAP)video_image_superhires);
    video_image_superhires=NULL;
    DeleteObject(video_image_border_special);
    video_image_border_special=NULL;
    DeleteObject(video_image_border_sides);
    video_image_border_sides=NULL;

    DeleteDC(hcdc);
    hcdc=NULL;

#ifdef KEGS_FULLINTERFACE
		hwndMain = NULL;
#endif

//    WSACleanup();
}

void win_centroid (int *centerx, int *centery) {

    RECT wrect,rect;
    int  adjx,adjy;

    GetClientRect(hwndMain,&rect);
    GetWindowRect(hwndMain,&wrect);
    adjx=(wrect.right-wrect.left)-(rect.right-rect.left);
    adjy=(wrect.bottom-wrect.top)-(rect.bottom-rect.top);

    if (wrect.right-wrect.left > BASE_WINDOW_WIDTH+adjx) {
        *centerx=((wrect.right-wrect.left)-(BASE_WINDOW_WIDTH+adjx));
        *centerx=(*centerx)/2;
    }

    if (wrect.bottom-wrect.top > STATUS_HEIGHT+adjy) {
        *centery=((wrect.bottom-wrect.top)-(STATUS_HEIGHT+adjy));
        *centery=(*centery)/2;
    }

}

static void
win_refresh_image (RGBQUAD* palette,HDC hdc,HBITMAP hbm, int srcx, int srcy,
                   int destx,int desty, int width,int height) {
    HBITMAP hbmOld;
    HBITMAP holdbm=NULL;
 #if defined(KEGS_DIRECTDRAW)
    HDC hdcBack;
    RECT  rcRectSrc,rect;
    int hr;
#endif
	    POINT p;

	int   centerx,centery;

    if (hwndMain == NULL)
        return;

    if (width==0 || height == 0) {
        return;
    }

    centerx=centery=0;
    win_centroid(&centerx,&centery);
    p.x = 0; p.y = 0;
    ClientToScreen(hwndMain, &p);
    hbmOld = SelectObject(hcdc,hbm);
    SetDIBColorTable(hcdc,0,256,palette);

    if (!g_fullscreen) {
        centery+=(win_toolHeight.bottom-win_toolHeight.top);
    }

    #if !defined(KEGS_DIRECTDRAW)
    BitBlt(hdc,destx+centerx,desty+centery,
           width,height,hcdc,srcx,srcy,SRCCOPY);
 
    #else 
    if ((hr=IDirectDrawSurface_GetDC(backsurf,&hdcBack)) != DD_OK) {
        ki_printf ("Error blitting surface (back_surf) %x\n",hr);
        my_exit(0);
        return;
    }

   
    BitBlt(hdcBack,destx+centerx,desty+centery,width,height,hcdc,srcx,srcy,
           SRCCOPY);
    IDirectDrawSurface_ReleaseDC(backsurf,hdcBack);

    SetRect(&rect,p.x+destx+centerx,p.y+desty+centery,
            p.x+destx+width+centerx,p.y+desty+height+centery);
    SetRect(&rcRectSrc,destx+centerx,desty+centery,
            destx+width+centerx,desty+height+centery);
    hr=IDirectDrawSurface_Restore(primsurf);
    if (hr != DD_OK) {
        ki_printf ("Error restoring surface (prim_surf) %x\n",hr);
        my_exit(0);
    }

    hr=IDirectDrawSurface_Blt(primsurf, 
                              &rect,backsurf,&rcRectSrc,DDBLT_WAIT, NULL);
    if (hr != DD_OK) {
        ki_printf ("Error blitting surface (prim_surf) %x\n",hr);
        my_exit(0);
    }

    #endif 
    
    SelectObject(hcdc,hbmOld);
    holdbm=hbm;

}

BOOL A2W_OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)  {
    RECT rect;
    RECT wrect;
    int  adjx,adjy;
    ZeroMemory(&win_rect_oldpos,sizeof(RECT));
    GetClientRect(hwnd,&rect);
    GetWindowRect(hwnd,&wrect);
    adjx=(wrect.right-wrect.left)-(rect.right-rect.left);
    adjy=(wrect.bottom-wrect.top)-(rect.bottom-rect.top);
    SetWindowPos(hwnd,NULL,0,0,BASE_WINDOW_WIDTH+adjx,
                 //BASE_WINDOW_HEIGHT+adjy,
                 BASE_MARGIN_TOP+A2_WINDOW_HEIGHT+BASE_MARGIN_BOTTOM+adjy+48,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW );
    return TRUE;
}

void A2W_OnKeyChar(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags) {
        handle_keysym(vk,fDown,cRepeat,flags);
}

void A2W_OnDestroy(HWND hwnd) {
	/*
    ReleaseDC(hwndMain,hdcRef);
    hdcRef=NULL;
    hwndMain=NULL;
    PostQuitMessage(0);
	*/
	set_halt(HALT_WANTTOQUIT);

}

BOOL A2W_OnEraseBkgnd(HWND hwnd, HDC hdc)  {
    RECT rect;
    RECT wrect;
    int  adjx,adjy;
    int  centerx,centery;
    if (video_image_superhires == NULL) {
        return FALSE;   
    }

    if (!g_fullscreen) {
        SendMessage(win_toolbar,TB_AUTOSIZE,0,0);
        SendMessage(win_status,WM_SIZE,0,0);
    }

    GetClientRect(hwnd,&rect);
    GetWindowRect(hwnd,&wrect);
    adjx=(wrect.right-wrect.left)-(rect.right-rect.left);
    adjy=(wrect.bottom-wrect.top)-(rect.bottom-rect.top);

    centerx=centery=0;
    if (wrect.right-wrect.left > BASE_WINDOW_WIDTH+adjx) {
        centerx=((wrect.right-wrect.left)-(BASE_WINDOW_WIDTH+adjx))/2;
    }
    if (wrect.bottom-wrect.top > STATUS_HEIGHT+adjy) {
        centery=((wrect.bottom-wrect.top)-(STATUS_HEIGHT+adjy))/2;
    }

    if (wrect.right-wrect.left > BASE_WINDOW_WIDTH+adjx) {
        PatBlt(hdcRef,
               BASE_WINDOW_WIDTH+centerx,0,
               (wrect.right-wrect.left)-BASE_WINDOW_WIDTH-adjx, 
               (wrect.bottom-wrect.top),
               BLACKNESS
              );
        PatBlt(hdcRef,
               0,0,
               BASE_WINDOW_WIDTH, 
               (wrect.bottom-wrect.top),
               BLACKNESS
              );
    }

    if (wrect.bottom-wrect.top > (STATUS_HEIGHT+adjy)) {
        PatBlt(hdcRef,
               centerx, centery+(BASE_MARGIN_TOP)+(A2_WINDOW_HEIGHT)+
               (BASE_MARGIN_BOTTOM),
               BASE_WINDOW_WIDTH+adjx,
               wrect.bottom-
               (centery+(BASE_MARGIN_TOP)+(A2_WINDOW_HEIGHT)+
               (BASE_MARGIN_BOTTOM)),
               BLACKNESS
              );
        PatBlt(hdcRef,
               centerx,0,
               BASE_WINDOW_WIDTH+adjx,
               centery,
               BLACKNESS
              );

    }

    a2_screen_buffer_changed = -1;
    g_full_refresh_needed = -1;
    g_border_sides_refresh_needed = 1;
    g_border_special_refresh_needed = 1;
    g_status_refresh_needed = 1;
    video_refresh_image_win32();
    video_redraw_status_lines_win32();
    return TRUE;
}

#ifdef KEGS_FULLINTERFACE

// Message handler for about box.
LRESULT CALLBACK A2W_Dlg_About_Dialog(HWND hDlg, UINT message, WPARAM wParam, 
                                      LPARAM lParam)
{
    RECT rc, rcDlg,rcOwner;
    HWND hwndOwner;

    switch (message) {
    case WM_INITDIALOG:
          if ((hwndOwner = GetParent(hDlg)) == NULL) {
            hwndOwner = GetDesktopWindow();
          }

          GetWindowRect(hwndOwner,&rcOwner);
          GetWindowRect(hDlg,&rcDlg);
          CopyRect(&rc,&rcOwner);
          OffsetRect(&rcDlg,-rcDlg.left,-rcDlg.top);
          OffsetRect(&rc,-rc.left,-rc.top);
          OffsetRect(&rc,-rcDlg.right,-rcDlg.bottom);

          SetWindowPos(hDlg,hwndMain,rcOwner.left+(rc.right/2),
                       rcOwner.top+(rc.bottom/2), 0,0,
                       SWP_SHOWWINDOW | SWP_NOSIZE );
          return TRUE;
    case WM_COMMAND:
       switch (LOWORD(wParam)) {
       case IDOK:
           EndDialog(hDlg, LOWORD(wParam));
           return TRUE;
       case IDCANCEL:
           EndDialog(hDlg, LOWORD(wParam));
           return FALSE;
       }
    }
    return FALSE;
}

// Message handler for disk config.
LRESULT CALLBACK A2W_Dlg_Disk(HWND hDlg, UINT message, WPARAM wParam, 
                              LPARAM lParam)
{
    RECT rc, rcDlg,rcOwner;
    HWND hwndOwner;

    switch (message) {
    case WM_INITDIALOG:
          if ((hwndOwner = GetParent(hDlg)) == NULL) {
            hwndOwner = GetDesktopWindow();
          }

          GetWindowRect(hwndOwner,&rcOwner);
          GetWindowRect(hDlg,&rcDlg);
          CopyRect(&rc,&rcOwner);
          OffsetRect(&rcDlg,-rcDlg.left,-rcDlg.top);
          OffsetRect(&rc,-rc.left,-rc.top);
          OffsetRect(&rc,-rcDlg.right,-rcDlg.bottom);
          SetWindowPos(hDlg,hwndMain,rcOwner.left+(rc.right/2),
                       rcOwner.top+(rc.bottom/2), 0,0,
                       SWP_SHOWWINDOW | SWP_NOSIZE );

          // Clean entry field (init)
          SetWindowText(GetDlgItem(hDlg,IDC_EDIT_S5D1),"");
          SetWindowText(GetDlgItem(hDlg,IDC_EDIT_S5D2),"");
          SetWindowText(GetDlgItem(hDlg,IDC_EDIT_S6D1),"");
          SetWindowText(GetDlgItem(hDlg,IDC_EDIT_S6D2),"");
          SetWindowText(GetDlgItem(hDlg,IDC_EDIT_S7D1),"");
          SetWindowText(GetDlgItem(hDlg,IDC_EDIT_S7D2),"");
          read_disk_entry(hDlg);
          return TRUE;
    case WM_COMMAND:
       switch (LOWORD(wParam)) {
       case IDOK:
           if (write_disk_entry(hDlg)) {
               MessageBox(hDlg,"Failed to save disk configuration.\n"
                   "Please check or restore from backup.\n"
                   "Aborting Operation.\nYou can quit by pressing Cancel.",
                   "Disk configuration save failure",
                   MB_OK|MB_ICONEXCLAMATION);
               break;
           }
           EndDialog(hDlg, LOWORD(wParam));
           return TRUE;
       case IDCANCEL:
           EndDialog(hDlg, LOWORD(wParam));
           return FALSE;
       case IDC_BTN_S5D1:
       case IDC_BTN_S5D2:
       case IDC_BTN_S6D1:
       case IDC_BTN_S6D2:
       case IDC_BTN_S7D1:
       case IDC_BTN_S7D2:
           {
               OPENFILENAME opfn;
               char filename[1024];
               filename[0]=0;
               ZeroMemory(&opfn,sizeof(opfn));
               opfn.lStructSize=sizeof(opfn);
               opfn.hwndOwner=hDlg;
               opfn.lpstrFilter="2mg format (*.2mg)\0*.2mg\0"
                                "Prodos order format (*.po)\0*.po\0"
                                "Dos order format (*.dsk)\0*.dsk\0"
                                "\0\0";
               opfn.lpstrFile=filename;
               opfn.nMaxFile=1024;
               opfn.Flags=OFN_EXPLORER | OFN_FILEMUSTEXIST | 
                          OFN_HIDEREADONLY | OFN_NOCHANGEDIR ;
               if (GetOpenFileName(&opfn)) {
                   SetWindowText(GetDlgItem(hDlg,LOWORD(wParam-1000)),
                                 filename);
               }

           }
           break;
       }
       break;
    }
    return FALSE;
}

extern int want2restart;

void A2W_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
   switch (id) {
   case ID_HELP_ABOUT:
      win_pause=1;
      DialogBoxParam(GetModuleHandle(NULL), 
                     (LPCTSTR)IDD_ABOUT_DIALOG,
                     hwnd, 
                     (DLGPROC)A2W_Dlg_About_Dialog,0);
      win_pause=0;
      break;
   case ID_FILE_JOYSTICK:
       if (g_joystick_type == JOYSTICK_MOUSE) {
            joystick_init();
            if (g_joystick_type != JOYSTICK_MOUSE){
                CheckMenuItem(GetMenu(hwndMain),ID_FILE_JOYSTICK,
                              MF_CHECKED);
            };
       } else {
           g_joystick_type = JOYSTICK_MOUSE;
           CheckMenuItem(GetMenu(hwndMain),ID_FILE_JOYSTICK,
                         MF_UNCHECKED);
       }
       break;
   case ID_FILE_DISK:
      win_pause=1; 
      DialogBoxParam(GetModuleHandle(NULL), 
                     (LPCTSTR)IDD_DLG_DISKCONF,
                     hwnd, 
                     (DLGPROC)A2W_Dlg_Disk,0);
      win_pause=0;
      break;
   case ID_FILE_EXIT:
      PostQuitMessage(0);
      break;
   case ID_FILE_DEBUGSTAT:
       win_stat_debug = ! win_stat_debug;
       {
           RECT rect,wrect;
           int adjx,adjy;
           GetClientRect(hwndMain,&rect);
           GetWindowRect(hwndMain,&wrect);
           adjx=(wrect.right-wrect.left)-(rect.right-rect.left);
           adjy=(wrect.bottom-wrect.top)-(rect.bottom-rect.top);
           SetWindowPos(hwndMain,HWND_NOTOPMOST,
                        win_rect_oldpos.left,win_rect_oldpos.top,
                        BASE_WINDOW_WIDTH+adjx,
                        STATUS_HEIGHT+adjy,
                        SWP_NOMOVE | SWP_SHOWWINDOW );
       }

       if (win_stat_debug) {
           CheckMenuItem(GetMenu(hwndMain),ID_FILE_DEBUGSTAT,MF_CHECKED);
       } else {
           CheckMenuItem(GetMenu(hwndMain),ID_FILE_DEBUGSTAT,MF_UNCHECKED);
       }
       break;
   case ID_FILE_SENDRESET:
      if (hwndHidden) {
          // Simulate key pressing to send reset
          SendMessage(hwndHidden,WM_KEYDOWN,0x11,0x1d0001L);
		  adb_physical_key_update(0x36, 0);
          SendMessage(hwndHidden,WM_KEYDOWN,0x3,0x1460001L);
	
          SendMessage(hwndHidden,WM_KEYUP,3,0xc1460001L);
		  SendMessage(hwndHidden,WM_KEYUP,0x11,0xc01d0001L); 
		  adb_physical_key_update(0x36, 1);
      }
      break;
   case ID_FILE_FULLSCREEN:
	  handle_vk_keys(VK_F11);
	  break;
  case ID_DEBUG:
	   set_halt(HALT_WANTTOBRK);
	   break;
	case ID_REBOOT:
	   set_halt(HALT_WANTTOQUIT);
	   want2restart=1;
	   break;
   }
}

#endif

void A2W_OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags) {
    int motion=0;
    int centerx,centery;
    centerx=centery=0;
    win_centroid(&centerx,&centery);
    motion=update_mouse(x,y, 0, 0);
    if (motion && g_warp_pointer) {
        win_warp_cursor(hwndMain);
        update_mouse(-1,-1,-1,-1);
    }
}

void A2W_OnLButtonDown(HWND hwnd, BOOL fDClick, int x, int y, UINT keyFlags) {
    int motion=0;
    int centerx,centery;
    centerx=centery=0;
    win_centroid(&centerx,&centery);
    motion=update_mouse(x,y,1,1);
    if (motion && g_warp_pointer) {
        win_warp_cursor(hwndMain);
        update_mouse(-1,-1,-1,-1);
    }
}

void A2W_OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags) {
    int motion=0;
    int centerx,centery;
    centerx=centery=0;
    win_centroid(&centerx,&centery);
    motion=update_mouse(x,y,0,1);
    if (motion && g_warp_pointer) {
        win_warp_cursor(hwndMain);
        update_mouse(-1,-1,-1,-1);
    }
}

void A2W_OnRButtonDown(HWND hwnd, BOOL fDClick, int x, int y, UINT keyFlags) {
    g_limit_speed++;
    if(g_limit_speed > SPEED_ENUMSIZE) {
        g_limit_speed = 0;
    }

    ki_printf("Toggling g_limit_speed to %d\n",g_limit_speed);
    switch(g_limit_speed) {
    case SPEED_UNLIMITED:
        ki_printf("...as fast as possible!\n");
        break;
    case SPEED_1MHZ:
        ki_printf("... 1.024MHz\n");
        break;
    case SPEED_GS:
        ki_printf("... 2.8MHz\n");
        break;
	case SPEED_ZIP:
		ki_printf("... 8.0MHz (Zip)\n");
        break;
    }
}

LRESULT CALLBACK A2WndProcHidden(HWND hwnd,UINT uMsg,WPARAM wParam,  
                                 LPARAM lParam) {
    /*
    if (uMsg == WM_KEYDOWN) {
        ki_printf ("Down:%x %x\n",wParam,lParam);
    }
    if (uMsg == WM_KEYUP) {
        ki_printf ("UP:%x %x\n",wParam,lParam);
    }
	*/
    
    switch (uMsg) 
    {
        HANDLE_MSG(hwnd,WM_MOUSEMOVE,A2W_OnMouseMove);
        HANDLE_MSG(hwnd,WM_KEYUP,A2W_OnKeyChar);
        HANDLE_MSG(hwnd,WM_KEYDOWN,A2W_OnKeyChar);
        HANDLE_MSG(hwnd,WM_SYSKEYUP,A2W_OnKeyChar);
        HANDLE_MSG(hwnd,WM_SYSKEYDOWN,A2W_OnKeyChar);
        HANDLE_MSG(hwnd,WM_LBUTTONDOWN,A2W_OnLButtonDown);
        HANDLE_MSG(hwnd,WM_LBUTTONUP,A2W_OnLButtonUp);
    } 
    return DefWindowProc(hwnd, uMsg, wParam, lParam); 
}

LRESULT CALLBACK A2WndProc(HWND hwnd,UINT uMsg,WPARAM wParam,  
                           LPARAM lParam) {
    switch (uMsg) 
    {  
        case WM_KEYDOWN:
           handle_vk_keys((UINT)wParam);
        case WM_MOUSEMOVE:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            if (hwndHidden != NULL) {
                PostMessage(hwndHidden,uMsg,wParam,lParam);
            }
            return 0;
		
        HANDLE_MSG(hwnd,WM_CREATE,A2W_OnCreate); 
        HANDLE_MSG(hwnd,WM_DESTROY,A2W_OnDestroy);
        HANDLE_MSG(hwnd,WM_ERASEBKGND,A2W_OnEraseBkgnd);
#ifdef KEGS_FULLINTERFACE
        HANDLE_MSG(hwnd,WM_RBUTTONDOWN,A2W_OnRButtonDown);
        HANDLE_MSG(hwnd,WM_COMMAND,A2W_OnCommand);
#endif
    } 
    return DefWindowProc(hwnd, uMsg, wParam, lParam); 
}

#if defined(KEGS_DIRECTDRAW)
BOOL initDirectDraw()
{
    HRESULT ddrval;
    DDSURFACEDESC ddsd;

    ddrval = DirectDrawCreate( NULL, &lpDD, NULL );
    if (FAILED(ddrval))
    {
        ki_printf ("Error creating direct draw\n");
        return(FALSE);
    }
   
    ddrval = IDirectDraw_SetCooperativeLevel(lpDD,hwndMain,DDSCL_NORMAL);
    if (FAILED(ddrval))
    {
        ki_printf ("Error setting cooperative level\n");
        IDirectDraw_Release(lpDD);
        return(FALSE);
    }
    memset( &ddsd, 0, sizeof(ddsd) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    ddrval = IDirectDraw_CreateSurface(lpDD,&ddsd, &primsurf, NULL );
    if (FAILED(ddrval))
    {
        ki_printf ("Error creating primary surface = %ld\n",ddrval);
        IDirectDraw_Release(lpDD);
        return(FALSE);
    }
    
    ddrval = IDirectDraw_CreateClipper(lpDD,0, &lpClipper, NULL );
    if (FAILED(ddrval))
    {
        ki_printf ("Error creating direct draw clipper = %ld\n",ddrval);
        IDirectDrawSurface_Release(primsurf);
        IDirectDraw_Release(lpDD);
        return(FALSE);
    }

    // setting it to our hwnd gives the clipper the coordinates from 
    // our window
    ddrval = IDirectDrawClipper_SetHWnd(lpClipper, 0, hwndMain );
    if (FAILED(ddrval))
    {
        ki_printf ("Error attaching direct draw clipper\n");
        IDirectDrawClipper_Release(lpClipper);
        IDirectDrawSurface_Release(primsurf);
        IDirectDraw_Release(lpDD);
        return(FALSE);
    }

    // attach the clipper to the primary surface
    ddrval = IDirectDrawSurface_SetClipper(primsurf,lpClipper );
    if (FAILED(ddrval))
    {
        ki_printf ("Error setting direct draw clipper\n");
        IDirectDrawClipper_Release(lpClipper);
        IDirectDrawSurface_Release(primsurf);
        IDirectDraw_Release(lpDD);
        return(FALSE);
    }

    memset( &ddsd, 0, sizeof(ddsd) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth =  BASE_WINDOW_WIDTH;
    ddsd.dwHeight = BASE_MARGIN_TOP+A2_WINDOW_HEIGHT+BASE_MARGIN_BOTTOM;
    ddsd.dwWidth =  GetSystemMetrics(SM_CXSCREEN);
    ddsd.dwHeight = GetSystemMetrics(SM_CYSCREEN);
    // create the backbuffer separately
    ddrval = IDirectDraw_CreateSurface(lpDD,&ddsd, &backsurf, NULL );
    if (FAILED(ddrval))
    {
        IDirectDrawClipper_Release(lpClipper);
        IDirectDrawSurface_Release(primsurf);
        IDirectDraw_Release(lpDD);
        return(FALSE);
    }

    return(TRUE);
}
#endif

int
video_init_win32(void)
{
   
    WNDCLASS wc; 


#ifdef KEGS_FULLINTERFACE
	unsigned int tid;
	int waitVar=0;

    _beginthreadex(NULL,0,dev_video_init_ex,(void *)&waitVar,0,&tid);

    // Wait for the background thread to finish initializing
    while (!waitVar) {
        Sleep(1);
    }
#else
	dev_video_init_ex(NULL);
#endif

	win_pause=0;
	g_x_shift_control_state = 0;

    // Create a hidden window
    wc.style = 0; 
    wc.lpfnWndProc = (WNDPROC) A2WndProcHidden; 
    wc.cbClsExtra = 0; 
    wc.cbWndExtra = 0; 
    wc.hInstance =  GetModuleHandle(NULL);
    wc.hIcon = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION); 
    wc.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW); 
    wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = "Kegs32 Hidden Window"; 
    if (!RegisterClass(&wc)) {
        ki_printf ("Register window failed\n");
       // return 0; 
    }
    hwndHidden = CreateWindow(
        "Kegs32 Hidden Window", 
        "Kegs32 Hidden Window", 
        WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        CW_USEDEFAULT, CW_USEDEFAULT, NULL,NULL,GetModuleHandle(NULL),NULL); 
   
	// OG : set the screen depth to 16 in order to use the full color map
	g_screen_depth=16;

	// initialize the default palette
	{
		int palette,i,r,g,b,value;

	    palette = g_a2vid_palette << 4;
        for(i = 0; i < 16; i++) {
            value=lores_colors[i];

            b = (lores_colors[i%16]    & 0xf) << 4;
            g = (lores_colors[i%16]>>4 & 0xf) << 4;
            r = (lores_colors[i%16]>>8 & 0xf) << 4;

            a2v_palette[palette+i].rgbBlue       = b;
            a2v_palette[palette+i].rgbGreen      = g;
            a2v_palette[palette+i].rgbRed        = r;
            a2v_palette[palette+i].rgbReserved   = 0;
        }
	}
    return 1;
}

unsigned int __stdcall dev_video_init_ex(void *param) {
    int i;
    int    keycode;
    int    tmp_array[0x80];
    int *waitVar=(int *)param;
#ifdef KEGS_FULLINTERFACE
    MSG msg;
#endif
    HACCEL hAccel;

    WNDCLASS wc; 
    LPBITMAPINFOHEADER pDib;
    TEXTMETRIC tm;  
//    WORD wVersionRequested;
   // WSADATA wsaData;
    int iStatusWidths[] = {60, 100,200,300, -1};

    // Create key-codes
    g_num_a2_keycodes = 0;
    for(i = 0; i <= 0x7f; i++) {
        tmp_array[i] = 0;
    }
    for(i = 0; i < 0x7f; i++) {
        keycode = a2_key_to_wsym[i][0];
        if(keycode < 0) {
            g_num_a2_keycodes = i;
            break;
        } else if(keycode > 0x7f) {
            ki_printf("a2_key_to_wsym[%d] = %02x!\n", i, keycode);
                my_exit(2);
        } else {
            if(tmp_array[keycode]) {
                ki_printf("a2_key_to_x[%d] = %02x used by %d\n",
                    i, keycode, tmp_array[keycode] - 1);
            }
            tmp_array[keycode] = i + 1;
        }
    }

    // Initialize Common Controls;
    InitCommonControls();

	/* why ???
    // Initialize Winsock

    wVersionRequested = MAKEWORD(1, 1);

    if (WSAStartup(wVersionRequested, &wsaData)) {
        ki_printf ("Unable to initialize winsock\n");
        my_exit(0);
    }
	*/

    // Create a window
    wc.style = 0; 
    wc.lpfnWndProc = (WNDPROC) A2WndProc; 
    wc.cbClsExtra = 0; 
    wc.cbWndExtra = 0; 
    wc.hInstance =  GetModuleHandle(NULL);
    wc.hIcon = LoadIcon((HINSTANCE) GetModuleHandle(NULL), 
               MAKEINTRESOURCE(IDC_KEGS32)); 
    wc.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW); 
    wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
    wc.lpszMenuName =  MAKEINTRESOURCE(IDC_KEGS32);
    wc.lpszClassName = "Kegs32"; 

    ki_printf ("Registering Window\n");
    if (!RegisterClass(&wc)) {
        ki_printf ("Register window failed\n");
       // return 0; 
    } 
 
    // Create the main window. 

#ifdef KEGS_FULLINTERFACE
	{
 
		ki_printf ("Creating Window\n");
		hwndMain = CreateWindow(
			"Kegs32", 
			"Kegs32 - GS Emulator", 
			WS_OVERLAPPEDWINDOW&(~(WS_MAXIMIZEBOX | WS_SIZEBOX)), 
			//WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 
			CW_USEDEFAULT, CW_USEDEFAULT, NULL,NULL,GetModuleHandle(NULL),NULL); 
	}
#endif
	      
    // If the main window cannot be created, terminate 
    // the application. 
 
    if (!hwndMain) {
        ki_printf ("Window create failed\n");
        return 0;
    }

#ifdef KEGS_FULLINTERFACE
    // Loading Accelerators
    hAccel=LoadAccelerators(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_ACCEL));

    // Create Toolbar	
    win_toolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
                  WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS, 0, 0, 0, 0,
                  hwndMain, (HMENU)IDR_TOOLBAR, GetModuleHandle(NULL), NULL);

	SendMessage(win_toolbar,TB_BUTTONSTRUCTSIZE,(WPARAM)sizeof(TBBUTTON),0);
  
	{
        TBADDBITMAP tbab;
        TBBUTTON tbb[9];
        HWND hwndToolTip;
        int i,j;
        int idCmd[]={ID_FILE_DISK,-1,
                     ID_SPEED_1MHZ,ID_SPEED_2MHZ,ID_SPEED_FMHZ,-1,ID_DEBUG,ID_CONSOLE,ID_REBOOT};

        ZeroMemory(tbb, sizeof(tbb));

        for (i=0,j=0;i<sizeof(tbb)/sizeof(TBBUTTON);i++) {
            if (idCmd[i] <0) {
                tbb[i].fsStyle = TBSTYLE_SEP;
            } else {
                tbb[i].iBitmap = j;
                tbb[i].idCommand = idCmd[i];
                tbb[i].fsState = TBSTATE_ENABLED;
                tbb[i].fsStyle = TBSTYLE_BUTTON;
                j++;
            }
        }

        tbab.hInst = GetModuleHandle(NULL);
        tbab.nID = IDR_TOOLBAR; //IDC_KEGS32;
        SendMessage(win_toolbar, TB_ADDBITMAP,j, (LPARAM)&tbab);
        SendMessage(win_toolbar, TB_ADDBUTTONS,i, (LPARAM)&tbb);
		GetWindowRect(win_toolbar,&win_toolHeight);

        // Activate the tooltip
        hwndToolTip = (HWND) SendMessage(win_toolbar,TB_GETTOOLTIPS,0L,0L);
        if (hwndToolTip) {
            SendMessage(hwndToolTip,TTM_ACTIVATE,TRUE,0L);
        }
    }
    
  
    // Create Status
    win_status  = CreateWindowEx(0, STATUSCLASSNAME, NULL,
                  WS_CHILD | WS_VISIBLE , 0, 0, 0, 0,
                  hwndMain, (HMENU)ID_STATUSBAR, GetModuleHandle(NULL), NULL);
    SendMessage(win_status, SB_SETPARTS, 5, (LPARAM)iStatusWidths);
    GetWindowRect(win_status,&win_statHeight);

    SendMessage(win_toolbar,TB_AUTOSIZE,0,0);
    SendMessage(win_status,WM_SIZE,0,0);
#else
	hAccel=NULL;
#endif
 
    #if defined(KEGS_DIRECTDRAW) 
    if (!initDirectDraw()) {
        ki_printf ("Unable to initialize Direct Draw\n");
        my_exit(0);
    }
    #endif

    // Create Bitmaps
    pDib = (LPBITMAPINFOHEADER)GlobalAlloc(GPTR,
            (sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)));
    pDib->biSize = sizeof(BITMAPINFOHEADER);
    pDib->biWidth = A2_WINDOW_WIDTH;;
    pDib->biHeight =-A2_WINDOW_HEIGHT;
    pDib->biPlanes = 1;
    pDib->biBitCount = 8;

    hdcRef = GetDC(hwndMain);

    // Set Text color and background color;
    SetTextColor(hdcRef,0xffffff);
    SetBkColor(hdcRef,0);
    SelectObject(hdcRef,GetStockObject(SYSTEM_FIXED_FONT));
    GetTextMetrics(hdcRef,&tm);
    dwchary=tm.tmHeight; 
 
    hcdc = CreateCompatibleDC(hdcRef);

    video_image_text[0]=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,DIB_RGB_COLORS,
                                   (VOID **)&video_data_text[0],NULL,0);
    video_image_text[1]=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,DIB_RGB_COLORS,
                                   (VOID **)&video_data_text[1],NULL,0);
    video_image_hires[0]=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,DIB_RGB_COLORS,
                                   (VOID **)&video_data_hires[0],NULL,0);
    video_image_hires[1]=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,DIB_RGB_COLORS,
                                   (VOID **)&video_data_hires[1],NULL,0);
    video_image_superhires=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,
                                       DIB_RGB_COLORS,
                                       (VOID **)&video_data_superhires,NULL,0);
    pDib->biWidth = EFF_BORDER_WIDTH;
    video_image_border_sides=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,
                                         DIB_RGB_COLORS,
                                         (VOID **)&video_data_border_sides,NULL,0);
    pDib->biWidth = (X_A2_WINDOW_WIDTH);
    pDib->biHeight = - (X_A2_WINDOW_HEIGHT - A2_WINDOW_HEIGHT + 2*8);
    video_image_border_special=CreateDIBSection(hdcRef,(BITMAPINFO *)pDib,
                                           DIB_RGB_COLORS,
                                          (VOID **)&video_data_border_special,
                                          NULL,0);
    GlobalFree(pDib);

    ShowWindow(hwndMain, SW_SHOWDEFAULT); 
    UpdateWindow(hwndMain); 

#ifdef KEGS_FULLINTERFACE
    *waitVar=1;

	while ( 
			(GetMessage(&msg,hwndMain,0,0)>0) 
		&&  ( 
				!(halt_sim & (HALT_WANTTOQUIT /*| HALT_WANTTOBRK */) )
			) 
		  )
	{
        if (!(TranslateAccelerator(hwndMain,hAccel,&msg)))
            TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    }

    set_halt(HALT_WANTTOQUIT);	
#endif
    return 0;
}

void update_colormap(int mode) {
    
}

void redraw_border(void) {

}

void
video_check_input_events_win32(void) {
    MSG msg;
   
    // Try to update joystick buttons 
    joystick_update_button_win32(); 

    while (win_pause) {
        Sleep(1);
    }

    if (PeekMessage(&msg,hwndHidden,0,0,PM_NOREMOVE)) {
        do {
            if (GetMessage(&msg,hwndHidden,0,0)>0) {
                TranslateMessage(&msg); 
                DispatchMessage(&msg); 
            } else {
                my_exit(0);
            }
        } while (PeekMessage(&msg,hwndHidden,0,0,PM_NOREMOVE));
    }
}

void
video_redraw_status_lines_win32(void)
{
    
    int line=0;
    char *buf;
    char buffer[255]; 
    int height=dwchary;
    int centerx,centery;

    centerx=centery=0;
    win_centroid(&centerx,&centery);
    if (!g_fullscreen) {
        centery+=28;
    }
    if (win_stat_debug) {
        for(line = 0; line < MAX_STATUS_LINES; line++) {
            buf = &(g_video_status_buf[line][0]);
            TextOut(hdcRef, 0+centerx, X_A2_WINDOW_HEIGHT+
                    height*line+centery,buf, (int)strlen(buf));
        }
    }
	
    if (win_status !=NULL && !g_fullscreen) {
        SendMessage(win_status, SB_SETTEXT,0,(LPARAM)"KEGS 0.64");
        sprintf(buffer,"Vol:%d",g_doc_vol);
        SendMessage(win_status, SB_SETTEXT,1,(LPARAM)buffer);
        buf=strstr(g_video_status_buf[0],"sim MHz:");
        
        if (buf) {
            memset(buffer,0,255);
            strncpy(buffer,buf,strchr(buf+8,' ')-buf);
            SendMessage(win_status, SB_SETTEXT,2,(LPARAM) buffer);
        } else {
            SendMessage(win_status, SB_SETTEXT,2,(LPARAM) "sim MHz:???");
        }
		
        buf=strstr(g_video_status_buf[0],"Eff MHz:");
        if (buf) {
            memset(buffer,0,255);
            strncpy(buffer,buf,strchr(buf+8,',')-buf);           
            SendMessage(win_status, SB_SETTEXT,3,(LPARAM) buffer);
        } else {
            SendMessage(win_status, SB_SETTEXT,3,(LPARAM) "Eff MHz:???");
        }
        
        buf=strstr(g_video_status_buf[5],"fast");
        if (buf) {
            memset(buffer,0,255);
            strncpy(buffer,g_video_status_buf[5],buf-&(g_video_status_buf[5][0]));
            SendMessage(win_status, SB_SETTEXT,4,(LPARAM) buffer);
        } else {
           
        }

    }
   
}

void
video_refresh_image_win32()
{
    register word32 start_time;
    register word32 end_time;
    int    start;
    word32    mask;
    int    line;
    int    left_pix, right_pix;
    int    left, right;
    HBITMAP last_xim, cur_xim;

    if(g_border_sides_refresh_needed) {
        g_border_sides_refresh_needed = 0;
        win_refresh_border_sides();
    }
    if(g_border_special_refresh_needed) {
        g_border_special_refresh_needed = 0;
        win_refresh_border_special();
    }

    if(a2_screen_buffer_changed == 0) {
        return;
    }

    GET_ITIMER(start_time);

    start = -1;
    mask = 1;
    last_xim = NULL;

    left_pix = 640;
    right_pix = 0;

    for(line = 0; line < 25; line++) {
        if((g_full_refresh_needed & (1 << line)) != 0) {
            left = a2_line_full_left_edge[line];
            right = a2_line_full_right_edge[line];
        } else {
            left = a2_line_left_edge[line];
            right = a2_line_right_edge[line];
        }

        if(!(a2_screen_buffer_changed & mask)) {
            /* No need to update this line */
            /* Refresh previous chunks of lines, if any */
            if(start >= 0) {
                win_refresh_lines(last_xim, start, line,
                    left_pix, right_pix);
                start = -1;
                left_pix = 640;
                right_pix =  0;
            }
        } else {
            /* Need to update this line */
            cur_xim = a2_line_xim[line];
            if(start < 0) {
                start = line;
                last_xim = cur_xim;
            }
            if(cur_xim != last_xim) {
                /* do the refresh */
                win_refresh_lines(last_xim, start, line,
                    left_pix, right_pix);
                last_xim = cur_xim;
                start = line;
                left_pix = left;
                right_pix = right;
            }
            left_pix = MIN(left, left_pix);
            right_pix = MAX(right, right_pix);
        }
        mask = mask << 1;
    }

    if(start >= 0) {
        win_refresh_lines(last_xim, start, 25, left_pix, right_pix);
    }

    a2_screen_buffer_changed = 0;

    g_full_refresh_needed = 0;

    /* And redraw border rectangle? */

    GET_ITIMER(end_time);

    g_cycs_in_xredraw += (end_time - start_time);
}

void
win_refresh_lines(HBITMAP xim, int start_line, int end_line, int left_pix,
        int right_pix)
{
    int srcy;

    if(left_pix >= right_pix || left_pix < 0 || right_pix <= 0) {
        halt_printf("win_refresh_lines: lines %d to %d, pix %d to %d\n",
            start_line, end_line, left_pix, right_pix);
        ki_printf("a2_screen_buf_ch:%08x, g_full_refr:%08x\n",
            a2_screen_buffer_changed, g_full_refresh_needed);
        show_a2_line_stuff();
    }

    srcy = 16*start_line;

    if(xim == video_image_border_special) {
        /* fix up y pos in src */
        ki_printf("win_refresh_lines called, video_image_border_special!!\n");
        srcy = 0;
    }

    g_refresh_bytes_xfer += 16*(end_line - start_line) *
                            (right_pix - left_pix);

    if (hwndMain == NULL) {
        return;
    }
    if (g_cur_a2_stat & 0xa0) {
        win_refresh_image(win_palette,hdcRef,xim,left_pix,srcy,BASE_MARGIN_LEFT+left_pix,
                          BASE_MARGIN_TOP+16*start_line,
                          right_pix-left_pix,16*(end_line-start_line));
    } else {
        win_refresh_image(a2v_palette,hdcRef,xim,left_pix,srcy,BASE_MARGIN_LEFT+left_pix
                        /*  +BORDER_WIDTH*/,
                          BASE_MARGIN_TOP+16*start_line,
                          right_pix-left_pix,16*(end_line-start_line));
    }
}

void
x_redraw_border_sides_lines(int end_x, int width, int start_line,
    int end_line)
{
    HBITMAP    xim;

    if(start_line < 0 || width < 0) {
        return;
    }

#if 0
    ki_printf("redraw_border_sides lines:%d-%d from %d to %d\n",
        start_line, end_line, end_x - width, end_x);
#endif
    xim = video_image_border_sides;
    g_refresh_bytes_xfer += 16 * (end_line - start_line) * width;

    if (hwndMain == NULL) {
        return;
    }
    win_refresh_image (a2v_palette,hdcRef,xim,0,16*start_line,end_x-width,
                       BASE_MARGIN_TOP+16*start_line,width,
                       16*(end_line-start_line));
}

void
win_refresh_border_sides()
{
    int    old_width;
    int    prev_line;
    int    width;
    int    mode;
    int    i;

#if 0
    ki_printf("refresh border sides!\n");
#endif

    /* can be "jagged" */
    prev_line = -1;
    old_width = -1;
    for(i = 0; i < 25; i++) {
        mode = (a2_line_stat[i] >> 4) & 7;
        width = EFF_BORDER_WIDTH;
        if(mode == MODE_SUPER_HIRES) {
            width = BORDER_WIDTH;
        }
        if(width != old_width) {
            if (g_cur_a2_stat & 0xa0) {
                x_redraw_border_sides_lines(old_width,
                    old_width, prev_line, i);    
                x_redraw_border_sides_lines(X_A2_WINDOW_WIDTH,
                    old_width, prev_line, i);    
            } else {
                x_redraw_border_sides_lines(old_width-BORDER_WIDTH,
                    old_width-BORDER_WIDTH, prev_line, i);    
                x_redraw_border_sides_lines(X_A2_WINDOW_WIDTH,
                    old_width-BORDER_WIDTH, prev_line, i);    
            }

            prev_line = i;
            old_width = width;
        }
    }
    if (g_cur_a2_stat & 0xa0) {
        x_redraw_border_sides_lines(old_width, 
                                    old_width, prev_line,25);
        x_redraw_border_sides_lines(X_A2_WINDOW_WIDTH, 
                                    old_width, prev_line,25);
    } else {
        x_redraw_border_sides_lines(old_width-BORDER_WIDTH, 
                                    old_width-BORDER_WIDTH, prev_line,25);
        x_redraw_border_sides_lines(X_A2_WINDOW_WIDTH, 
                                    old_width-BORDER_WIDTH, prev_line,25);
    }
}

void
win_refresh_border_special()
{
    HBITMAP    xim;
    int    width, height;

    width = X_A2_WINDOW_WIDTH;
    height = BASE_MARGIN_TOP;

    xim = video_image_border_special;
    g_refresh_bytes_xfer += 16 * width *
                (BASE_MARGIN_TOP + BASE_MARGIN_BOTTOM);

    if (hwndMain == NULL) {
        return;
    }
    win_refresh_image (a2v_palette,hdcRef,xim,0,0,0,BASE_MARGIN_TOP+A2_WINDOW_HEIGHT,
                       width,BASE_MARGIN_BOTTOM);
    win_refresh_image (a2v_palette,hdcRef,xim,0,BASE_MARGIN_BOTTOM,0,0,
                       width,BASE_MARGIN_TOP);
}

void video_auto_repeat_on_win32(int must)
{
}

void video_auto_repeat_off_win32(int must)
{
}

#else  /* !WIN32 */
int video_init_win32() { return 0; }
void video_shutdown_win32() {}
void video_update_physical_colormap_win32() {}
void video_update_color_win32(int col_num, int a2_color) {}
void video_refresh_image_win32() {}
void video_check_input_events_win32() {}
void video_redraw_status_lines_win32() {}
void video_auto_repeat_on_win32(int must) {}
void video_auto_repeat_off_win32(int must) {}
void video_warp_pointer_win32(void) {}
#endif /* !WIN32 */
