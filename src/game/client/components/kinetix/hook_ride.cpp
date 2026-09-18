#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/collision.h>

#include <base/vmath.h>
#include <base/math.h>
#include <engine/keys.h>

// HookRide: red anchor point moves with the pilot's walking (left/right).
// The nearest dummy auto-hooks into solid blocks — in any direction — to
// hold its position at the anchor. When the anchor shifts away from the
// dummy, the dummy hooks into the nearest block in the shift direction.

void CBotNet::UpdateHookRide()
{
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	bool enabled = g_Config.m_KxHookRide != 0;

	if(enabled && !m_HookRideWasActive)
	{
		// Anchor starts at the pilot position.
		m_HookRideHookTile = pGame->m_PredictedChar.m_Pos;
		m_HookRideLastAnchorTick = -1;
		m_HookRideHookTimer = 0;
		m_HookRideWasActive = true;
	}
	else if(!enabled && m_HookRideWasActive)
	{
		// Release hooks on every controlled dummy.
		for(int D = 0; D < MAX_DUMMIES; D++)
		{
			if(D == g_Config.m_ClDummy || (D != 0 && !Client()->DummyConnected(D)))
				continue;
			CNetObj_PlayerInput *pInput = &pGame->m_aDummyInput[D];
			if(pInput->m_Hook)
			{
				pInput->m_Hook = 0;
				pGame->m_Controls.m_aInputData[D] = *pInput;
			}
		}
		m_HookRideWasActive = false;
		m_HookRideHookTile = vec2(0, 0);
		m_HookRideHookTimer = 0;
		return;
	}

	if(!enabled)
		return;

	if(!pGame->m_Snap.m_pLocalInfo)
		return;

	const int activeD = g_Config.m_ClDummy;

	// ── Per-tick anchor update: moves with the pilot's walking ──
	const int curTick = Client()->GameTick(activeD);
	if(m_HookRideLastAnchorTick != curTick)
	{
		m_HookRideLastAnchorTick = curTick;
		int dir = 0;
		if(pGame->m_Controls.m_aInputDirectionLeft[activeD])
			dir -= 1;
		if(pGame->m_Controls.m_aInputDirectionRight[activeD])
			dir += 1;
		m_HookRideHookTile.x += dir * 8.0f;
	}

	// ── Nearest dummy becomes the hook bot ──
	int nearestD = -1;
	float nearestDistSq = 1e18f;
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
		const float dsq = length_squared(dPos - m_HookRideHookTile);
		if(dsq < nearestDistSq)
		{
			nearestDistSq = dsq;
			nearestD = D;
			botPos = dPos;
		}
	}

	if(nearestD < 0)
		return;

	CNetObj_PlayerInput *pInput = &pGame->m_aDummyInput[nearestD];
	const vec2 delta = m_HookRideHookTile - botPos;
	const float dist = length(delta);

	// ── Inside deadzone: hold position, no hook ──
	if(dist <= 48.0f)
	{
		pInput->m_Hook = 0;
		pInput->m_Direction = 0;
		m_HookRideHookTimer = 0;
		pGame->m_Controls.m_aInputData[nearestD] = *pInput;
		return;
	}

	// ── Anchor shifted: hook into the nearest block in the shift direction ──
	const vec2 dir = delta / dist;
	const float hookDist = (float)g_Config.m_KxHookRideRadius;
	const vec2 rayEnd = botPos + dir * hookDist;
	vec2 hitPos;
	const int hit = pGame->Collision()->IntersectLine(botPos, rayEnd, &hitPos, nullptr);

	m_HookRideHookTimer++;

	if(hit > 0 && distance(botPos, hitPos) > 28.0f)
	{
		// Grab the block — the pull carries the bot toward the anchor.
		pInput->m_Hook = 1;
		SetMousePos(pGame, nearestD, hitPos - botPos);
	}
	else
	{
		// No block on the ray: aim along the shift and walk toward the anchor.
		pInput->m_Hook = 0;
		pInput->m_Direction = (delta.x > 4.0f) ? 1 : ((delta.x < -4.0f) ? -1 : 0);
		SetMousePos(pGame, nearestD, dir * hookDist);
	}

	pGame->m_Controls.m_aInputData[nearestD] = *pInput;
}

// Red semi-transparent tile at the anchor point (same style as FlyRide).
void CBotNet::RenderHookRideAnchor()
{
	if(!g_Config.m_KxHookRide)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	const vec2 p = m_HookRideHookTile;
	const float s = 16.0f;

	pGame->Graphics()->TextureClear();
	pGame->Graphics()->BlendNormal();
	pGame->Graphics()->QuadsBegin();
	pGame->Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.5f);
	IGraphics::CFreeformItem Quad(
		p.x - s, p.y - s, p.x + s, p.y - s,
		p.x - s, p.y + s, p.x + s, p.y + s);
	pGame->Graphics()->QuadsDrawFreeform(&Quad, 1);
	pGame->Graphics()->QuadsEnd();
}
