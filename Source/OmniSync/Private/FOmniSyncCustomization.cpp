#include "FOmniSyncCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Macros.h"
#include "UOmniSyncConfig.h"

#define LOCTEXT_NAMESPACE "OmniSyncCustomization"

void FOmniSyncCustomization::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TRACE_CPU_SCOPE;

	TArray< TWeakObjectPtr< > > ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized( ObjectsBeingCustomized );

	if( !ObjectsBeingCustomized.IsEmpty() )
		ConfigObject = Cast< UOmniSyncConfig >( ObjectsBeingCustomized[ 0 ].Get() );

	TSharedRef< IPropertyHandle > StructHandle    = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UOmniSyncConfig, ConfigFileSettingsStruct ) );
	IDetailCategoryBuilder&       ActionsCategory = DetailBuilder.EditCategory( "Actions", FText::FromString( "Actions" ), ECategoryPriority::Important );

	ActionsCategory.AddCustomRow( LOCTEXT( "SyncActionsRow", "Sync Actions" ) ).WholeRowContent()
	[
		SNew( SHorizontalBox )

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 2 )
		[
			SNew( SButton )
			.Text( LOCTEXT( "DiscoverFiles", "Discover All Config Files" ) )
			.ToolTipText( LOCTEXT( "DiscoverFilesTooltip", "Scan the project and add all .ini files to the sync list" ) )
			.OnClicked_Lambda( [this, StructHandle]
			{
				if( UOmniSyncConfig* Config = ConfigObject.Get() )
				{
					Config->DiscoverAndAddConfigFiles();
					StructHandle->NotifyFinishedChangingProperties();
				}
				return FReply::Handled();
			} )
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 2 )
		[
			SNew( SButton )
			.Text( LOCTEXT( "SaveToGlobal", "Save to Global" ) )
			.ToolTipText( LOCTEXT( "SaveToGlobalTooltip", "Save enabled config files to their global sync locations" ) )
			.OnClicked_Lambda( [this]
			{
				if( UOmniSyncConfig* Config = ConfigObject.Get() )
					Config->SaveSettingsToGlobal();
				return FReply::Handled();
			} )
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 2 )
		[
			SNew( SButton )
			.Text( LOCTEXT( "LoadFromGlobal", "Load from Global" ) )
			.ToolTipText( LOCTEXT( "LoadFromGlobalTooltip", "Load config files from their global sync locations. Restart may be required." ) )
			.OnClicked_Lambda( [this]
			{
				if( UOmniSyncConfig* Config = ConfigObject.Get() )
					Config->LoadSettingsFromGlobal();
				return FReply::Handled();
			} )
		]
	];

	DetailBuilder.HideProperty( StructHandle );

	RefreshTreeData( DetailBuilder );

	IDetailCategoryBuilder& FilesCategory = DetailBuilder.EditCategory( "Configs", LOCTEXT( "Configs", "Configs" ), ECategoryPriority::Default );
	FilesCategory.AddCustomRow( LOCTEXT( "FilesTreeRow", "Files Tree" ) ).WholeRowContent()
	[
		SAssignNew( TreeView, STreeView< TSharedRef< FConfigTreeItem > > )
		.TreeItemsSource( &RootItems )
		.OnGenerateRow( this, &FOmniSyncCustomization::OnGenerateRow )
		.OnGetChildren( this, &FOmniSyncCustomization::OnGetChildren )
		.SelectionMode( ESelectionMode::None )
	];
}

void FOmniSyncCustomization::RefreshTreeData( const IDetailLayoutBuilder& DetailBuilder )
{
	TRACE_CPU_SCOPE;
	RootItems.Empty();

	if( !ConfigObject.IsValid() )
		return;

	const TSharedRef< IPropertyHandle > StructHandle   = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UOmniSyncConfig, ConfigFileSettingsStruct ) );
	const TSharedPtr< IPropertyHandle > SettingsHandle = StructHandle->GetChildHandle( GET_MEMBER_NAME_CHECKED( FConfigFileSettingsStruct, Settings ) );

	if( !SettingsHandle.IsValid() )
		return;

	uint32 NumElements = 0;
	SettingsHandle->GetNumChildren( NumElements );

	TMap< FString, TSharedRef< FConfigTreeItem > > FolderCache;

	for( uint32 i = 0; i < NumElements; ++i )
	{
		TSharedPtr< IPropertyHandle > ElementHandle = SettingsHandle->GetChildHandle( i );
		if( !ElementHandle.IsValid() )
			continue;

		FString FullRelativePath;
		ElementHandle->GetChildHandle( GET_MEMBER_NAME_CHECKED( FConfigFileSettings, RelativePath ) )->GetValue( FullRelativePath );
		FullRelativePath.ReplaceInline( TEXT( "\\" ), TEXT( "/" ) );

		FString FileName;
		ElementHandle->GetChildHandle( GET_MEMBER_NAME_CHECKED( FConfigFileSettings, FileName ) )->GetValue( FileName );

		TArray< FString > PathParts;
		FullRelativePath.ParseIntoArray( PathParts, TEXT( "/" ) );

		TSharedPtr< FConfigTreeItem > CurrentParent  = nullptr;
		FString                       CumulativePath = "";

		for( int32 j = 0; j < PathParts.Num() - 1; ++j )
		{
			FString PartName = PathParts[ j ];
			CumulativePath   += ( j == 0 ? "" : "/" ) + PartName;

			if( !FolderCache.Contains( CumulativePath ) )
			{
				TSharedRef< FConfigTreeItem > NewFolder = MakeShared< FConfigTreeItem >();
				NewFolder->Name                         = PartName;
				NewFolder->bIsFolder                    = true;
				NewFolder->FullPath                     = CumulativePath;

				if( CurrentParent.IsValid() )
					CurrentParent->Children.Add( NewFolder );
				else
					RootItems.Add( NewFolder );

				FolderCache.Add( CumulativePath, NewFolder );
			}
			CurrentParent = FolderCache[ CumulativePath ];
		}

		TSharedRef< FConfigTreeItem > FileItem = MakeShared< FConfigTreeItem >();
		FileItem->Name                         = FileName;
		FileItem->bIsFolder                    = false;
		FileItem->FullPath                     = FullRelativePath;
		FileItem->EnabledHandle                = ElementHandle->GetChildHandle( GET_MEMBER_NAME_CHECKED( FConfigFileSettings, bEnabled ) );
		FileItem->ScopeHandle                  = ElementHandle->GetChildHandle( GET_MEMBER_NAME_CHECKED( FConfigFileSettings, SettingsScope ) );
		FileItem->AutoSyncHandle               = ElementHandle->GetChildHandle( GET_MEMBER_NAME_CHECKED( FConfigFileSettings, bAutoSyncEnabled ) );

		if( CurrentParent.IsValid() )
			CurrentParent->Children.Add( FileItem );
		else
			RootItems.Add( FileItem );
	}

	if( TreeView.IsValid() )
	{
		TreeView->RequestTreeRefresh();
	}
}

TSharedRef< ITableRow > FOmniSyncCustomization::OnGenerateRow( TSharedRef< FConfigTreeItem > InItem, const TSharedRef< STableViewBase >& OwnerTable ) const
{
	if( InItem->bIsFolder )
	{
		return SNew( STableRow< TSharedRef< FConfigTreeItem > >, OwnerTable )
			[
				SNew( STextBlock )
				.Text( FText::FromString( InItem->Name ) )
				.Font( IDetailLayoutBuilder::GetDetailFontBold() )

			];
	}
	return SNew( STableRow< TSharedRef< FConfigTreeItem > >, OwnerTable )
		.Padding( FMargin( 0, 2 ) )
		[
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0, 2 )
			[
				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0, 0, 4, 0 )
				.VAlign( VAlign_Center )
				[
					SNew( SButton )
					.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
					.ContentPadding( FMargin( 1, 0 ) )
					.OnClicked_Lambda( [InItem, this]()
					{
						InItem->bIsExpanded = !InItem->bIsExpanded;
						if( TreeView.IsValid() )
							TreeView->RequestTreeRefresh();
						return FReply::Handled();
					} )
					[
						SNew( STextBlock )
						.Text_Lambda( [InItem]()
						{
							return FText::FromString( InItem->bIsExpanded ? TEXT( "▼" ) : TEXT( "►" ) );
						} )
						.Font( FCoreStyle::GetDefaultFontStyle( "Regular", 8 ) )
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0, 0, 8, 0 )
				.VAlign( VAlign_Center )
				[
					SNew( SCheckBox )
					.IsChecked_Lambda( [InItem]()
					{
						bool bVal = false;
						if( InItem->EnabledHandle.IsValid() )
							InItem->EnabledHandle->GetValue( bVal );
						return bVal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					} )
					.OnCheckStateChanged_Lambda( [InItem, this]( ECheckBoxState NewState )
					{
						if( InItem->EnabledHandle.IsValid() )
						{
							InItem->EnabledHandle->SetValue( NewState == ECheckBoxState::Checked );
							if( ConfigObject.IsValid() )
								ConfigObject->OnSettingsChanged();
						}
					} )
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0, 0, 12, 0 )
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
					.Text( FText::FromString( InItem->Name ) )
					.Font( IDetailLayoutBuilder::GetDetailFontBold() )
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0, 0, 12, 0 )
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
					.Text_Lambda( [InItem]()
					{
						bool bEnabled = false;
						if( InItem->EnabledHandle.IsValid() )
							InItem->EnabledHandle->GetValue( bEnabled );
						if( !bEnabled )
							return LOCTEXT( "Disabled", "Disabled" );

						if( InItem->ScopeHandle.IsValid() )
						{
							uint8 ScopeValue = 0;
							InItem->ScopeHandle->GetValue( ScopeValue );
							switch( static_cast< EOmniSyncScope >( ScopeValue ) )
							{
								case EOmniSyncScope::Global:
									return LOCTEXT( "ScopeGlobal", "Global" );
								case EOmniSyncScope::PerEngineVersion:
									return LOCTEXT( "ScopePerEngine", "Per Engine" );
								case EOmniSyncScope::PerProject:
									return LOCTEXT( "ScopePerProject", "Per Project" );
							}
						}
						return FText::GetEmpty();
					} )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
					.ColorAndOpacity_Lambda( [InItem]()
					{
						bool bEnabled = false;
						if( InItem->EnabledHandle.IsValid() )
							InItem->EnabledHandle->GetValue( bEnabled );
						return bEnabled ? FLinearColor( 0.7f, 0.7f, 1.0f ) : FLinearColor( 0.5f, 0.5f, 0.5f );
					} )
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
					.Text_Lambda( [InItem]()
					{
						bool bEnabled = false;
						if( InItem->EnabledHandle.IsValid() )
							InItem->EnabledHandle->GetValue( bEnabled );
						if( !bEnabled )
							return FText::GetEmpty();

						bool bAutoSync = false;
						if( InItem->AutoSyncHandle.IsValid() )
							InItem->AutoSyncHandle->GetValue( bAutoSync );
						return bAutoSync ? LOCTEXT( "AutoSyncOn", "[Auto-Sync]" ) : LOCTEXT( "ManualSync", "[Manual]" );
					} )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
					.ColorAndOpacity_Lambda( [InItem]()
					{
						bool bAutoSync = false;
						if( InItem->AutoSyncHandle.IsValid() )
							InItem->AutoSyncHandle->GetValue( bAutoSync );
						return bAutoSync ? FLinearColor( 0.3f, 1.0f, 0.3f ) : FLinearColor( 0.5f, 0.5f, 0.5f );
					} )
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 32, 4, 0, 4 )
			[
				SNew( SVerticalBox )
				.Visibility_Lambda( [InItem]() { return InItem->bIsExpanded ? EVisibility::Visible : EVisibility::Collapsed; } )

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 0, 2 )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.FillWidth( 0.4f )
					.VAlign( VAlign_Center )
					.Padding( 0, 0, 8, 0 )
					[
						SNew( STextBlock )
						.Text( LOCTEXT( "SettingsScope", "Sync Scope" ) )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
					+ SHorizontalBox::Slot()
					.FillWidth( 0.6f )
					[
						InItem->ScopeHandle
						.IsValid()
							? InItem->ScopeHandle->CreatePropertyValueWidget()
							: SNullWidget::NullWidget
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 0, 2 )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.FillWidth( 0.4f )
					.VAlign( VAlign_Center )
					.Padding( 0, 0, 8, 0 )
					[
						SNew( STextBlock )
						.Text( LOCTEXT( "AutoSync", "Auto-Sync" ) )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
					+ SHorizontalBox::Slot()
					.FillWidth( 0.6f )
					[
						InItem->AutoSyncHandle
						.IsValid()
							? InItem->AutoSyncHandle->CreatePropertyValueWidget()
							: SNullWidget::NullWidget
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE