#include "Network/NetworkHandler.h"
#include "Net/DataChannel.h"
#include "FGGameInstance.h"
#include "FGGameMode.h"
#include "Patching/NativeHookManager.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"

DEFINE_LOG_CATEGORY(LogModNetworkHandler);
DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(ModMessage, 40, FString, int32, FString);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(ModMessage);

FMessageEntry& UModNetworkHandler::RegisterMessageType(const FMessageType& MessageType) {
    UE_LOG(LogModNetworkHandler, Display, TEXT("Registering message type %s:%d"), *MessageType.ModReference, MessageType.MessageId);
    TMap<int32, FMessageEntry>& ModEntries = MessageHandlers.FindOrAdd(MessageType.ModReference);
    if (ModEntries.Contains(MessageType.MessageId)) {
        UE_LOG(LogModNetworkHandler, Fatal, TEXT("Tried to register mod message with duplicate identifier %d for mod %s"), MessageType.MessageId, *MessageType.ModReference);
        check(0);
    }
    return ModEntries.Add(MessageType.MessageId, FMessageEntry{});
}

void UModNetworkHandler::CloseWithFailureMessage(UNetConnection* Connection, const FString& Message) {
    FString MutableMessage = Message;
    FNetControlMessage<NMT_Failure>::Send(Connection, MutableMessage);
    Connection->FlushNet(true);
}

void UModNetworkHandler::SendMessage(UNetConnection* Connection, FMessageType MessageType, FString Data) {
    FNetControlMessage<NMT_ModMessage>::Send(Connection, MessageType.ModReference, MessageType.MessageId, Data);
    Connection->FlushNet(true);
}

UGameInstance* UModNetworkHandler::GetGameInstanceFromNetDriver( const UNetDriver* NetDriver )
{
	if ( NetDriver )
	{
		if ( NetDriver->World != nullptr && NetDriver->World->GetGameInstance() != nullptr )
		{
			return NetDriver->World->GetGameInstance();
		}
		const FWorldContext* Context = GEngine->GetWorldContextFromPendingNetGameNetDriver( NetDriver );
		if ( Context != nullptr && Context->OwningGameInstance != nullptr )
		{
			return Context->OwningGameInstance;
		}
	}
	return nullptr;
}

void UModNetworkHandler::ReceiveMessage(UNetConnection* Connection, const FString& ModId, int32 MessageId, const FString& Content) const {
    const TMap<int32, FMessageEntry>* Result = MessageHandlers.Find(ModId);
    if (Result != nullptr) {
        const FMessageEntry* MessageEntry = Result->Find(MessageId);
        if (MessageEntry != nullptr) {
            const bool bIsClientSide = Connection->ClientLoginState == EClientLoginState::Invalid;
            const bool bCanBeHandled = (bIsClientSide && MessageEntry->bClientHandled) || (!bIsClientSide && MessageEntry->bServerHandled);
            if (bCanBeHandled) {
                MessageEntry->MessageReceived.ExecuteIfBound(Connection, Content);
            }
        }
    }
}

void UModNetworkHandler::InitializePatches() {
	
    UWorld* WorldObjectInstance = GetMutableDefault<UWorld>();
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(UWorld::WelcomePlayer, WorldObjectInstance, [=](UWorld* ServerWorld, UNetConnection* Connection) {
        UModNetworkHandler* NetworkHandler = GEngine->GetEngineSubsystem<UModNetworkHandler>();
        NetworkHandler->OnWelcomePlayer().Broadcast(ServerWorld, Connection);
    });
	
    SUBSCRIBE_METHOD_AFTER(UPendingNetGame::SendInitialJoin, [=](UPendingNetGame* NetGame) {
        if (NetGame->NetDriver != nullptr) {
            UNetConnection* ServerConnection = NetGame->NetDriver->ServerConnection;
            if (ServerConnection != nullptr) {
                UModNetworkHandler* NetworkHandler = GEngine->GetEngineSubsystem<UModNetworkHandler>();
                NetworkHandler->OnClientInitialJoin().Broadcast(ServerConnection);
            }
        }
    });
	
    auto MessageHandler = [=](auto& Call, void*, UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) {
        if (MessageType == NMT_ModMessage) {
            FString ModId; int32 MessageId; FString Content;
            if (FNetControlMessage<NMT_ModMessage>::Receive(Bunch, ModId, MessageId, Content)) {
                UModNetworkHandler* NetworkHandler = GEngine->GetEngineSubsystem<UModNetworkHandler>();
                NetworkHandler->ReceiveMessage(Connection, ModId, MessageId, Content);
                Call.Cancel();
            }
        }
    };

    SUBSCRIBE_METHOD_VIRTUAL(UWorld::NotifyControlMessage, WorldObjectInstance, MessageHandler);

    // Same for UPendingNetGame
    UPendingNetGame* PendingNetGame = static_cast<UPendingNetGame *>(FindObjectChecked<UClass>(NULL, TEXT("/Script/Engine.PendingNetGame"))->GetDefaultObject());
    SUBSCRIBE_METHOD_VIRTUAL(UPendingNetGame::NotifyControlMessage, PendingNetGame, MessageHandler);
}
