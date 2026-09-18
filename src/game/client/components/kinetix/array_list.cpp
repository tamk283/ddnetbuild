#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <base/vmath.h>
#include <base/math.h>

// ArrayList: vertical list of enabled features in the top-right corner.

static const struct
{
	const char *m_pName;
	int CConfig::*m_pCvar;
} s_aArrayListFeatures[] = {
	{"AimBot", &CConfig::m_KxAimBot},
	{"TriggerBot", &CConfig::m_KxTriggerBot},
	{"Attack", &CConfig::m_KxAttack},
	{"ESP", &CConfig::m_KxEsp},
	{"Avoid Freeze", &CConfig::m_KxAvoidFreeze},
	{"Kinodynamic", &CConfig::m_KxKinodynamic},
	{"Laser Rescue", &CConfig::m_KxLaserRescue},
	{"Trajectory", &CConfig::m_KxShowTrajectory},
	{"Fly Ride", &CConfig::m_KxFlyRide},
	{"Hook Ride", &CConfig::m_KxHookRide},
	{"Balance Bot", &CConfig::m_KxBalanceBot},
	{"Jet Ride", &CConfig::m_KxJetRide},
	{"Triple Fly", &CConfig::m_KxTripleFly},
	{"Zoom Hack", &CConfig::m_KxZoomHack},
	{"Spectator List", &CConfig::m_KxSpecList},
};

void CBotNet::RenderArrayList()
{
	if(!g_Config.m_KxArrayList)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	float Width = 400.0f * pGame->Graphics()->ScreenAspect();
	pGame->Graphics()->MapScreen(0.0f, 0.0f, Width, 400.0f);

	const float FontSize = 6.0f;
	const float LineH = FontSize + 1.0f;
	const float Margin = 5.0f;

	float y = 5.0f;
	pGame->TextRender()->TextColor(1.0f, 1.0f, 0.3f, 1.0f);
	for(const auto &Feature : s_aArrayListFeatures)
	{
		if(g_Config.*Feature.m_pCvar == 0)
			continue;
		const float TextW = pGame->TextRender()->TextWidth(FontSize, Feature.m_pName);
		pGame->TextRender()->Text(Width - TextW - Margin, y, FontSize, Feature.m_pName, -1.0f);
		y += LineH;
	}
	pGame->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}
