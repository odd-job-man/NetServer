#pragma once
#include "IHandler.h"
#include "CLockFreeStack.h"

class Stack;
class SmartPacket;

class NetServer : public IHandler
{
public:
	NetServer();
	virtual void SendPacket(ULONGLONG id, SmartPacket& sendPacket);
	virtual BOOL OnConnectionRequest() = 0;
	virtual void* OnAccept(ULONGLONG id) = 0;
	virtual void OnRelease(ULONGLONG id) = 0;
	virtual void OnRecv(ULONGLONG id, Packet* pPacket) = 0;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) = 0;
	// 디버깅용
	void Disconnect(ULONGLONG id);
	static unsigned __stdcall AcceptThread(LPVOID arg);
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
public:
	// Accept
	DWORD IOCP_WORKER_THREAD_NUM_ = 0;
	LONG lSessionNum_ = 0;
	LONG maxSession_ = 0;
	LONG TIME_OUT_MILLISECONDS_ = 0;
	ULONGLONG ullIdCounter = 0;
	Session* pSessionArr_;
	CLockFreeStack<short> DisconnectStack_;
	HANDLE hcp_;
	HANDLE hAcceptThread_;
	HANDLE* hIOCPWorkerThreadArr_;
	SOCKET hListenSock_;
	virtual BOOL RecvPost(Session* pSession);
	virtual BOOL SendPost(Session* pSession);
	virtual void ReleaseSession(Session* pSession);
	void RecvProc(Session* pSession, int numberOfBytesTransferred);
	void SendProc(Session* pSession, DWORD dwNumberOfBytesTransferred);
	friend class Packet;
	// Monitoring 변수
	// Recv (Per MSG)

public:
	LONG lPlayerNum = 0;
	LONG lAcceptTotal_PREV = 0;
	alignas(64) LONG lAcceptTotal_ = 0;
	alignas(64) LONG lRecvTPS_ = 0;

	// Send (Per MSG)
	alignas(64) LONG lSendTPS_ = 0;

	// Disconnect
	alignas(64) LONG lDisconnectTPS_ = 0;
};

