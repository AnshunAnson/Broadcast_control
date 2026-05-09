#include "VPStageUDPReceiver.h"

#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"

FVPStageUDPReceiver::FVPStageUDPReceiver()
	: Socket(nullptr)
	, ListenPort(0)
	, CommandQueue(nullptr)
	, Thread(nullptr)
	, bStopRequested(false)
	, bIsRunning(false)
	, ReceivedCount(0)
{
}

FVPStageUDPReceiver::~FVPStageUDPReceiver()
{
	Shutdown();
}

bool FVPStageUDPReceiver::Start(int32 Port, TQueue<FVPStageCommand, EQueueMode::Mpsc>* InCommandQueue)
{
	if (bIsRunning)
	{
		return false;
	}

	ListenPort = Port;
	CommandQueue = InCommandQueue;
	bStopRequested = false;

	Thread = FRunnableThread::Create(this, TEXT("VPStageUDPReceiver"), 0, TPri_AboveNormal);
	if (!Thread)
	{
		return false;
	}

	return true;
}

void FVPStageUDPReceiver::Shutdown()
{
	bStopRequested = true;

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FVPStageUDPReceiver::Init()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	FIPv4Endpoint Endpoint(FIPv4Address::Any, ListenPort);
	Socket = FUdpSocketBuilder(TEXT("VPStageUDP"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(256 * 1024);

	if (!Socket)
	{
		return false;
	}

	bIsRunning = true;
	return true;
}

uint32 FVPStageUDPReceiver::Run()
{
	uint8 Buffer[8];

	while (!bStopRequested)
	{
		if (!Socket)
		{
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		TSharedRef<FInternetAddr> SenderAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		int32 BytesRead = 0;

		bool bReceived = Socket->RecvFrom(Buffer, sizeof(Buffer), BytesRead, *SenderAddr);
		if (!bReceived)
		{
			FPlatformProcess::Sleep(0.001f);
			continue;
		}

		if (BytesRead != 8)
		{
			continue;
		}

		FVPStageCommand Command;
		Command.ParamHash =
			(static_cast<uint32>(Buffer[0]) << 24) |
			(static_cast<uint32>(Buffer[1]) << 16) |
			(static_cast<uint32>(Buffer[2]) << 8)  |
			(static_cast<uint32>(Buffer[3]));

		uint32 ValueBits =
			(static_cast<uint32>(Buffer[4]) << 24) |
			(static_cast<uint32>(Buffer[5]) << 16) |
			(static_cast<uint32>(Buffer[6]) << 8)  |
			(static_cast<uint32>(Buffer[7]));
		Command.Value = *reinterpret_cast<float*>(&ValueBits);

		CommandQueue->Enqueue(Command);
		ReceivedCount++;
	}

	bIsRunning = false;
	return 0;
}

void FVPStageUDPReceiver::Exit()
{
	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void FVPStageUDPReceiver::Stop()
{
	bStopRequested = true;
}