/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Keyboard bindings and mouse capture for the preview window.
===========================================================================
*/
#include "flare/mathlib.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../common/q_round.h"
#include "flare/print.h"
#include "flare/options.h"
#include "flare/preview.h"


int previewMouseCenterX = 0;
int previewMouseCenterY = 0;
POINT previewMouseCenter = { 0 };
char *previewKeyBindings[256] = { NULL };
bool previewKeyDown[256] = { false };
bool previewCapsLock = false;
unsigned int movementFlags = 0;

/* Unshifted and shifted characters indexed by Win32 virtual-key code. */
extern const char previewKeyChars[2][256] = {
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, '\t', 0, 0, 0, '\n', 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0, 0, 0, 0, 0, 0,
        0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0, 0, 0, 0, 0,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '+', 0, '-', '.', '/',
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ';', '=', ',', '-', '.', '/',
        '`', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '[', '\\', ']', '\'', 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, '\t', 0, 0, 0, '\n', 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ')', '!', '@', '#', '$', '%', '^', '&', '*', '(', 0, 0, 0, 0, 0, 0,
        0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0, 0, 0, 0, 0,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '+', 0, '-', '.', '/',
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ':', '+', '<', '_', '>', '?',
        '~', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '{', '|', '}', '"', 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
};


/* Binding names share that index; the final two entries are wheel pulses. */
char *previewKeyNames[256] = {
    NULL, "mouse1", "mouse2", NULL, "mouse3", NULL, NULL, NULL,
    NULL, "tab", NULL, NULL, NULL, "enter", NULL, NULL,
    "shift", "ctrl", "alt", "pause", "caps", NULL, NULL, NULL,
    NULL, NULL, NULL, "escape", NULL, NULL, NULL, NULL,
    "space", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", NULL, NULL, NULL, NULL, NULL,
    "numpad0", "numpad1", "numpad2", "numpad3", "numpad4", "numpad5", "numpad6", "numpad7",
    "numpad8", "numpad9", "numpad*", "numpad+", NULL, "numpad-", "numpad.", "numpad/",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "numlock", "scroll", NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, "semicolon", "=", ",", "-", ".", "/",
    "`", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, "[", "\\", "]", "quote", NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, "mwheelup", "mwheeldown",
};



/*
=============
Key_ExecuteBinding

Execute the bound command on a key press. On release, turn a held-button plus
command into its corresponding minus command.
=============
*/
bool Key_ExecuteBinding( int key, char character, bool down ) {
    char command[65536];
    const char *binding = previewKeyBindings[key];
    if ( binding ) {
        if ( down ) {
            ExecuteConfigLine( binding );
        } else if ( binding[0] == '+' ) {
            /* A held-button command gets its matching minus command on release. */
            /* Detect truncation before executing the copied command. */
            command[sizeof( command ) - 1] = 0;
            strncpy( command, binding, sizeof( command ) );
            if ( command[sizeof( command ) - 1] )
                Q3Map_Error( "key command too long" );
            command[0] = '-';
            ExecuteConfigLine( command );
        }
    }
    return true;
}

/*
=============
Key_Init

Clear held keys, read Caps Lock state and establish the mouse recentering
coordinates for the window.
=============
*/
void Key_Init( HWND window ) {
    RECT rect;
    memset( previewKeyDown, 0, sizeof( previewKeyDown ) );
    previewCapsLock = (GetKeyState( VK_CAPITAL ) & 1) != 0;
    GetWindowRect( window, &rect );
    previewMouseCenterX = (rect.left + rect.right) / 2;
    previewMouseCenterY = (rect.top + rect.bottom) / 2;
    previewMouseCenter.x = previewMouseCenterX;
    previewMouseCenter.y = previewMouseCenterY;
    ClientToScreen( window, &previewMouseCenter );
    SetCursorPos( previewMouseCenter.x, previewMouseCenter.y );
}

/*
=============
Key_Shutdown

Provide the input shutdown hook; no cleanup is required here.
=============
*/
void Key_Shutdown( void ) {
}

/*
=============
Key_Event

Update the key state and derive its character from Shift and Caps Lock.
Dispatch the binding and handle an unconsumed Escape press.
=============
*/
void Key_Event( int key, bool down ) {
    char character;
    if ( key == VK_CAPITAL && down )
        previewCapsLock = (GetKeyState( VK_CAPITAL ) & 1) != 0;
    character = previewKeyChars[previewKeyDown[VK_SHIFT]][key];
    /* Caps Lock reverses the alphabetic case selected by Shift. */
    if ( previewCapsLock && isalpha( character ) )
        character ^= 32;
    previewKeyDown[key] = down;
    if ( !Key_ExecuteBinding( key, character, down ) && down && key == VK_ESCAPE )
        PostQuitMessage( 0 );
}

/*
=============
Key_CaptureMouse

Capture mouse input, hide the cursor and confine it to the window rectangle.
=============
*/
void Key_CaptureMouse( HWND window ) {
    RECT rect;
    SetCapture( window );
    ShowCursor( FALSE );
    GetWindowRect( window, &rect );
    ClipCursor( &rect );
}

/*
=============
Key_ReleaseMouse

Release mouse capture and cursor confinement, and show the cursor.
=============
*/
void Key_ReleaseMouse( void ) {
    ClipCursor( NULL );
    ShowCursor( TRUE );
    ReleaseCapture();
}

/*
=============
Key_KeynumToString

Return the binding name for the supplied key number.
=============
*/
const char *Key_KeynumToString( unsigned int key ) {
    return previewKeyNames[key];
}

/*
=============
Key_StringToKeynum

Find a key number by its binding name, ignoring case. Return -1 when the name
is unknown.
=============
*/
int Key_StringToKeynum( const char *name ) {
    unsigned int key;
    for ( key = 0; key < 256; key++ ) {
        if ( previewKeyNames[key] && !stricmp( previewKeyNames[key], name ) )
            return key;
    }
    return -1;
}

/*
=============
Key_IsDown

Return whether the supplied key is currently held.
=============
*/
bool Key_IsDown( int key ) {
    return previewKeyDown[key];
}

/*
=============
Key_SetBinding

Replace a key binding with an owned copy of the command. Clear the binding
when the supplied command is empty.
=============
*/
void Key_SetBinding( int key, const char *binding ) {
    if ( previewKeyBindings[key] )
        delete[] previewKeyBindings[key];
    if ( binding && binding[0] ) {
        previewKeyBindings[key] = new char[strlen( binding ) + 1];
        if ( !previewKeyBindings[key] )
            Q3Map_Error( "Couldn't allocate storage for key binding (%i bytes)\n", strlen( binding ) + 1 );
        strcpy( previewKeyBindings[key], binding );
    } else {
        previewKeyBindings[key] = NULL;
    }
}

/* Mouse movement controls the camera angles. */
#include "flare/preview_view.h"

/*
=============
Key_MouseMove

Apply mouse sensitivity to yaw and pitch changes. Wrap yaw to one turn and
clamp pitch short of vertical.
=============
*/
bool Key_MouseMove( int dx, int dy ) {
    float sensitivity = Preview_CvarFloat( "sensitivity" );
    float yaw = Preview_CvarFloat( "m_yaw" ) * -dx * sensitivity;
    float pitch = Preview_CvarFloat( "m_pitch" ) * dy * sensitivity;
    /* Wrap yaw to one turn, including negative mouse deltas. */
    previewView.yaw += yaw;
    if ( previewView.yaw > 360 || previewView.yaw < 0 )
        previewView.yaw -= FloorToInt( previewView.yaw * (1.0f / 360.0f) ) * 360;
    /* Stop short of vertical so the view keeps a stable horizontal direction. */
    previewView.pitch += pitch;
    if ( previewView.pitch < -85 )
        previewView.pitch = -85;
    else if ( previewView.pitch > 85 )
        previewView.pitch = 85;
    return true;
}

/*
=============
Key_MouseEvent

Apply movement away from the mouse center to the camera and recenter the
cursor.
=============
*/
void Key_MouseEvent( int x, int y ) {
    if ( x != previewMouseCenterX || y != previewMouseCenterY ) {
        Key_MouseMove( x - previewMouseCenterX, y - previewMouseCenterY );
        SetCursorPos( previewMouseCenter.x, previewMouseCenter.y );
    }
}
