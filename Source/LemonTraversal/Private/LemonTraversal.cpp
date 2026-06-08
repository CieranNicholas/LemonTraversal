// Copyright Cieran Nicholas. All Rights Reserved.

#include "LemonTraversal.h"

#define LOCTEXT_NAMESPACE "FLemonTraversalModule"

DEFINE_LOG_CATEGORY(LogLemonTraversal);

void FLemonTraversalModule::StartupModule()
{
	UE_LOG(LogLemonTraversal, Log, TEXT("LemonTraversal module started."));
}

void FLemonTraversalModule::ShutdownModule()
{
	UE_LOG(LogLemonTraversal, Log, TEXT("LemonTraversal module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLemonTraversalModule, LemonTraversal)
