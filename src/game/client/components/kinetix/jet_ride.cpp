#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <base/vmath.h>
#include <base/math.h>
#include <engine/keys.h>

// JetRide: jetpack flight with position correction.
// Dummy hammers pilot (jetpack effect). Pilot corrects position to stay
// at anchor — smooth, controlled hovering instead of jerky free-fly.
// WASD moves anchor slowly. X correction via direction, Y via hammer timing.

void CBotNet::UpdateJetRide()
{
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	bool enabled = g_Config.m_KxJetRide != 0;

	if(enabled && !m_JetRideWasActive)
	{
		m_JetRideWasActive = true;
		m_JetRideTargetDummy = -1;
	}
	else if(!enabled && m_JetRideWasActive)
	{
		if(m_JetRideTargetDummy >= 0 && m_JetRideTargetDummy < MAX_DUMMIES)
		{
			CNetObj_PlayerInput *pDummy = &pGame->m_aDummyInput[m_JetRideTargetDummy];
			if(pDummy->m_Fire & 1)
				pDummy->m_Fire = (pDummy->m_Fire + 1) & ~1;
			pGame->m_Controls.m_aInputData[m_JetRideTargetDummy] = *pDummy;
		}
		m_JetRideWasActive = false;
		m_JetRideTargetDummy = -1;
		return;
	}

	if(!enabled)
		return;

	if(!pGame->m_Snap.m_pLocalInfo)
		return;

	const vec2 pilotPos = pGame->m_PredictedChar.m_Pos;

	const int curTick = Client()->GameTick(g_Config.m_ClDummy);

	int nearestD = -1;
	float nearestDistSq = 1e18f;
	vec2 nearestDummyPos = pilotPos;
	const int activeD = g_Config.m_ClDummy;

	for(int D = 0; D < MAX_DUMMIES; D++)
	{
		if(D == activeD)
			continue;
		if(D != 0 && !Client()->DummyConnected(D))
			continue;
		const int cid = pGame->m_aLocalIds[D];
		if(cid < 0 || cid >= 128)
			continue;
		if(!pGame->m_aClients[cid].m_Active)
			continue;

		const vec2 dPos = pGame->m_aClients[cid].m_Predicted.m_Pos;
		const float dsq = length_squared(dPos - pilotPos);
		if(dsq < nearestDistSq)
		{
			nearestDistSq = dsq;
			nearestD = D;
			nearestDummyPos = dPos;
		}
	}

	if(nearestD < 0)
	{
		m_JetRideTargetDummy = -1;
		return;
	}

	m_JetRideTargetDummy = nearestD;

	const float dist = std::sqrt(nearestDistSq);
	const float triggerR = (float)g_Config.m_KxJetRideRadius;

	CNetObj_PlayerInput *pDummy = &pGame->m_aDummyInput[nearestD];

	if(dist <= triggerR)
	{
		pDummy->m_WantedWeapon = WEAPON_HAMMER + 1;
		const vec2 aim = pilotPos - nearestDummyPos;
		pDummy->m_TargetX = (int)aim.x;
		pDummy->m_TargetY = (int)aim.y;
		pDummy->m_Fire = (pDummy->m_Fire + 1) | 1;
	}
	else
	{
		if(pDummy->m_Fire & 1)
			pDummy->m_Fire = (pDummy->m_Fire + 1) & ~1;
	}

	float dx = pilotPos.x - nearestDummyPos.x;
	float dummyDir = 0.0f;
	if(std::abs(dx) > 8.0f)
		dummyDir = (dx > 0) ? 1.0f : -1.0f;
	pDummy->m_Direction = (int)dummyDir;
	pGame->m_Controls.m_aInputData[nearestD] = *pDummy;

	CNetObj_PlayerInput *pPilot = &pGame->m_Controls.m_aInputData[g_Config.m_ClDummy];

	float pdx = pilotPos.x - nearestDummyPos.x;
	int pilotDir = 0;
	if(std::abs(pdx) > 10.0f)
		pilotDir = (pdx > 0) ? -1 : 1;
	pPilot->m_Direction = pilotDir;
	pPilot->m_Hook = 0;
	pPilot->m_PlayerFlags |= 1;
}
