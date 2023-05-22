﻿#include "ModTargetsConfig.h"

#include "Interfaces/IPluginManager.h"

FModTargetsConfig::FModTargetsConfig(TSharedRef<IPlugin> InMod) {
	Mod = InMod;

	ConfigFile.Read(GetConfigPath());

	TArray<FString> Targets;
	ConfigFile.GetArray(TEXT("ModTargets"), TEXT("Targets"), Targets);
	bWindowsNoEditor = Targets.Contains(TEXT("WindowsNoEditor")) || Targets.Num() == 0;
	bWindowsServer = Targets.Contains(TEXT("WindowsServer"));
	bLinuxServer = Targets.Contains(TEXT("LinuxServer"));
}

void FModTargetsConfig::Save() {
	TArray<FString> Targets;
	if (bWindowsNoEditor)
		Targets.Add(TEXT("WindowsNoEditor"));
	if (bWindowsServer)
		Targets.Add(TEXT("WindowsServer"));
	if (bLinuxServer)
		Targets.Add(TEXT("LinuxServer"));

	ConfigFile.SetArray(TEXT("ModTargets"), TEXT("Targets"), Targets);
	ConfigFile.Write(GetConfigPath());
}

FString FModTargetsConfig::GetConfigPath() const {
	return FPaths::Combine(Mod->GetBaseDir(), TEXT("Config"), TEXT("Alpakit.ini"));
}