#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <base/vmath.h>
#include <base/math.h>

// BalanceBot: active dummy balances on the head of the nearest player.
// Finds nearest non-frozen player within radius, moves to match their X,
// jumps if below their head level. No hook, no fire — pure balance.

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
		m_BalanceBotWasActive = false;
		m_BalanceBotTargetId = -1;
		return;
	}

	if(!enabled)
		return;

	if(!pGame->m_Snap.m_pLocalInfo)
		return;

	int LocalId = pGame->m_Snap.m_LocalClientId;
	if(LocalId < 0)
		return;

	CCharacter *pLocalChar = pGame->m_PredictedWorld.GetCharacterById(LocalId);
	if(!pLocalChar)
		return;

	vec2 myPos = pLocalChar->Core()->m_Pos;

	int bestTarget = -1;
	float bestDistSq = 1e18f;
	float maxR = (float)g_Config.m_KxBalanceBotRadius;

	for(int i = 0; i < 128; i++)
	{
		if(i == LocalId)
			continue;
		if(!pGame->m_aClients[i].m_Active)
			continue;
		if(!pGame->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(pGame->m_aClients[i].m_FreezeEnd > 0)
			continue;

		vec2 tPos = pGame->m_aClients[i].m_Predicted.m_Pos;
		float dSq = length_squared(tPos - myPos);
		if(dSq > maxR * maxR)
			continue;

		if(dSq < bestDistSq)
		{
			bestDistSq = dSq;
			bestTarget = i;
		}
	}

	m_BalanceBotTargetId = bestTarget;
	if(bestTarget < 0)
		return;

	vec2 targetPos = pGame->m_aClients[bestTarget].m_Predicted.m_Pos;

	float dx = targetPos.x - myPos.x;
	float physSize = CCharacterCore::PhysicalSize();
	float headY = targetPos.y - physSize;

	CNetObj_PlayerInput *pInput = &pGame->m_Controls.m_aInputData[g_Config.m_ClDummy];

	if(std::abs(dx) > 4.0f)
		pInput->m_Direction = (dx > 0) ? 1 : -1;
	else
		pInput->m_Direction = 0;

	if(myPos.y > headY - 2.0f)
		pInput->m_Jump = 1;
	else
		pInput->m_Jump = 0;

	pInput->m_Hook = 0;
	pInput->m_PlayerFlags |= 1;
}
