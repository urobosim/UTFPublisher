// Copyright 2018, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "TFPublisher.h"
#include "tf2_msgs/TFMessage.h"
#include "RosSettings.h"
#include "Kismet/GameplayStatics.h"
#include "RosSettings.h"

// Sets default values
ATFPublisher::ATFPublisher()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// set default TF root frame
	TFRootFrameName = TEXT("map");

	// Update on tick by default
	bUseConstantPublishRate = false;

	// Use root node as blank (no relative transformations calculated with identity transform)
	bUseBlankRootNode = true;

	// Default timer delta time (s) (0 = on Tick)
	ConstantPublishRate = 0.0f;

	// ROSBridge server default values
	ServerIP = "127.0.0.1";
	ServerPORT = 9090;
}

// Called when the game starts or when spawned
void ATFPublisher::BeginPlay()
{
	Super::BeginPlay();
        if(bUseGlobalIP)
          {
            const URosSettings* Settings = GetDefault<URosSettings>();
            ServerIP = Settings->ROSBridgeServerHost;
            ServerPORT = Settings->ROSBridgeServerPort;
          }

	// Build TF tree
	BuildTFTree();

	// Create the ROSBridge handler for connecting with ROS
	ROSBridgeHandler = MakeShareable<FROSBridgeHandler>(
		new FROSBridgeHandler(ServerIP, ServerPORT));

	// Create the tf publisher
	TFPublisher = MakeShareable<FROSBridgePublisher>(
		new FROSBridgePublisher(TFTopic, "tf2_msgs/TFMessage"));

	// Connect to ROS
	ROSBridgeHandler->Connect();

	// Add publisher
	ROSBridgeHandler->AddPublisher(TFPublisher);

	// Bind publish function to timer
	if (bUseConstantPublishRate)
	{
		if (ConstantPublishRate > 0.f)
		{
			// Disable tick
			SetActorTickEnabled(false);
			// Setup timer
			GetWorldTimerManager().SetTimer(TFPubTimer, this, &ATFPublisher::PublishTF, ConstantPublishRate, true);
		}
	}
	else
	{
		// Take into account the PublishRate Tag key value pair (if missing, publish on tick)
	}
}

// Called when destroyed or game stopped
void ATFPublisher::EndPlay(const EEndPlayReason::Type Reason)
{
	// Disconnect before parent ends
	ROSBridgeHandler->Disconnect();

	Super::EndPlay(Reason);
}

// Called every frame
void ATFPublisher::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Publish tf
	PublishTF();
}

// Build TF tree
void ATFPublisher::BuildTFTree()
{
	// Create root node
	TFRootNode = NewObject<UTFNode>(this);
	TFRootNode->RegisterComponent();

	if (bUseBlankRootNode)
	{
		// Init node as blank (no relative transform calculation with root children)
		TFRootNode->Init(TFRootFrameName, &TFTree);
	}
	else
	{
		// TF root node uses this actor as origin position
		// relative calculation now need to happen between root and its children
		TFRootNode->Init(TFRootFrameName, &TFTree, this);
	}

	// Initialize tree with the root node
	TFTree.Init(TFRootNode);

	// Bind root node transform function pointer (call after adding to tree)
	TFRootNode->BindTransformFunction();

	// Build tree
	TFTree.Build(GetWorld());
}

// Publish tf tree
void ATFPublisher::PublishTF()
{
	// Current time as ROS time
	FROSTime TimeNow = FROSTime::Now();

	// Create TFMessage
	TSharedPtr<tf2_msgs::TFMessage> TFMsgPtr = TFTree.GetTFMessageMsg(TimeNow, Seq);

	// PUB
	if(TFMsgPtr->GetTransforms().Num()!=0)
	{
	 	ROSBridgeHandler->PublishMsg(TFTopic, TFMsgPtr);
	}

	ROSBridgeHandler->Process();

	// Update message sequence
	Seq++;
}

void ATFPublisher::AddObject(UObject* InObject)
{
  FString Name = InObject->GetName();
  if(AActor* A = Cast<AActor>(InObject))
    {
      Name = FTags::GetValue(A->Tags, TEXT("SemLog"), TEXT("Id"));
      Name = TFPrefix + TEXT("/") + Name;
    }
  if(!TFTree.FindNode(Name))
    {
      UE_LOG(LogTF, Log, TEXT("Object created %s"), *Name);
      TFTree.AddRootChildNode(Name, InObject);
    }
}
