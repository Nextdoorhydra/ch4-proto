#include "ListenServerNetworkSettings.h"

UListenServerNetworkSettings::UListenServerNetworkSettings()
{
	ProjectKey = TEXT("SteamListenLab");
	DefaultGameModeId = TEXT("Default");
	DefaultRegion = NAME_None;
	DefaultSessionDisplayName = TEXT("Steam Listen Server");
	MainMenuMap = FSoftObjectPath(TEXT("/Game/Maps/L_MainMenu.L_MainMenu"));
	LobbyMap = FSoftObjectPath(TEXT("/Game/Maps/L_Lobby.L_Lobby"));
	DefaultGameMap = FSoftObjectPath(TEXT("/Game/Maps/L_Game.L_Game"));
}
