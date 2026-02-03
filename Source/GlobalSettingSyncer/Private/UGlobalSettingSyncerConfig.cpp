#include "UGlobalSettingSyncerConfig.h"

#include "DirectoryWatcherModule.h"
#include "FGlobalSettingSyncer.h"
#include "IDirectoryWatcher.h"
#include "JsonObjectConverter.h"
#include "Macros.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

UGlobalSettingSyncerConfig::UGlobalSettingSyncerConfig()
: LastSyncTime( FDateTime::MinValue() )
{
	TRACE_CPU_SCOPE;

	InitializeDefaultFileFilters();
}

void UGlobalSettingSyncerConfig::DiscoverAndAddConfigFiles()
{
	TRACE_CPU_SCOPE;

	TArray< FString > AllConfigFiles = DiscoverAllConfigFiles();

	TSet< FString > ExistingFileNames;
	for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
	{
		ExistingFileNames.Add( Filter.FileName );
	}

	int32 FilesAdded = 0;
	for( const FString& FileName: AllConfigFiles )
	{
		if( !ExistingFileNames.Contains( FileName ) )
		{
			FConfigFileSettings NewFilter( FileName, true );
			ConfigFileSettingsStruct.Settings.Add( NewFilter );
			FilesAdded++;
		}
	}

	if( FilesAdded > 0 )
	{
		SavePluginSettings();

		FNotificationInfo Info( FText::FromString( FString::Printf( TEXT( "Added %d new config files to sync list" ), FilesAdded ) ) );
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification( Info );
	}
	else
	{
		FNotificationInfo Info( FText::FromString( TEXT( "No new config files found to add" ) ) );
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification( Info );
	}
}

bool UGlobalSettingSyncerConfig::SaveSettingsToGlobal()
{
	TRACE_CPU_SCOPE;

	bool bSuccess = false;

	TRACE_CPU_SCOPE_STR( "FileLoop" );
	for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
	{
		if( !Filter.bEnabled )
			continue;

		const FString TargetDirectory = GetScopedSettingsDirectory( Filter.SettingsScope );

		if( !EnsureDirectoryExists( TargetDirectory ) )
			continue;

		if( SaveConfigFile( Filter, TargetDirectory ) )
			bSuccess = true;
	}

	if( bSuccess )
	{
		LastSyncTime = FDateTime::Now();

		FNotificationInfo Info( FText::FromString( TEXT( "Settings saved to global location!" ) ) );
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification( Info );
	}
	else
	{
		FNotificationInfo Info( FText::FromString( TEXT( "Failed to save settings to global location!" ) ) );
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification( Info );
	}

	return bSuccess;
}

bool UGlobalSettingSyncerConfig::LoadSettingsFromGlobal()
{
	TRACE_CPU_SCOPE;

	bool bSuccess = false;

	TRACE_CPU_SCOPE_STR( "FileLoop" );
	for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
	{
		if( !Filter.bEnabled )
			continue;

		const FString SourceDirectory = GetScopedSettingsDirectory( Filter.SettingsScope );

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if( !PlatformFile.DirectoryExists( *SourceDirectory ) )
			continue;

		if( LoadConfigFile( Filter, SourceDirectory ) )
			bSuccess = true;
	}

	if( bSuccess )
	{
		GConfig->Flush( false );
		LastSyncTime = FDateTime::Now();

		FNotificationInfo Info( FText::FromString( TEXT( "Settings loaded from global location! Please restart the editor for all changes to take effect." ) ) );
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification( Info );
	}
	else
	{
		FNotificationInfo Info( FText::FromString( TEXT( "Failed to load settings from global location!" ) ) );
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification( Info );
	}

	return bSuccess;
}

FString UGlobalSettingSyncerConfig::GetGlobalSettingSyncerDirectory()
{
	TRACE_CPU_SCOPE;

	static FString BaseDir = FPlatformProcess::UserSettingsDir();
	return FPaths::Combine( BaseDir, "UnrealEngine", "GlobalSettingSyncer" );
}

FString UGlobalSettingSyncerConfig::GetScopedSettingsDirectory( const EGlobalSettingSyncerScope Scope )
{
	TRACE_CPU_SCOPE;

	FString BaseDir = GetGlobalSettingSyncerDirectory();

	switch( Scope )
	{
		case EGlobalSettingSyncerScope::Global:
			return FPaths::Combine( BaseDir, "Global" );
		case EGlobalSettingSyncerScope::PerEngineVersion:
		{
			static FString EngineVersion = FString::Printf( TEXT( "%d.%d" ), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION );
			return FPaths::Combine( BaseDir, "PerEngineVersion", EngineVersion );
		}
		case EGlobalSettingSyncerScope::PerProject:
			return FPaths::Combine( BaseDir, "PerProject", FApp::GetProjectName() );
		default:
			return BaseDir;
	}
}

UGlobalSettingSyncerConfig* UGlobalSettingSyncerConfig::Get()
{
	TRACE_CPU_SCOPE;

	if( !Instance )
	{
		Instance = GetMutableDefault< UGlobalSettingSyncerConfig >();
		Instance->AddToRoot();
		Instance->LoadPluginSettings();
	}

	return Instance;
}

bool UGlobalSettingSyncerConfig::SavePluginSettings() const
{
	TRACE_CPU_SCOPE;

	const FString SettingsFilePath = GetPluginSettingsFilePath();
	const FString SettingsDir      = FPaths::GetPath( SettingsFilePath );
	if( !EnsureDirectoryExists( SettingsDir ) )
	{
		UE_LOG( GlobalSettingSyncer, Error, TEXT( "Failed to create settings directory: %s" ), *SettingsDir );
		return false;
	}

	FString OutputString;
	if( !FJsonObjectConverter::UStructToJsonObjectString( ConfigFileSettingsStruct, OutputString, 0, 0, 0, nullptr, true ) )
	{
		UE_LOG( GlobalSettingSyncer, Error, TEXT( "Failed to convert settings JSON: %s" ), *SettingsFilePath );
		return false;
	}

	if( !FFileHelper::SaveStringToFile( OutputString, *SettingsFilePath ) )
	{
		UE_LOG( GlobalSettingSyncer, Error, TEXT( "Failed to write settings file: %s" ), *SettingsFilePath );
		return false;
	}

	UE_LOG( GlobalSettingSyncer, Log, TEXT( "Plugin settings saved to: %s" ), *SettingsFilePath );
	return true;
}

bool UGlobalSettingSyncerConfig::LoadPluginSettings()
{
	TRACE_CPU_SCOPE;

	const FString SettingsFilePath = GetPluginSettingsFilePath();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if( !PlatformFile.FileExists( *SettingsFilePath ) )
	{
		UE_LOG( GlobalSettingSyncer, Warning, TEXT( "Settings file not found: %s" ), *SettingsFilePath );
		return false;
	}

	FString JsonString;
	if( !FFileHelper::LoadFileToString( JsonString, *SettingsFilePath ) )
	{
		UE_LOG( GlobalSettingSyncer, Error, TEXT( "Failed to read settings file: %s" ), *SettingsFilePath );
		return false;
	}

	if( !FJsonObjectConverter::JsonObjectStringToUStruct( JsonString, &ConfigFileSettingsStruct ) )
	{
		UE_LOG( GlobalSettingSyncer, Error, TEXT( "Failed to convert settings JSON: %s" ), *SettingsFilePath );
		return false;
	}

	UE_LOG( GlobalSettingSyncer, Log, TEXT( "Plugin settings loaded from: %s" ), *SettingsFilePath );
	return true;
}

FString UGlobalSettingSyncerConfig::GetPluginSettingsFilePath()
{
	TRACE_CPU_SCOPE;

	return FPaths::Combine( GetScopedSettingsDirectory( EGlobalSettingSyncerScope::PerProject ), "GlobalSettingSyncerSettings.json" );
}

void UGlobalSettingSyncerConfig::OnSettingsChanged()
{
	TRACE_CPU_SCOPE;

	bool bAnyAutoSyncEnabled = false;
	for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
	{
		if( Filter.bEnabled && Filter.bAutoSyncEnabled )
		{
			bAnyAutoSyncEnabled = true;
			break;
		}
	}

	SavePluginSettings();

	if( bAnyAutoSyncEnabled )
	{
		DisableAutoSync();
		EnableAutoSync();

		SaveSettingsToGlobal();
	}
	else
	{
		DisableAutoSync();
	}
}

TArray< FString > UGlobalSettingSyncerConfig::DiscoverAllConfigFiles()
{
	TRACE_CPU_SCOPE;

	TArray< FString > AllConfigFiles;
	IPlatformFile&    PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TRACE_CPU_SCOPE_STR( "ProjectConfigDiscovery" );
	const FString     ProjectConfigDir = FPaths::ProjectConfigDir();
	TArray< FString > ConfigDirFiles;
	PlatformFile.FindFiles( ConfigDirFiles, *ProjectConfigDir, TEXT( ".ini" ) );

	TRACE_CPU_SCOPE_STR( "ProjectConfigLoop" );
	for( const FString& FilePath: ConfigDirFiles )
	{
		FString FileName = FPaths::GetCleanFilename( FilePath );
		AllConfigFiles.AddUnique( FileName );
	}

	TRACE_CPU_SCOPE_STR( "SavedConfigDiscovery" );
	const FString SavedConfigDir = FPaths::ProjectSavedDir() / TEXT( "Config" );
	if( PlatformFile.DirectoryExists( *SavedConfigDir ) )
	{
		TArray< FString > SavedConfigFiles;
		PlatformFile.FindFilesRecursively( SavedConfigFiles, *SavedConfigDir, TEXT( ".ini" ) );

		TRACE_CPU_SCOPE_STR( "SavedConfigLoop" );
		for( const FString& FilePath: SavedConfigFiles )
		{
			FString FileName = FPaths::GetCleanFilename( FilePath );

			if( !FileName.Contains( TEXT( ".bak" ) ) &&
			    !FileName.Contains( TEXT( ".tmp" ) ) &&
			    !FileName.Contains( TEXT( "~" ) ) )
			{
				AllConfigFiles.AddUnique( FileName );
			}
		}
	}

	TRACE_CPU_SCOPE_STR( "PluginConfigDiscovery" );
	const FString PluginsDir = FPaths::ProjectPluginsDir();
	if( PlatformFile.DirectoryExists( *PluginsDir ) )
	{
		TArray< FString > PluginConfigFiles;
		PlatformFile.FindFilesRecursively( PluginConfigFiles, *PluginsDir, TEXT( ".ini" ) );

		TRACE_CPU_SCOPE_STR( "PluginConfigLoop" );
		for( const FString& FilePath: PluginConfigFiles )
		{
			if( FilePath.Contains( TEXT( "/Config/" ) ) || FilePath.Contains( TEXT( "\\Config\\" ) ) )
			{
				FString FileName = FPaths::GetCleanFilename( FilePath );
				if( !FileName.Contains( TEXT( ".bak" ) ) &&
				    !FileName.Contains( TEXT( ".tmp" ) ) )
				{
					AllConfigFiles.AddUnique( FileName );
				}
			}
		}
	}

	return AllConfigFiles;
}

FString UGlobalSettingSyncerConfig::FindConfigFilePath( const FString& FileName )
{
	TRACE_CPU_SCOPE;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const FString ProjectConfigDir = FPaths::ProjectConfigDir();
	FString       FilePath         = FPaths::Combine( ProjectConfigDir, FileName );
	if( PlatformFile.FileExists( *FilePath ) )
		return FilePath;

	const FString SavedConfigDir = FPaths::ProjectSavedDir() / TEXT( "Config" );
	if( PlatformFile.DirectoryExists( *SavedConfigDir ) )
	{
		TArray< FString > SavedConfigFiles;
		PlatformFile.FindFilesRecursively( SavedConfigFiles, *SavedConfigDir, TEXT( ".ini" ) );

		for( const FString& SavedFile: SavedConfigFiles )
		{
			if( FPaths::GetCleanFilename( SavedFile ) == FileName )
				return SavedFile;
		}
	}

	const FString PluginsDir = FPaths::ProjectPluginsDir();
	if( PlatformFile.DirectoryExists( *PluginsDir ) )
	{
		TArray< FString > PluginConfigFiles;
		PlatformFile.FindFilesRecursively( PluginConfigFiles, *PluginsDir, TEXT( ".ini" ) );

		for( const FString& PluginFile: PluginConfigFiles )
		{
			if( FPaths::GetCleanFilename( PluginFile ) == FileName &&
			    ( PluginFile.Contains( TEXT( "/Config/" ) ) || PluginFile.Contains( TEXT( "\\Config\\" ) ) ) )
			{
				return PluginFile;
			}
		}
	}

	return FString();
}

FString UGlobalSettingSyncerConfig::GetRelativePathForFile( const FString& AbsolutePath )
{
	TRACE_CPU_SCOPE;

	const FString ProjectConfigDir = FPaths::ProjectConfigDir();
	const FString SavedConfigDir   = FPaths::ProjectSavedDir() / TEXT( "Config" );
	const FString PluginsDir       = FPaths::ProjectPluginsDir();
	const FString FileName         = FPaths::GetCleanFilename( AbsolutePath );

	if( AbsolutePath.StartsWith( ProjectConfigDir ) )
	{
		return TEXT( "Config/" ) + FileName;
	}

	if( AbsolutePath.StartsWith( SavedConfigDir ) )
	{
		FString RelPath = AbsolutePath;
		FPaths::MakePathRelativeTo( RelPath, *( SavedConfigDir + TEXT( "/" ) ) );
		return TEXT( "Config/SavedConfig/" ) + RelPath;
	}

	if( AbsolutePath.StartsWith( PluginsDir ) )
	{
		FString RelPath = AbsolutePath;
		FPaths::MakePathRelativeTo( RelPath, *( PluginsDir + TEXT( "/" ) ) );
		return TEXT( "Config/Plugins/" ) + RelPath;
	}

	return TEXT( "Config/" ) + FileName;
}

bool UGlobalSettingSyncerConfig::SaveConfigFile( const FConfigFileSettings& Filter, const FString& TargetDirectory )
{
	TRACE_CPU_SCOPE;

	FString SourcePath = FindConfigFilePath( Filter.FileName );

	if( SourcePath.IsEmpty() )
	{
		UE_LOG( GlobalSettingSyncer, Warning, TEXT( "Could not find config file: %s" ), *Filter.FileName );
		return false;
	}

	FString DestinationPath;
	if( !Filter.RelativePath.IsEmpty() )
	{
		DestinationPath = FPaths::Combine( TargetDirectory, Filter.RelativePath );
	}
	else
	{
		DestinationPath = FPaths::Combine( TargetDirectory, "Config", Filter.FileName );
	}

	return CopyIniFile( SourcePath, DestinationPath );
}

bool UGlobalSettingSyncerConfig::LoadConfigFile( const FConfigFileSettings& Filter, const FString& SourceDirectory )
{
	TRACE_CPU_SCOPE;

	const FString ProjectConfigDir = FPaths::ProjectConfigDir();
	const FString SavedConfigDir   = FPaths::ProjectSavedDir() / TEXT( "Config" );

	FString SourcePath;
	if( !Filter.RelativePath.IsEmpty() )
	{
		SourcePath = FPaths::Combine( SourceDirectory, Filter.RelativePath );
	}
	else
	{
		SourcePath = FPaths::Combine( SourceDirectory, "Config", Filter.FileName );
	}

	FString OriginalPath = FindConfigFilePath( Filter.FileName );
	FString DestinationPath;

	if( !OriginalPath.IsEmpty() )
	{
		DestinationPath = OriginalPath;
	}
	else if( Filter.FileName.Contains( TEXT( "Layout" ) ) ||
	         Filter.FileName.Contains( TEXT( "UserSettings" ) ) ||
	         Filter.FileName.Contains( TEXT( "AssetEditor" ) ) )
	{
		DestinationPath = FPaths::Combine( SavedConfigDir, Filter.FileName );
	}
	else
	{
		DestinationPath = FPaths::Combine( ProjectConfigDir, Filter.FileName );
	}

	return CopyIniFile( SourcePath, DestinationPath );
}

bool UGlobalSettingSyncerConfig::SaveConfigFiles( const FString& TargetDirectory )
{
	TRACE_CPU_SCOPE;

	UGlobalSettingSyncerConfig*          Config      = Get();
	const TArray< FConfigFileSettings >& FileFilters = Config->ConfigFileSettingsStruct.Settings;

	bool          bSuccess         = false;
	const FString ProjectConfigDir = FPaths::ProjectConfigDir();
	const FString SavedConfigDir   = FPaths::ProjectSavedDir() / TEXT( "Config" );

	TRACE_CPU_SCOPE_STR( "FileLoop" );
	for( const FConfigFileSettings& Filter: FileFilters )
	{
		if( !Filter.bEnabled )
			continue;

		FString SourcePath = FindConfigFilePath( Filter.FileName );

		if( SourcePath.IsEmpty() )
		{
			UE_LOG( GlobalSettingSyncer, Warning, TEXT( "Could not find config file: %s" ), *Filter.FileName );
			continue;
		}

		const FString DestinationPath = FPaths::Combine( TargetDirectory, "Config", Filter.FileName );

		if( CopyIniFile( SourcePath, DestinationPath ) )
			bSuccess = true;
	}

	return bSuccess;
}

bool UGlobalSettingSyncerConfig::LoadConfigFiles( const FString& SourceDirectory )
{
	TRACE_CPU_SCOPE;

	UGlobalSettingSyncerConfig*          Config      = Get();
	const TArray< FConfigFileSettings >& FileFilters = Config->ConfigFileSettingsStruct.Settings;

	bool    bSuccess         = false;
	FString ProjectConfigDir = FPaths::ProjectConfigDir();
	FString SavedConfigDir   = FPaths::ProjectSavedDir() / TEXT( "Config" );

	TRACE_CPU_SCOPE_STR( "FileLoop" );
	for( const FConfigFileSettings& Filter: FileFilters )
	{
		if( !Filter.bEnabled )
			continue;

		const FString SourcePath = FPaths::Combine( SourceDirectory, "Config", Filter.FileName );

		FString DestinationPath;
		if( Filter.FileName.Contains( TEXT( "Layout" ) ) ||
		    Filter.FileName.Contains( TEXT( "UserSettings" ) ) ||
		    Filter.FileName.Contains( TEXT( "AssetEditor" ) ) )
		{
			DestinationPath = FPaths::Combine( SavedConfigDir, Filter.FileName );
		}
		else
		{
			DestinationPath = FPaths::Combine( ProjectConfigDir, Filter.FileName );
		}

		if( CopyIniFile( SourcePath, DestinationPath ) )
			bSuccess = true;
	}

	return bSuccess;
}

bool UGlobalSettingSyncerConfig::CopyIniFile( const FString& SourcePath, const FString& DestPath )
{
	TRACE_CPU_SCOPE;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if( !PlatformFile.FileExists( *SourcePath ) )
		return false;

	const FString DestDir = FPaths::GetPath( DestPath );
	if( !EnsureDirectoryExists( DestDir ) )
		return false;

	return PlatformFile.CopyFile( *DestPath, *SourcePath );
}

bool UGlobalSettingSyncerConfig::EnsureDirectoryExists( const FString& DirectoryPath )
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if( PlatformFile.DirectoryExists( *DirectoryPath ) )
		return true;

	return PlatformFile.CreateDirectoryTree( *DirectoryPath );
}

void UGlobalSettingSyncerConfig::InitializeDefaultFileFilters()
{
	TRACE_CPU_SCOPE;

	if( ConfigFileSettingsStruct.Settings.Num() == 0 )
	{
		TRACE_CPU_SCOPE_STR( "FileDiscovery" );
		TArray< FString > DiscoveredFiles = DiscoverAllConfigFiles();

		TRACE_CPU_SCOPE_STR( "FileLoop" );
		for( const FString& FileName: DiscoveredFiles )
		{
			FConfigFileSettings NewFilter( FileName, false );

			FString AbsPath = FindConfigFilePath( FileName );
			if( !AbsPath.IsEmpty() )
			{
				NewFilter.RelativePath = GetRelativePathForFile( AbsPath );
			}

			ConfigFileSettingsStruct.Settings.Add( NewFilter );
		}
	}
	else
	{
		for( FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
		{
			if( Filter.RelativePath.IsEmpty() )
			{
				FString AbsPath = FindConfigFilePath( Filter.FileName );
				if( !AbsPath.IsEmpty() )
				{
					Filter.RelativePath = GetRelativePathForFile( AbsPath );
				}
			}
		}
	}
}

TArray< FString > UGlobalSettingSyncerConfig::GetConfigFilesToSync() const
{
	TRACE_CPU_SCOPE;

	TArray< FString > ConfigFiles;

	for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
	{
		if( Filter.bEnabled )
			ConfigFiles.Add( Filter.FileName );
	}

	return ConfigFiles;
}

void UGlobalSettingSyncerConfig::EnableAutoSync()
{
	TRACE_CPU_SCOPE;

	bool bAnyAutoSyncEnabled = false;
	for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
	{
		if( Filter.bEnabled && Filter.bAutoSyncEnabled )
		{
			bAnyAutoSyncEnabled = true;
			break;
		}
	}

	if( !bAnyAutoSyncEnabled )
		return;

	if( DirectoryWatcherHandles.Num() > 0 )
		return;

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked< FDirectoryWatcherModule >( "DirectoryWatcher" );
	IDirectoryWatcher*       DirectoryWatcher       = DirectoryWatcherModule.Get();

	if( DirectoryWatcher )
	{
		TRACE_CPU_SCOPE_STR( "DirectoryWatchSetup" );

		FString ProjectConfigDir = FPaths::ProjectConfigDir();
		if( FPaths::DirectoryExists( ProjectConfigDir ) )
		{
			FDelegateHandle Handle;
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				ProjectConfigDir,
				IDirectoryWatcher::FDirectoryChanged::CreateUObject( this, &UGlobalSettingSyncerConfig::OnDirectoryChanged ),
				Handle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges );
			DirectoryWatcherHandles.Add( Handle );
		}

		FString SavedConfigDir = FPaths::ProjectSavedDir() / TEXT( "Config" );
		if( FPaths::DirectoryExists( SavedConfigDir ) )
		{
			FDelegateHandle Handle;
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				SavedConfigDir,
				IDirectoryWatcher::FDirectoryChanged::CreateUObject( this, &UGlobalSettingSyncerConfig::OnDirectoryChanged ),
				Handle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges );
			DirectoryWatcherHandles.Add( Handle );
		}

		FString PluginsDir = FPaths::ProjectPluginsDir();
		if( FPaths::DirectoryExists( PluginsDir ) )
		{
			FDelegateHandle Handle;
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				PluginsDir,
				IDirectoryWatcher::FDirectoryChanged::CreateUObject( this, &UGlobalSettingSyncerConfig::OnDirectoryChanged ),
				Handle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges );
			DirectoryWatcherHandles.Add( Handle );
		}
	}
}

void UGlobalSettingSyncerConfig::DisableAutoSync()
{
	TRACE_CPU_SCOPE;

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked< FDirectoryWatcherModule >( "DirectoryWatcher" );
	IDirectoryWatcher*       DirectoryWatcher       = DirectoryWatcherModule.Get();

	if( DirectoryWatcher )
	{
		for( const FDelegateHandle& Handle: DirectoryWatcherHandles )
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle( FPaths::ProjectConfigDir(), Handle );
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle( FPaths::ProjectSavedDir() / TEXT( "Config" ), Handle );
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle( FPaths::ProjectPluginsDir(), Handle );
		}
	}

	DirectoryWatcherHandles.Empty();
	PendingChangedFiles.Empty();

	if( ProcessChangesTimerHandle.IsValid() )
	{
		GEditor->GetTimerManager()->ClearTimer( ProcessChangesTimerHandle );
	}
}

void UGlobalSettingSyncerConfig::OnDirectoryChanged( const TArray< FFileChangeData >& FileChanges )
{
	TRACE_CPU_SCOPE;

	TRACE_CPU_SCOPE_STR( "FileChangeLoop" );
	for( const FFileChangeData& Change: FileChanges )
	{
		if( Change.Action != FFileChangeData::FCA_Modified || !Change.Filename.EndsWith( TEXT( ".ini" ) ) )
			continue;

		FString FileName = FPaths::GetCleanFilename( Change.Filename );

		const FConfigFileSettings* MatchedFilter = nullptr;
		for( const FConfigFileSettings& Filter: ConfigFileSettingsStruct.Settings )
		{
			if( Filter.bEnabled && FileName == Filter.FileName )
			{
				MatchedFilter = &Filter;
				break;
			}
		}

		if( !MatchedFilter )
			continue;

		if( !MatchedFilter->bAutoSyncEnabled )
			continue;

		PendingChangedFiles.Add( Change.Filename );
	}

	if( PendingChangedFiles.Num() > 0 && !ProcessChangesTimerHandle.IsValid() )
	{
		GEditor->GetTimerManager()->SetTimer(
			ProcessChangesTimerHandle,
			[ this ]()
			{
				if( PendingChangedFiles.Num() > 0 )
				{
					SaveSettingsToGlobal();
					PendingChangedFiles.Empty();
				}
				ProcessChangesTimerHandle.Invalidate();
			},
			0.5f,
			false );
	}
}

UGlobalSettingSyncerConfig* UGlobalSettingSyncerConfig::Instance = nullptr;