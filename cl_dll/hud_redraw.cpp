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

// Basit ESP için gerekli fonksiyonlar
void DrawSimpleESP()
{
    if (!gHUD.cl_esp || gHUD.cl_esp->value <= 0)
        return;

    // Tüm oyuncuları dolaş
    for (int i = 1; i < MAX_PLAYERS; i++)
    {
        cl_entity_t* pPlayer = gEngfuncs.GetEntityByIndex(i);
        if (!pPlayer || !pPlayer->model || !pPlayer->player)
            continue;
            
        // Kendi oyuncumuzu ve ölü oyuncuları atla
        if (pPlayer->index == gEngfuncs.GetLocalPlayer()->index || 
            pPlayer->curstate.solid == 0)
            continue;

        // Oyuncu pozisyonu
        Vector origin = pPlayer->origin;
        Vector headPos = origin;
        headPos.z += 64.0f; // Oyuncu boyu

        // Ekran koordinatları
        Vector screenPos, headScreenPos;
        
        // Dünya koordinatlarını ekran koordinatlarına çevir
        if (gEngfuncs.pTriAPI->WorldToScreen(origin, screenPos) &&
            gEngfuncs.pTriAPI->WorldToScreen(headPos, headScreenPos))
        {
            // Ekran dışındaysa atla
            if (screenPos[2] < 0 || headScreenPos[2] < 0)
                continue;

            // Kutu yüksekliği ve genişliği
            float height = abs(screenPos[1] - headScreenPos[1]);
            float width = height / 2.0f;

            // Kutu koordinatları
            float x = screenPos[0] - width / 2;
            float y = headScreenPos[1];
            
            // Takım rengi
            int r, g, b;
            int playerTeam = g_PlayerExtraInfo[pPlayer->index].teamnumber;
            
            if (playerTeam == 1) // Terörist - Kırmızı
            {
                r = 255; g = 0; b = 0;
            }
            else if (playerTeam == 2) // Counter-Terörist - Mavi
            {
                r = 0; g = 0; b = 255;
            }
            else // Bilinmeyen takım - Sarı
            {
                r = 255; g = 255; b = 0;
            }

            // Kutu çiz
            gEngfuncs.pfnFillRGBABlend(x, y, width, height, r, g, b, 50); // Şeffaf kutu
            gEngfuncs.pfnDrawRectangle(x, y, x + width, y + height, r, g, b, 255); // Kenarlık
        }
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

	// ESP çizimi - tüm HUD elementlerinden sonra
	if (!intermission && gHUD.cl_esp && gHUD.cl_esp->value > 0)
	{
		DrawSimpleESP();
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
