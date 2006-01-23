#include "precompiled.h"

#include "Game.h"
#include "GameAttributes.h"
#include "CLogger.h"
#ifndef NO_GUI
#include "gui/CGUI.h"
#endif
#include "timer.h"
#include "Profile.h"
#include "Loader.h"
#include "CStr.h"
#include "EntityManager.h"
#include "CConsole.h"

extern CConsole* g_Console;
CGame *g_Game=NULL;

// Disable "warning C4355: 'this' : used in base member initializer list".
//   "The base-class constructors and class member constructors are called before
//   this constructor. In effect, you've passed a pointer to an unconstructed
//   object to another constructor. If those other constructors access any
//   members or call member functions on this, the result will be undefined."
// In this case, the pointers are simply stored for later use, so there
// should be no problem.
#if MSC_VERSION
# pragma warning (disable: 4355)
#endif

CGame::CGame():
	m_World(this),
	m_Simulation(this),
	m_GameView(this),
	m_pLocalPlayer(NULL),
	m_GameStarted(false),
	m_Paused(false),
	m_Time(0)
{
	debug_printf("CGame::CGame(): Game object CREATED; initializing..\n");
}

#if MSC_VERSION
# pragma warning (default: 4355)
#endif

CGame::~CGame()
{
	// Again, the in-game call tree is going to be different to the main menu one.
	g_Profiler.StructuralReset();
	debug_printf("CGame::~CGame(): Game object DESTROYED\n");
}



PSRETURN CGame::RegisterInit(CGameAttributes* pAttribs)
{
	LDR_BeginRegistering();

	// RC, 040804 - GameView needs to be initialised before World, otherwise GameView initialisation
	// overwrites anything stored in the map file that gets loaded by CWorld::Initialize with default
	// values.  At the minute, it's just lighting settings, but could be extended to store camera position.  
	// Storing lighting settings in the gameview seems a little odd, but it's no big deal; maybe move it at 
	// some point to be stored in the world object?
	m_GameView.RegisterInit(pAttribs);
	m_World.RegisterInit(pAttribs);
	m_Simulation.RegisterInit(pAttribs);

	LDR_EndRegistering();
	return 0;
}
PSRETURN CGame::ReallyStartGame()
{
#ifndef NO_GUI

	// Call the reallyStartGame function, but only if it exists
	jsval fval, rval;
	JSBool ok = JS_GetProperty(g_ScriptingHost.getContext(), g_GUI.GetScriptObject(), "reallyStartGame", &fval);
	debug_assert(ok);
	if (ok && !JSVAL_IS_VOID(fval))
	{
		ok = JS_CallFunctionValue(g_ScriptingHost.getContext(), g_GUI.GetScriptObject(), fval, 0, NULL, &rval);
		debug_assert(ok);
	}
#endif

	debug_printf("GAME STARTED, ALL INIT COMPLETE\n");
	m_GameStarted=true;

	// The call tree we've built for pregame probably isn't useful in-game.
	g_Profiler.StructuralReset();

#ifndef NO_GUI
	g_GUI.SendEventToAll("sessionstart");
#endif

	return 0;
}

PSRETURN CGame::StartGame(CGameAttributes *pAttribs)
{
	try
	{
		// JW: this loop is taken from ScEd and fixes lack of player color.
		// TODO: determine proper number of players.
		for (int i=1; i<8; ++i) 
			pAttribs->GetSlot(i)->AssignLocal();

		pAttribs->FinalizeSlots();
		m_NumPlayers=pAttribs->GetSlotCount();

		// Player 0 = Gaia - allocate one extra
		m_Players.resize(m_NumPlayers + 1);

		for (uint i=0;i <= m_NumPlayers;i++)
			m_Players[i]=pAttribs->GetPlayer(i);
		
		m_pLocalPlayer=m_Players[1];

		RegisterInit(pAttribs);
	}
	catch (PSERROR_Game& e)
	{
		return e.getCode();
	}
	return 0;
}

void CGame::Update(double deltaTime)
{
	if( m_Paused )
	{
		return;
	}

	m_Time += deltaTime;

	m_Simulation.Update(deltaTime);
	
	// TODO Detect game over and bring up the summary screen or something
	// ^ Quick game over hack is implemented, no summary screen however
	if ( m_World.GetEntityManager()->GetDeath() )
	{
			UpdateGameStatus();
		if (GameStatus != 0)
			EndGame();
	}
	//reset death event flag
	m_World.GetEntityManager()->SetDeath(false);
}
void CGame::UpdateGameStatus()
{
	bool EOG_lose = true;
	bool EOG_win = true;
	CPlayer *local = GetLocalPlayer();

	for (int i=0; i<MAX_HANDLES; i++)
	{	
		CHandle *handle = m_World.GetEntityManager()->getHandle(i);
		if ( !handle )
			continue;
		CPlayer *tmpPlayer = handle->m_entity->GetPlayer();
		
		//Are we still alive?
		if ( local == tmpPlayer && handle->m_entity->m_extant )
		{	
			EOG_lose = false;
			if (EOG_win == false)
				break;
		}
		//Are they still alive?
		else if ( handle->m_entity->m_extant )
		{
			EOG_win = false;
			if (EOG_lose == false)
				break;
		}
	}
	if (EOG_lose && EOG_win)
		GameStatus = EOG_SPECIAL_DRAW;
	else if (EOG_win)
		GameStatus = EOG_WIN;
	else if (EOG_lose)	
		GameStatus = EOG_LOSE;
	else
		GameStatus = EOG_NEUTRAL;
}

void CGame::EndGame()
{
	g_Console->InsertMessage( L"It's the end of the game as we know it!");
	switch (GameStatus)
	{
	case EOG_DRAW:
		g_Console->InsertMessage( L"A diplomatic draw ain't so bad, eh?");
		break;
	case EOG_SPECIAL_DRAW:
		g_Console->InsertMessage( L"Amazingly, you managed to draw from dieing at the same time as your opponent...you have my respect.");
		break;
	case EOG_LOSE:
	    g_Console->InsertMessage( L"My condolences on your loss.");
		break;
	case EOG_WIN:
		g_Console->InsertMessage( L"Thou art victorious!");
		break;
	default:
		break;
	}
}
CPlayer *CGame::GetPlayer(uint idx)
{
	if (idx > m_NumPlayers)
	{
//		debug_warn("Invalid player ID");
//		LOG(ERROR, "", "Invalid player ID %d (outside 0..%d)", idx, m_NumPlayers);
		return m_Players[0];
	}
	// Be a bit more paranoid - maybe m_Players hasn't been set large enough
	else if (idx >= m_Players.size())
	{
		debug_warn("Invalid player ID");
		LOG(ERROR, "", "Invalid player ID %d (not <=%d - internal error?)", idx, m_Players.size());

		if (m_Players.size() != 0)
			return m_Players[0];
		else
			return NULL; // the caller will probably crash because of this,
			             // but at least we've reported the error
	}
	else
		return m_Players[idx];
}
