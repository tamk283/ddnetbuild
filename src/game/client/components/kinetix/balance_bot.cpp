#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <base/vmath.h>
#include <base/math.h>

// BalanceBot: the nearest connected dummy tries to stand on the head of the
// nearest player. The dummy hooks onto the target — the grabbed hook follows
// the target — jumps and walks to climb onto the head and stay centered.

void CBotNet::UpdateBalanceBot()
{
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	bool enabled = g_Config.m_KxBalanceBot != 0;

	if(enabled && !m_BalanceBotWasActive)
	{
		m_BalanceBotWasActive = true;
		m_BalanceBotTargetId = -1;
	}
	else if(!enabled && m_BalanceBotWasActive)
	{
		// Reset every controlled dummy.
		for(int D = 0; D < MAX_DUMMIES; D++)
		{
			if(D == g_Config.m_ClDummy || (D != 0 && !Client()->DummyConnected(D)))
				continue;
			CNetObj_PlayerInput *pInput = &pGame->m_aDummyInput[D];
			pInput->m_Hook = 0;
			pInput->m_Jump = 0;
			pInput->m_Direction = 0;
			pGame->m_Controls.m_aInputData[D] = *pInput;
		}
		m_BalanceBotWasActive = false;
		m_BalanceBotTargetId = -1;
		return;
	}

	if(!enabled)
		return;

	if(!pGame->m_Snap.m_pLocalInfo)
		return;

	const int activeD = g_Config.m_ClDummy;
	const vec2 pilotPos = pGame->m_PredictedChar.m_Pos;

	// ── Nearest connected dummy is the balance bot ──
	int botD = -1;
	float nearestBotDistSq = 1e18f;
	vec2 botPos = vec2(0, 0);
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
		if(dsq < nearestBotDistSq)
		{
			nearestBotDistSq = dsq;
			botD = D;
			botPos = dPos;
		}
	}

	if(botD < 0)
		return;
	const int botCid = pGame->m_aLocalIds[botD];

	// ── Nearest player to the bot is the balance target ──
	const float maxR = (float)g_Config.m_KxBalanceBotRadius;
	int bestTarget = -1;
	float bestDistSq = maxR * maxR;
	for(int i = 0; i < 128; i++)
	{
		if(i == botCid)
			continue;
		if(!pGame->m_aClients[i].m_Active)
			continue;
		if(!pGame->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(pGame->m_aClients[i].m_FreezeEnd > 0)
			continue;
		const vec2 tPos = pGame->m_aClients[i].m_Predicted.m_Pos;
		const float dSq = length_squared(tPos - botPos);
		if(dSq < bestDistSq)
		{
			bestDistSq = dSq;
			bestTarget = i;
		}
	}

	m_BalanceBotTargetId = bestTarget;
	if(bestTarget < 0)
	{
		// No target in range: stand still, release everything.
		CNetObj_PlayerInput *pReset = &pGame->m_aDummyInput[botD];
		pReset->m_Hook = 0;
		pReset->m_Jump = 0;
		pReset->m_Direction = 0;
		pGame->m_Controls.m_aInputData[botD] = *pReset;
		return;
	}

	const vec2 targetPos = pGame->m_aClients[bestTarget].m_Predicted.m_Pos;
	const float headY = targetPos.y - CCharacterCore::PhysicalSize();
	const float dx = targetPos.x - botPos.x;
	const float dist = length(targetPos - botPos);

	CNetObj_PlayerInput *pInput = &pGame->m_aDummyInput[botD];

	// ── Horizontal balance: walk to stay centered over the target ──
	if(std::abs(dx) > 4.0f)
		pInput->m_Direction = (dx > 0) ? 1 : -1;
	else
		pInput->m_Direction = 0;

	// ── Below head level: jump to climb up ──
	if(botPos.y > headY + 6.0f && std::abs(dx) < 40.0f)
		pInput->m_Jump = 1;
	else
		pInput->m_Jump = 0;

	// ── Hook onto the target: the grab follows the target and pulls the bot
	// onto the head; DDNet auto-releases the hook, so it re-fires each cycle ──
	if(dist > 16.0f && dist <= maxR)
	{
		SetMousePos(pGame, botD, targetPos - botPos);
		pInput->m_Hook = 1;
	}
	else
	{
		pInput->m_Hook = 0;
	}

	pGame->m_Controls.m_aInputData[botD] = *pInput;
}
