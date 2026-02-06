#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "UOmniSyncConfig.generated.h"

UENUM( BlueprintType )
enum class EOmniSyncScope : uint8
{
	Global,
	PerEngineVersion,
	PerProject,
};

USTRUCT( BlueprintType )
struct FConfigFileSettings
{
	GENERATED_BODY()

	UPROPERTY( VisibleAnywhere )
	FString FileName;

	UPROPERTY( VisibleAnywhere )
	FString RelativePath;

	UPROPERTY( EditAnywhere )
	bool bEnabled = false;

	UPROPERTY( EditAnywhere )
	EOmniSyncScope SettingsScope = EOmniSyncScope::PerEngineVersion;

	UPROPERTY( EditAnywhere )
	bool bAutoSyncEnabled = true;
};

USTRUCT()
struct FConfigFileSettingsStruct
{
	GENERATED_BODY()

	UPROPERTY( EditAnywhere )
	TArray< FConfigFileSettings > Settings;
};

UCLASS()
class OMNISYNC_API UOmniSyncConfig : public UObject
{
	GENERATED_BODY()

public:
	static UOmniSyncConfig* Get();

	void Initialize() { EnableAutoSync(); }
	void Shutdown() const { DisableAutoSync(); }

	void DiscoverAndAddConfigFiles();
	void SaveSettingsToGlobal();
	void LoadSettingsFromGlobal();

	void OnSettingsChanged();

	UPROPERTY( EditAnywhere )
	FConfigFileSettingsStruct ConfigFileSettingsStruct;

private:
	void SavePluginSettings() const;
	void LoadPluginSettings();

	void EnableAutoSync();
	void DisableAutoSync() const;

	bool AutoSyncTick( float DeltaTime );

	static bool CopyIniFile( const FString& Source, const FString& Destination );
	static bool EnsureDirectoryExists( const FString& DirectoryPath );

	static FString GetScopedSettingsDirectory( EOmniSyncScope Scope );
	static FString GetPluginSettingsFilePath();

	FTSTicker::FDelegateHandle AutoSyncHandle;

	static UOmniSyncConfig* Instance;
};