#include <game/client/components/kinetix/kinetix.h>
#include <game/client/components/kinetix/kinetix_internal.h>

#include <base/io.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

std::string CBotNet::ReplacePlaceholders(const std::string &Cmd, int DummyIndex, CGameClient *pGame)
{
        std::string Result = Cmd;

        // {i} — rank of this dummy among connected dummies (1-based)
        // Original: online = sorted(control_clients.keys()); rank = online.index(client_index) + 1
        {
                // Collect connected dummy indices (sorted ascending, which they already are)
                int Online[MAX_DUMMIES];
                int Count = 0;
                for(int d = 0; d < MAX_DUMMIES; ++d)
                {
                        if(d != 0 && !pGame->Client()->DummyConnected(d))
                                continue;
                        if(pGame->m_aLocalIds[d] < 0)
                                continue;
                        Online[Count++] = d;
                }
                int Rank = DummyIndex + 1; // fallback if not found
                for(int a = 0; a < Count; ++a)
                {
                        if(Online[a] == DummyIndex)
                        {
                                Rank = a + 1;
                                break;
                        }
                }
                size_t Pos;
                while((Pos = Result.find("{i}")) != std::string::npos)
                        Result.replace(Pos, 3, std::to_string(Rank));
        }

        // {r} — random char from a-zA-Z0-9._-
        // Original: random.choice(string.ascii_letters + string.digits + "._-")
        {
                static const char RAND_CHARS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-";
                static const int RAND_CHARS_LEN = sizeof(RAND_CHARS) - 1;
                size_t Pos;
                while((Pos = Result.find("{r}")) != std::string::npos)
                {
                        char Ch = RAND_CHARS[rand() % RAND_CHARS_LEN];
                        Result.replace(Pos, 3, 1, Ch);
                }
        }

        // {ri-N} — random integer from 0 to N (inclusive)
        // Original: re.sub(r'{ri-(\d+)}', lambda m: str(random.randint(0, int(m.group(1)))), cmd)
        {
                size_t Pos;
                while((Pos = Result.find("{ri-")) != std::string::npos)
                {
                        size_t End = Result.find('}', Pos);
                        if(End != std::string::npos)
                        {
                                std::string NumStr = Result.substr(Pos + 4, End - Pos - 4);
                                int MaxVal = atoi(NumStr.c_str());
                                if(MaxVal < 1) MaxVal = 1;
                                int Val = rand() % (MaxVal + 1);
                                Result.replace(Pos, End - Pos + 1, std::to_string(Val));
                        }
                        else
                        {
                                break;
                        }
                }
        }

        // {c} — random CJK character U+4E00–U+9FFF
        // Original: chr(random.randint(0x4E00, 0x9FFF))
        {
                size_t Pos;
                while((Pos = Result.find("{c}")) != std::string::npos)
                {
                        int Codepoint = 0x4E00 + (rand() % (0x9FFF - 0x4E00 + 1));
                        char Utf8[5];
                        Utf8[0] = (char)(0xE0 | ((Codepoint >> 12) & 0x0F));
                        Utf8[1] = (char)(0x80 | ((Codepoint >> 6) & 0x3F));
                        Utf8[2] = (char)(0x80 | (Codepoint & 0x3F));
                        Utf8[3] = '\0';
                        Result.replace(Pos, 3, Utf8);
                }
        }

        return Result;
}

void CBotNet::ConSendDummy(IConsole::IResult *pResult, void *pUserData)
{
        CBotNet *pSelf = (CBotNet *)pUserData;
        const char *pIds = pResult->GetString(0);
        const char *pCmd = pResult->GetString(1);

        if(!pCmd[0])
                return;

        // Parse dummy IDs
        bool aTargets[MAX_DUMMIES] = {};
        bool AllDummies = false;

        if(str_comp(pIds, "-1") == 0)
        {
                AllDummies = true;
        }
        else
        {
                // Parse comma-separated IDs: "0,1,2,3"
                char aBuf[64];
                str_copy(aBuf, pIds, sizeof(aBuf));
                char *pTok = strtok(aBuf, ",");
                while(pTok)
                {
                        int Id = atoi(pTok);
                        if(Id >= 0 && Id < MAX_DUMMIES)
                                aTargets[Id] = true;
                        pTok = strtok(nullptr, ",");
                }
        }

        // Save current dummy
        int SavedDummy = g_Config.m_ClDummy;

        // Execute command on each target dummy
        for(int D = 0; D < MAX_DUMMIES; D++)
        {
                if(!AllDummies && !aTargets[D])
                        continue;
                // Skip disconnected dummies (0=main, always connected if online)
                if(D != 0 && !pSelf->Client()->DummyConnected(D))
                        continue;

                // Replace placeholders per-dummy
                std::string Cmd = ReplacePlaceholders(pCmd, D, pSelf->GameClient());

                g_Config.m_ClDummy = D;
                pSelf->Console()->ExecuteLine(Cmd.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
        }

        // Restore original dummy
        g_Config.m_ClDummy = SavedDummy;
}

// =========================================================
// KINODYNAMIC A* — CONSOLE COMMANDS
// =========================================================

void CBotNet::ConPfLive(IConsole::IResult *pResult, void *pUserData)
{
        CBotNet *p = (CBotNet *)pUserData;
        int s = pResult->GetInteger(0);
        if(s < 0 || s > 2)
                return;
        if(s == PF_STATE_RUNNING && p->m_PfState != PF_STATE_RUNNING)
                p->PfResetRun();
        p->m_PfState = s;
        if(s == PF_STATE_IDLE)
			{
				p->PfThreadStop();
				std::lock_guard<std::mutex> lock(p->m_PfDataMutex);
				p->m_PfVPath.clear();
			}
}

void CBotNet::ConPfPaste(IConsole::IResult *pResult, void *pUserData)
{
        ((CBotNet *)pUserData)->PfPasteToTas();
}

// Reset run: anchor to active player's predicted core, build flow field.

// =========================================================
// SETTINGS SAVE/LOAD — kx_save / kx_load
// Saves all Kx cvars to a config file and loads them back.
// =========================================================

struct SCvarSaveCtx
{
	const char *pName;
	bool Found;
	char aLine[512];
};

static void CvarSaveCallback(const SConfigVariable *pVariable, void *pUserData)
{
	SCvarSaveCtx *pCtx = (SCvarSaveCtx *)pUserData;
	if(pCtx->Found || str_comp(pVariable->m_pScriptName, pCtx->pName) != 0)
		return;
	pCtx->Found = true;
	pVariable->Serialize(pCtx->aLine, sizeof(pCtx->aLine));
}

void CBotNet::ConSaveSettings(IConsole::IResult *pResult, void *pUserData)
{
	CBotNet *pSelf = (CBotNet *)pUserData;
	const char *pFilename = pResult->NumArguments() > 0 ? pResult->GetString(0) : g_Config.m_KxSettingsFile;
	if(!pFilename[0])
		pFilename = "kinetix_settings.cfg";

	IOHANDLE File = pSelf->Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		dbg_msg("kinetix", "Failed to open %s for writing", pFilename);
		return;
	}

	const char *Cvars[] = {
		"kx_aimbot", "kx_triggerbot",
		"kx_attack", "kx_stand", "kx_autoaim", "kx_autofire", "kx_autohook",
		"kx_move", "kx_rescue", "kx_rescue_all", "kx_smart_detect", "kx_smart_rescue",
		"kx_kill_frz", "kx_atk_main", "kx_hammer", "kx_avoid_freeze", "kx_pf_hook",
		"kx_kinodynamic", "kx_atk_pathfinder", "kx_atk_pathfinder_rays", "kx_atk_pathfinder_rays_dist",
		"kx_atk_pathfinder_snap", "kx_atk_pathfinder_sps", "kx_pf_simulate_players",
		"kx_fire_dist", "kx_hook_dist", "kx_rescue_radius", "kx_target_dist",
		"kx_main_dist", "kx_stand_dist", "kx_main_stand_dist", "kx_target_all",
		"kx_main", "kx_copy_moves", "kx_copy_target_id", "kx_random_aim",
		"kx_random_aim_interval", "kx_client_delay", "kx_stand_on_x",
		"kx_atk_hook_delay", "kx_laser_rescue", "kx_laser_rescue_dist",
		"kx_esp", "kx_esp_box", "kx_esp_hitbox", "kx_esp_style", "kx_esp_speed",
		"kx_fake_aim", "kx_fake_aim_mode", "kx_fake_aim_speed", "kx_fake_aim_distance",
		"kx_fake_aim_show_for_me",
		"kx_baf_avoid_freeze", "kx_baf_avoid_teleport",
		"kx_baf_avoid_death", "kx_baf_direction", "kx_baf_jump", "kx_baf_hook",
		"kx_baf_aim", "kx_baf_fov", "kx_baf_angles", "kx_baf_silent",
		"kx_baf_ticks", "kx_baf_trigger_ticks",
		"kx_fly_ride", "kx_fly_ride_speed", "kx_fly_ride_deadzone",
		"kx_triple_fly", "kx_triple_fly_dummy_mode", "kx_triple_fly_dummy_id",
		"kx_triple_fly_target_mode", "kx_triple_fly_trigger_radius", "kx_triple_fly_radius",
		"kx_balance_bot", "kx_balance_bot_radius",
		"kx_hook_ride", "kx_hook_ride_radius",
		"kx_jet_ride", "kx_jet_ride_radius",
		"kx_kino_candidates", "kx_kino_ticks", "kx_kino_hook_angles",
		"kx_kino_cache_ticks", "kx_kino_show_path", "kx_kino_show_field", "kx_kino_aggressive",
		"kx_show_trajectory",
		"kx_zoom_hack",
\t\t\"kx_spec_list\",\n\t\t\"kx_array_list\",
	};

	int count = sizeof(Cvars) / sizeof(Cvars[0]);
	int saved = 0;
	for(int i = 0; i < count; i++)
	{
		SCvarSaveCtx Ctx;
		Ctx.pName = Cvars[i];
		Ctx.Found = false;
		Ctx.aLine[0] = 0;
		pSelf->ConfigManager()->PossibleConfigVariables(Cvars[i], CFGFLAG_CLIENT, CvarSaveCallback, &Ctx);
		if(!Ctx.Found)
		{
			dbg_msg("kinetix", "skipping unknown cvar '%s'", Cvars[i]);
			continue;
		}
		io_write(File, Ctx.aLine, str_length(Ctx.aLine));
		io_write_newline(File);
		saved++;
	}

	io_close(File);
	dbg_msg("kinetix", "Saved %d settings to %s", saved, pFilename);
}

void CBotNet::ConLoadSettings(IConsole::IResult *pResult, void *pUserData)
{
	CBotNet *pSelf = (CBotNet *)pUserData;
	const char *pFilename = pResult->NumArguments() > 0 ? pResult->GetString(0) : g_Config.m_KxSettingsFile;
	if(!pFilename[0])
		pFilename = "kinetix_settings.cfg";

	IOHANDLE File = pSelf->Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
	{
		dbg_msg("kinetix", "Failed to open %s for reading", pFilename);
		return;
	}

	char aBuf[4096];
	int read = io_read(File, aBuf, sizeof(aBuf) - 1);
	io_close(File);
	if(read <= 0)
	{
		dbg_msg("kinetix", "Settings file %s is empty or unreadable", pFilename);
		return;
	}
	aBuf[read] = '\0';

	char *pLine = aBuf;
	int loaded = 0;
	while(*pLine)
	{
		char *pEnd = pLine;
		while(*pEnd && *pEnd != '\n')
			pEnd++;
		if(*pEnd)
			*pEnd++ = '\0';

		if(pLine[0] && pLine[0] != '#')
		{
			pSelf->Console()->ExecuteLine(pLine, IConsole::CLIENT_ID_UNSPECIFIED);
			loaded++;
		}

		pLine = pEnd;
	}

	dbg_msg("kinetix", "Loaded %d settings from %s", loaded, pFilename);
}
