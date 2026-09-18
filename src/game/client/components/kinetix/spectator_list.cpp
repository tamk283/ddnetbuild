#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <base/vmath.h>
#include <base/math.h>

// SpectatorList: on-screen list of players currently spectating (top-left).

void CBotNet::RenderSpectatorList()
{
	if(!g_Config.m_KxSpecList)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame || !pGame->m_Snap.m_pLocalInfo)
		return;

	int Total = 0;
	for(const CNetObj_PlayerInfo *pInfo : pGame->m_Snap.m_apInfoByName)
	{
		if(pInfo && pInfo->m_Team == TEAM_SPECTATORS)
			Total++;
	}
	if(Total == 0)
		return;

	float Width = 400.0f * pGame->Graphics()->ScreenAspect();
	pGame->Graphics()->MapScreen(0.0f, 0.0f, Width, 400.0f);

	const float x = 5.0f;
	const float y0 = 5.0f;
	const float FontSize = 6.0f;
	const float LineH = FontSize + 1.0f;
	const int MaxShown = 20;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Spectators (%d)", Total);
	pGame->TextRender()->TextColor(1.0f, 1.0f, 0.3f, 1.0f);
	pGame->TextRender()->Text(x, y0, FontSize, aBuf, -1.0f);

	int Shown = 0;
	int Extra = 0;
	pGame->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	for(const CNetObj_PlayerInfo *pInfo : pGame->m_Snap.m_apInfoByName)
	{
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;
		if(Shown >= MaxShown)
		{
			Extra++;
			continue;
		}
		pGame->TextRender()->Text(x, y0 + (Shown + 1) * LineH, FontSize,
			pGame->m_aClients[pInfo->m_ClientId].m_aName, -1.0f);
		Shown++;
	}
	if(Extra > 0)
	{
		str_format(aBuf, sizeof(aBuf), "+%d more", Extra);
		pGame->TextRender()->Text(x, y0 + (Shown + 1) * LineH, FontSize, aBuf, -1.0f);
	}
	pGame->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}
