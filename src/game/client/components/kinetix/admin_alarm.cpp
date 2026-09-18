#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <base/vmath.h>
#include <base/math.h>

// AdminAlarm: sound + red on-screen message when a new admin joins the server.

void CBotNet::UpdateAdminAlarm()
{
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	bool enabled = g_Config.m_KxAdminAlarm != 0;

	if(enabled && !m_AdminAlarmWasActive)
	{
		// Snapshot: no alarm for staff already on the server.
		for(int i = 0; i < 128; i++)
			m_aAdminAlarmState[i] = pGame->m_aClients[i].m_Active ? pGame->m_aClients[i].m_AuthLevel : -1;
		m_AdminAlarmWasActive = true;
		m_AdminAlarmUntilTick = 0;
		m_aAdminAlarmMsg[0] = 0;
	}
	else if(!enabled && m_AdminAlarmWasActive)
	{
		m_AdminAlarmWasActive = false;
		m_AdminAlarmUntilTick = 0;
		m_aAdminAlarmMsg[0] = 0;
		return;
	}

	if(!enabled)
		return;

	for(int i = 0; i < 128; i++)
	{
		if(!pGame->m_aClients[i].m_Active)
		{
			m_aAdminAlarmState[i] = -1;
			continue;
		}

		const int Level = pGame->m_aClients[i].m_AuthLevel;
		if(m_aAdminAlarmState[i] == Level)
			continue;

		m_aAdminAlarmState[i] = Level;

		if(Level == 0)
			continue;

		str_format(m_aAdminAlarmMsg, sizeof(m_aAdminAlarmMsg),
			"ADMIN: %s (rank %d)", pGame->m_aClients[i].m_aName, Level);
		m_AdminAlarmUntilTick = Client()->GameTick(g_Config.m_ClDummy) + 6 * 50;
		pGame->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
		dbg_msg("kinetix", "%s", m_aAdminAlarmMsg);
	}
}

void CBotNet::RenderAdminAlarm()
{
	if(!g_Config.m_KxAdminAlarm || !m_aAdminAlarmMsg[0])
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	if(Client()->GameTick(g_Config.m_ClDummy) >= m_AdminAlarmUntilTick)
		return;

	float Width = 400.0f * pGame->Graphics()->ScreenAspect();
	pGame->Graphics()->MapScreen(0.0f, 0.0f, Width, 400.0f);

	const float TextW = pGame->TextRender()->TextWidth(8.0f, m_aAdminAlarmMsg);
	pGame->TextRender()->TextColor(1.0f, 0.2f, 0.2f, 1.0f);
	pGame->TextRender()->Text((Width - TextW) * 0.5f, 20.0f, 8.0f, m_aAdminAlarmMsg, -1.0f);
	pGame->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}
