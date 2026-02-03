#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "UGlobalSettingSyncerConfig.generated.h"

UENUM( BlueprintType )
enum class EGlobalSettingSyncerScope : uint8
{
	Global,
	PerEngineVersion,
	PerProject,
};

USTRUCT( BlueprintType )
struct FConfigFileSettings
{
	GENERATED_BODY()

	FConfigFileSettings() = default;

	FConfigFileSettings( const FString& InFileName, const bool bInEnabled = true )
	: FileName( InFileName )
	, bEnabled( bInEnabled ) {}

	UPROPERTY( EditAnywhere )
	FString FileName;

	UPROPERTY( EditAnywhere )
	FString RelativePath;

	UPROPERTY( EditAnywhere )
	bool bEnabled = false;

	UPROPERTY( EditAnywhere )
	EGlobalSettingSyncerScope SettingsScope = EGlobalSettingSyncerScope::PerEngineVersion;

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
class GLOBALSETTINGSYNCER_API UGlobalSettingSyncerConfig : public UObject
{
	GENERATED_BODY()

public:
	UGlobalSettingSyncerConfig();

	void DiscoverAndAddConfigFiles();
	bool SaveSettingsToGlobal();
	bool LoadSettingsFromGlobal();

	static FString GetGlobalSettingSyncerDirectory();

	static FString GetScopedSettingsDirectory( EGlobalSettingSyncerScope Scope );

	static UGlobalSettingSyncerConfig* Get();

	bool           SavePluginSettings() const;
	bool           LoadPluginSettings();
	static FString GetPluginSettingsFilePath();

	void Initialize() { EnableAutoSync(); }

	void Shutdown() { DisableAutoSync(); }

	FDateTime GetLastSyncTime() const { return LastSyncTime; }

	void OnSettingsChanged();

	static TArray< FString > DiscoverAllConfigFiles();

	static FString FindConfigFilePath( const FString& FileName );

	static FString GetRelativePathForFile( const FString& AbsolutePath );

	UPROPERTY( EditAnywhere )
	FConfigFileSettingsStruct ConfigFileSettingsStruct;

private:
	static bool SaveConfigFile( const FConfigFileSettings& Filter, const FString& TargetDirectory );
	static bool LoadConfigFile( const FConfigFileSettings& Filter, const FString& SourceDirectory );

	static bool SaveConfigFiles( const FString& TargetDirectory );
	static bool LoadConfigFiles( const FString& SourceDirectory );

	static bool CopyIniFile( const FString& SourcePath, const FString& DestPath );

	static bool EnsureDirectoryExists( const FString& DirectoryPath );

	void InitializeDefaultFileFilters();

	TArray< FString > GetConfigFilesToSync() const;

	void EnableAutoSync();

	void DisableAutoSync();

	void OnDirectoryChanged( const TArray< struct FFileChangeData >& FileChanges );

	static UGlobalSettingSyncerConfig* Instance;

	FDateTime                 LastSyncTime;
	FTimerHandle              AutoSyncTimerHandle;
	TArray< FDelegateHandle > DirectoryWatcherHandles;
	TSet< FString >           PendingChangedFiles;
	FTimerHandle              ProcessChangesTimerHandle;
};