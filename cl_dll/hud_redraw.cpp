/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud_redraw.cpp
//
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "triangleapi.h"

#include <string.h>

#include "draw_util.h"

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};


extern int g_iVisibleMouse;

float HUD_GetFOV( void );

// hud_redraw.cpp dosyasının en üstüne bu yapıları ve namespace'i ekleyin
struct vec2 { float x, y; };
struct vec3 { float x, y, z; };
struct vec4 { float x, y, z, w; };

namespace GL {
    int viewport[4];
    
    void SetupOrtho();
    void Restore();
    void DrawFilledRect(float x, float y, float width, float height, const unsigned char color[3]);
    void DrawOutline(float x, float y, float width, float height, float lineWidth, const unsigned char color[3]);
    void DrawCrosshair(vec2& l1p1, vec2& l1p2, vec2& l2p1, vec2& l2p2, float lineWidth, const unsigned char color[3]);
    void DrawESPBox(vec2 head, vec2 feet, float lineWidth, const unsigned char color[3]);
    bool WorldToScreen(vec3 pos, vec2& screen, float matrix[16], int windowWidth, int windowHeight);
}

// ESP çizim fonksiyonları
void DrawESP()
{
    if (!gHUD.cl_esp || gHUD.cl_esp->value <= 0)
        return;
        
    GL::SetupOrtho();
    
    for (int i = 1; i < MAX_PLAYERS; i++)
    {
        cl_entity_t* pPlayer = gEngfuncs.GetEntityByIndex(i);
        if (!pPlayer || !pPlayer->model || !pPlayer->player)
            continue;
            
        // Kendi oyuncumuzu ve ölü oyuncuları atla
        if (pPlayer->index == gEngfuncs.GetLocalPlayer()->index || 
            pPlayer->curstate.solid == 0)
            continue;
            
        DrawPlayerESP(pPlayer);
    }
    
    GL::Restore();
}

void DrawPlayerESP(cl_entity_t* pPlayer)
{
    vec3 origin = pPlayer->origin;
    vec3 headPos = origin;
    headPos.z += 64.0f; // Oyuncu boyu
    
    vec2 feetScreen, headScreen;
    float matrix[16];
    
    gEngfuncs.pTriAPI->GetMatrix(GL_MODELVIEW_MATRIX, matrix);
    
    int width = ScreenWidth();
    int height = ScreenHeight();
    
    if (GL::WorldToScreen(origin, feetScreen, matrix, width, height) &&
        GL::WorldToScreen(headPos, headScreen, matrix, width, height))
    {
        // Takım renklerine göre ESP rengi
        unsigned char color[3];
        
        // Team bilgisini g_PlayerExtraInfo'dan al
        int playerTeam = g_PlayerExtraInfo[pPlayer->index].teamnumber;
        
        if (playerTeam == 1) // Terörist - Kırmızı
        {
            color[0] = 255; color[1] = 0; color[2] = 0;
        }
        else if (playerTeam == 2) // Counter-Terörist - Mavi
        {
            color[0] = 0; color[1] = 0; color[2] = 255;
        }
        else // Bilinmeyen takım - Sarı
        {
            color[0] = 255; color[1] = 255; color[2] = 0;
        }
        
        GL::DrawESPBox(headScreen, feetScreen, 2.0f, color);
    }
}

// Think
void CHud::Think(void)
{
	int newfov;

	extern int g_weaponselect_frames;
	if( g_weaponselect_frames )
		g_weaponselect_frames--;

	for( HUDLIST *pList = m_pHudList; pList; pList = pList->pNext )
	{
		if( pList->p->m_iFlags & HUD_THINK )
			pList->p->Think();
	}

	newfov = HUD_GetFOV();
	m_iFOV = newfov ? newfov : default_fov->value;

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)default_fov->value) * zoom_sens_ratio->value;
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  // only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = max( default_fov->value, 90 );
	}

}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static int m_flShotTime = 0;

	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

	UpdateDefaultHUDColor();

	if ( m_pCvarDraw->value && (intermission || !(m_iHideHUDDisplay & HIDEHUD_ALL) ) )
	{
		for( HUDLIST *pList = m_pHudList; pList; pList = pList->pNext )
		{
			if( pList->p->m_iFlags & HUD_DRAW )
			{
				if( intermission && !(pList->p->m_iFlags & HUD_INTERMISSION) )
					continue; // skip no-intermission during intermission

				pList->p->Draw( flTime );
			}
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}

	// Bu satırı en sona ekleyin (tüm HUD çizimlerinden sonra)
	if (!intermission && gHUD.cl_esp && gHUD.cl_esp->value > 0)
	{
		DrawESP();
	}

	// update codepage parameters
	if( !stricmp( con_charset->string, "cp1251" ))
	{
		g_codepage = 1251;
	}
	else if( !stricmp( con_charset->string, "cp1252" ))
	{
		g_codepage = 1252;
	}
	else
	{
		g_codepage = 0;
	}

	g_accept_utf8 = !stricmp( cl_charset->string, "utf-8" );

	return 1;
}

void CHud::UpdateDefaultHUDColor()
{
	int r, g, b;

	if (sscanf(m_pCvarColor->string, "%d %d %d", &r, &g, &b) == 3) {
		r = max(r, 0);
		g = max(g, 0);
		b = max(b, 0);

		r = min(r, 255);
		g = min(g, 255);
		b = min(b, 255);

		m_iDefaultHUDColor = (r << 16) | (g << 8) | b;
	} else {
		m_iDefaultHUDColor = RGB_YELLOWISH;
	}
}

// GL fonksiyonlarının implementasyonu
void GL::SetupOrtho()
{
    glGetIntegerv(GL_VIEWPORT, viewport);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewport[2], viewport[3], 0, 0, 1);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
}

void GL::Restore()
{
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
}

void GL::DrawFilledRect(float x, float y, float width, float height, const unsigned char color[3])
{
    glColor4ub(color[0], color[1], color[2], 255);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void GL::DrawOutline(float x, float y, float width, float height, float lineWidth, const unsigned char color[3])
{
    glLineWidth(lineWidth);
    glBegin(GL_LINE_STRIP);
    glColor4ub(color[0], color[1], color[2], 255);
    glVertex2f(x - 0.5f, y - 0.5f);
    glVertex2f(x + width + 0.5f, y - 0.5f);
    glVertex2f(x + width + 0.5f, y + height + 0.5f);
    glVertex2f(x - 0.5f, y + height + 0.5f);
    glVertex2f(x - 0.5f, y - 0.5f);
    glEnd();
}

void GL::DrawESPBox(vec2 head, vec2 feet, float lineWidth, const unsigned char color[3])
{
    glLineWidth(lineWidth);
    glBegin(GL_LINE_STRIP);

    int height = abs(head.y - feet.y);
    vec2 tl, tr, bl, br;

    tl.x = head.x - height / 4;
    tr.x = head.x + height / 4;
    tl.y = tr.y = head.y;

    bl.x = feet.x - height / 4;
    br.x = feet.x + height / 4;
    bl.y = br.y = feet.y;

    // outline
    glColor4ub(0, 0, 0, 255/2);
    glVertex2f(tl.x - 1, tl.y - 1);
    glVertex2f(tr.x + 1, tr.y - 1);
    glVertex2f(br.x + 1, br.y + 1);
    glVertex2f(bl.x - 1, bl.y + 1);
    glVertex2f(tl.x - 1, tl.y);
    // inline
    glVertex2f(tl.x + 1, tl.y + 1);
    glVertex2f(tr.x - 1, tr.y + 1);
    glVertex2f(br.x - 1, br.y - 1);
    glVertex2f(bl.x + 1, bl.y - 1);
    glVertex2f(tl.x + 1, tl.y + 1);

    glColor4ub(color[0], color[1], color[2], 255);
    glVertex2f(tl.x, tl.y);
    glVertex2f(tr.x, tr.y);
    glVertex2f(br.x, br.y);
    glVertex2f(bl.x, bl.y);
    glVertex2f(tl.x, tl.y);

    glEnd();
}

bool GL::WorldToScreen(vec3 pos, vec2& screen, float matrix[16], int windowWidth, int windowHeight)
{
    // Matrix-vector Product, multiplying world(eye) coordinates by projection matrix = clipCoords
    vec4 clipCoords;
    clipCoords.x = pos.x * matrix[0] + pos.y * matrix[4] + pos.z * matrix[8] + matrix[12];
    clipCoords.y = pos.x * matrix[1] + pos.y * matrix[5] + pos.z * matrix[9] + matrix[13];
    clipCoords.z = pos.x * matrix[2] + pos.y * matrix[6] + pos.z * matrix[10] + matrix[14];
    clipCoords.w = pos.x * matrix[3] + pos.y * matrix[7] + pos.z * matrix[11] + matrix[15];

    if (clipCoords.w < 0.1f)
        return false;

    // perspective division, dividing by clip.W = Normalized Device Coordinates
    vec3 NDC;
    NDC.x = clipCoords.x / clipCoords.w;
    NDC.y = clipCoords.y / clipCoords.w;
    NDC.z = clipCoords.z / clipCoords.w;

    // Transform to window coordinates
    screen.x = (windowWidth / 2 * NDC.x) + (NDC.x + windowWidth / 2);
    screen.y = -(windowHeight / 2 * NDC.y) + (NDC.y + windowHeight / 2);
    return true;
}
