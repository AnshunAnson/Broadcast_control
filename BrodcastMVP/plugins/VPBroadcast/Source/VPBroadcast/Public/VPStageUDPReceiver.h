#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"

struct FVPStageCommand
{
	int32 ParamHash;
	float Value;
};

class FVPStageUDPReceiver : public FRunnable
{
public:
	FVPStageUDPReceiver();
	virtual ~FVPStageUDPReceiver();

	bool Start(int32 Port, TQueue<FVPStageCommand, EQueueMode::Mpsc>* InCommandQueue);

	void Shutdown();

	uint32 GetReceivedCount() const { return ReceivedCount; }
	bool IsRunning() const { return bIsRunning; }

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	class FSocket* Socket;

	int32 ListenPort;

	TQueue<FVPStageCommand, EQueueMode::Mpsc>* CommandQueue;

	FRunnableThread* Thread;

	FThreadSafeBool bStopRequested;
	FThreadSafeBool bIsRunning;

	uint32 ReceivedCount;
};