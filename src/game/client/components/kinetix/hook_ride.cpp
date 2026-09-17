#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/collision.h>

#include <base/vmath.h>
#include <base/math.h>
#include <engine/keys.h>

// HookRide: active dummy auto-hooks to solid tiles and rides like a cable car.
// WASD selects direction. Scans for solid walls in that direction, hooks to
// the nearest one, holds while being pulled in, releases when close.

void CBotNet::UpdateHookRide()
{
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	bool enabled = g_Config.m_KxHookRide != 0;

	if(enabled && !m_HookRideWasActive)
	{
		m_HookRideWasActive = true;
		m_HookRideHookTile = vec2(0, 0);
		m_HookRideHookTimer = 0;
	}
	else if(!enabled && m_HookRideWasActive)
	{
		CNetObj_PlayerInput *pInput = &pGame->m_Controls.m_aInputData[g_Config.m_ClDummy];
		pInput->m_Hook = 0;
		m_HookRideWasActive = false;
		m_HookRideHookTile = vec2(0, 0);
		m_HookRideHookTimer = 0;
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

	float dirX = 0.0f, dirY = 0.0f;
	if(Input()->KeyIsPressed(KEY_W))
		dirY -= 1.0f;
	if(Input()->KeyIsPressed(KEY_S))
		dirY += 1.0f;
	if(Input()->KeyIsPressed(KEY_A))
		dirX -= 1.0f;
	if(Input()->KeyIsPressed(KEY_D))
		dirX += 1.0f;

	CNetObj_PlayerInput *pInput = &pGame->m_Controls.m_aInputData[g_Config.m_ClDummy];

	float hookDist = (float)g_Config.m_KxHookRideRadius;

	bool hasDir = (dirX != 0.0f || dirY != 0.0f);

	if(hasDir)
	{
		float len = sqrtf(dirX * dirX + dirY * dirY);
		dirX /= len;
		dirY /= len;

		vec2 rayEnd = myPos + vec2(dirX, dirY) * hookDist;
		vec2 hitPos;
		int hit = pGame->Collision()->IntersectLine(myPos, rayEnd, &hitPos, nullptr);

		if(hit > 0)
		{
			vec2 aim = hitPos - myPos;
			float distToHit = length(aim);

			if(distToHit > 32.0f)
			{
				m_HookRideHookTile = hitPos;
				SetMousePos(pGame, g_Config.m_ClDummy, aim);
				pInput->m_Hook = 1;
				m_HookRideHookTimer = 0;
				return;
			}
			else
			{
				pInput->m_Hook = 0;
				m_HookRideHookTimer = 0;
				return;
			}
		}
		else
		{
			vec2 aim2 = rayEnd - myPos;
			SetMousePos(pGame, g_Config.m_ClDummy, aim2);
			pInput->m_Hook = 1;
			m_HookRideHookTimer = 0;
			return;
		}
	}

	if(m_HookRideHookTile.x != 0.0f || m_HookRideHookTile.y != 0.0f)
	{
		float distToTile = distance(myPos, m_HookRideHookTile);
		if(distToTile > 40.0f)
		{
			vec2 aim = m_HookRideHookTile - myPos;
			SetMousePos(pGame, g_Config.m_ClDummy, aim);
			pInput->m_Hook = 1;
		}
		else
		{
			pInput->m_Hook = 0;
			m_HookRideHookTile = vec2(0, 0);
		}
	}
	else
	{
		pInput->m_Hook = 0;
	}

	pInput->m_PlayerFlags |= 1;
}
