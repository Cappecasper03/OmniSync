# OmniSync Plugin

Unreal Engine editor plugin that synchronizes .ini configuration files across multiple projects and engine versions using centralized storage.

## Code Style

- **Naming**: Unreal Engine conventions - prefix classes with `F` (plain), `U` (UObject), `I` (interface), `E` (enum)
- **Reflection**: Use `UPROPERTY()`, `UFUNCTION()`, `USTRUCT()`, `UCLASS()` macros for UObject system integration
- **Headers**: Public headers contain interfaces/declarations; Private folder has implementations
- **Property Access**: Use `GET_MEMBER_NAME_CHECKED(ClassName, PropertyName)` for compile-time validated property references
- **Profiling**: Wrap all functions with `TRACE_CPU_SCOPE(FunctionName)` macro using `TRACE_CPUPROFILER_EVENT_SCOPE_STR` (see [Macros.h](../Source/OmniSync/Private/Macros.h))
- **Logging**: Define `DECLARE_LOG_CATEGORY_EXTERN(OmniSync, Log, All)` in header, `DEFINE_LOG_CATEGORY(OmniSync)` in .cpp
- **Localization**: Wrap UI strings with `LOCTEXT(Key, Text)` - define `LOCTEXT_NAMESPACE` at file start, undefine at end
- **Static Path Caching**: Cache frequently used paths at initialization time for performance (ProjectConfigDir, SavedConfigDir, PluginsDir)

Reference: [FOmniSync.h](../Source/OmniSync/Public/FOmniSync.h), [UOmniSyncConfig.h](../Source/OmniSync/Public/UOmniSyncConfig.h)

## Architecture

**Singleton Pattern**: `UOmniSyncConfig::Get()` creates instance via `GetMutableDefault`, manually adds to root to prevent garbage collection

**Component Structure**:
- **FOmniSyncModule**: Module lifecycle (startup/shutdown), settings registration with editor
- **UOmniSyncConfig**: Core logic - file discovery, sync operations, auto-sync ticker, JSON persistence
- **FOmniSyncCustomization**: Custom Slate UI - tree view for hierarchical config file selection

**Key Data Structures**:
- **FConfigTreeItem**: Tree node with `FString Name`, `FString FullPath`, `bool bIsFolder`, property handles (`EnabledHandle`, `ScopeHandle`, `AutoSyncHandle`), `TArray<TSharedPtr<FConfigTreeItem>> Children`, `bool bIsExpanded`
- **FConfigFileSettings**: Struct containing `FileName`, `RelativePath`, `bEnabled`, `SettingsScope`, `bAutoSyncEnabled`
- **EOmniSyncScope**: Enum for sync scope - `Global`, `PerEngineVersion`, `PerProject`

**Data Flow**:
1. Discover .ini files in ProjectConfigDir(), ProjectSavedDir()/Config, ProjectPluginsDir()
2. User enables files and sets scope (Global/PerEngineVersion/PerProject)
3. Auto-sync ticker (10s interval) compares file sizes, copies changed files
4. Manual sync: Save pushes to centralized storage, Load pulls from it

**Storage Locations**: `%UserSettingsDir%/UnrealEngine/OmniSync/{Scope}/{RelativePath}`

See: [UOmniSyncConfig.cpp](../Source/OmniSync/Private/UOmniSyncConfig.cpp) for sync logic, [FOmniSyncCustomization.cpp](../Source/OmniSync/Private/FOmniSyncCustomization.cpp) for UI

## Build and Test

**Build**: This plugin builds with the host Unreal Engine project. Editor-only module specified in [OmniSync.uplugin](../OmniSync.uplugin). Ensure dependencies in [OmniSync.Build.cs](../Source/OmniSync/OmniSync.Build.cs) match your UE version.

**Required Modules**: CoreUObject, Engine, Slate, SlateCore, InputCore, UnrealEd, ToolMenus, EditorStyle, EditorFramework, Projects, Json, JsonUtilities, DeveloperSettings, DirectoryWatcher

**Test Plugin**:
1. Open host project in Unreal Editor
2. Navigate to Editor → Project Settings → Plugins → OmniSync
3. Click "Discover All Config Files"
4. Enable config files and set their scope
5. Click "Save to Global"
6. Verify files copied to `%USERPROFILE%/AppData/Local/UnrealEngine/OmniSync/`

## Project Conventions

**No File Watchers**: Uses ticker-based polling (10s) instead of DirectoryWatcher despite dependency - keeps implementation simple

**Size-Based Sync**: Auto-sync compares file sizes only, not timestamps or hashes - assumes size change indicates modification

**Manual Root Management**: Settings object added to root explicitly in `Get()` to prevent garbage collection - lives for editor lifetime (never removed)

**Property Handles Over Delegates**: UI uses `IPropertyHandle` for live updates without recreating widgets - see `OnGenerateRow()` in [FOmniSyncCustomization.cpp](../Source/OmniSync/Private/FOmniSyncCustomization.cpp)

**Property Handle Refresh**: Call `StructHandle->NotifyFinishedChangingProperties()` to trigger complete panel rebuild after file discovery - see [FOmniSyncCustomization.cpp](../Source/OmniSync/Private/FOmniSyncCustomization.cpp)

**JSON Persistence**: Settings saved using `FJsonObjectConverter::UStructToJsonObjectString()` to per-project scope, not .ini files - see `SavePluginSettings()` in [UOmniSyncConfig.cpp](../Source/OmniSync/Private/UOmniSyncConfig.cpp)

**Tree Item Pattern**: Custom `FConfigTreeItem` struct builds hierarchical UI from flat config list - folder nodes generated from file paths, not filesystem

**Mixed File APIs**: Uses `IFileManager::Get()` for size checks, `IPlatformFile` for copy operations - both work cross-platform

**UI Color Coding**: Scope indicators blue (`0.7, 0.7, 1.0`), auto-sync green (`0.3, 1.0, 0.3`), disabled gray (`0.5, 0.5, 0.5`)

## Integration Points

**Settings Module**: Registers with `ISettingsModule` in [FOmniSync.cpp](../Source/OmniSync/Private/FOmniSync.cpp) startup - appears under Editor → Plugins

**Detail Customization**: Overrides default property panel using `FPropertyEditorModule::RegisterCustomClassLayout()` - provides custom tree UI instead of property grid

**Platform File API**: Uses `IPlatformFile::CopyFile()` for sync operations - abstract layer works across Windows/Mac/Linux

**Ticker System**: `FTSTicker::GetCoreTicker().AddTicker()` for auto-sync polling - ticker handle stored and removed in module `Shutdown()`

**Module Lifecycle**: Check `FModuleManager::Get().IsModuleLoaded("PropertyEditor")` before unregistering customization in shutdown

## Security

**User Settings Directory**: All centralized files stored in user-writable locations (`FPlatformProcess::UserSettingsDir()`) - no admin privileges required

**File Overwrite**: Both manual and auto-sync overwrite files without confirmation - critical configs should be backed up before enabling sync

**Engine Version Detection**: Uses `ENGINE_MAJOR_VERSION`/`ENGINE_MINOR_VERSION` macros for scoping - only detects major.minor (e.g., 5.3), not patch versions

## Common Pitfalls

**Ignored Copy Errors**: `CopyIniFile()` returns bool but `SaveSettingsToGlobal()` and `AutoSyncTick()` ignore failures - wrap calls with error logging

**Size-Only Comparison**: Auto-sync can miss content changes if file size unchanged - limitation of current implementation

**Unused DeltaTime**: `AutoSyncTick(float DeltaTime)` parameter unused - ticker interval hardcoded to 10s

**Property Handle Invalidation**: Tree refresh requires full panel rebuild via `NotifyFinishedChangingProperties()` - incremental updates not supported

**DirectoryWatcher Dependency**: Module listed in [OmniSync.Build.cs](../Source/OmniSync/OmniSync.Build.cs) but never used - originally intended for file watching, replaced by ticker polling
