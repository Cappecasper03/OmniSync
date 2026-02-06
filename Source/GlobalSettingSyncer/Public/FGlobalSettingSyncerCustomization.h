#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UGlobalSettingSyncerConfig;

struct FConfigTreeItem
{
	FString Name;
	FString FullPath;
	bool    bIsFolder = false;

	TSharedPtr< IPropertyHandle > EnabledHandle;
	TSharedPtr< IPropertyHandle > ScopeHandle;
	TSharedPtr< IPropertyHandle > AutoSyncHandle;

	TArray< TSharedRef< FConfigTreeItem > > Children;

	bool bIsExpanded = false;
};

class FGlobalSettingSyncerCustomization : public IDetailCustomization
{
public:
	static TSharedRef< IDetailCustomization > MakeInstance() { return MakeShareable( new FGlobalSettingSyncerCustomization ); }

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	void RefreshTreeData( const IDetailLayoutBuilder& DetailBuilder );

	TSharedRef< ITableRow > OnGenerateRow( TSharedRef< FConfigTreeItem > InItem, const TSharedRef< STableViewBase >& OwnerTable ) const;
	void                    OnGetChildren( TSharedRef< FConfigTreeItem > InItem, TArray< TSharedRef< FConfigTreeItem > >& OutChildren ) { OutChildren = InItem->Children; }

	TWeakObjectPtr< UGlobalSettingSyncerConfig >             ConfigObject;
	TSharedPtr< STreeView< TSharedRef< FConfigTreeItem > > > TreeView;
	TArray< TSharedRef< FConfigTreeItem > >                  RootItems;
};