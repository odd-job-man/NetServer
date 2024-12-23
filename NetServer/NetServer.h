#pragma once
#include "IHandler.h"
#include "CLockFreeStack.h"
#include "MyOVERLAPPED.h"
#include "Timer.h"


class Stack;
class SmartPacket;

class NetServer : public IHandler
{
public:
	NetServer(const WCHAR* pTextFileStr);
	// Test용
	void SENDPACKET(ULONGLONG id, SmartPacket& sendPacket);
	void SEND_POST_PER_FRAME();
	void SendPacket(ULONGLONG id, SmartPacket& sendPacket);
	void SendPacket(ULONGLONG id, Packet* pPacket);
	void SendPacket_ALREADY_ENCODED(ULONGLONG id, Packet* pPacket);
	void SendPacket_ENQUEUE_ONLY(ULONGLONG id, Packet* pPacket);
	virtual BOOL OnConnectionRequest() = 0;
	virtual void* OnAccept(ULONGLONG id) = 0;
	virtual void OnRelease(ULONGLONG id) = 0;
	virtual void OnRecv(ULONGLONG id, Packet* pPacket) = 0;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) = 0;
	virtual void OnPost(void* order) = 0;
	virtual void OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown() = 0; // 일반적으로 DB스레드에대한 PQCS를 쏠때 사용할것이다

	void WaitUntilShutDown(); // 메인스레드는 start이후 무조건 이걸 호출해야한다
	void ShutDown(); // WaitUntilShutDown에서 호출
	void RequestShutDown(); // 워커스레드가 호출
	// 디버깅용
	void Disconnect(ULONGLONG id);
	void SendPostPerFrame();
protected:
	void SendPostPerFrame_IMPL(LONG* pCounter);
private:
	void ProcessTimeOut();
	static unsigned __stdcall AcceptThread(LPVOID arg);
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
public:
	// Accept
	DWORD IOCP_WORKER_THREAD_NUM_ = 0;
	DWORD IOCP_ACTIVE_THREAD_NUM_ = 0;
	LONG lSessionNum_ = 0;
	LONG maxSession_ = 0;
	LONG TIME_OUT_MILLISECONDS_ = 0;
	ULONGLONG TIME_OUT_CHECK_INTERVAL_ = 0;
	ULONGLONG ullIdCounter = 0;
	NetSession* pSessionArr_;
	CLockFreeStack<short> DisconnectStack_;
	const MYOVERLAPPED SendPostFrameOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::SEND_POST_FRAME };
	const MYOVERLAPPED SendWorkerOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::SEND_WORKER };
	const MYOVERLAPPED OnPostOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::POST };
	HANDLE hcp_;
	HANDLE hAcceptThread_;
	HANDLE* hIOCPWorkerThreadArr_;
	HANDLE hSendPostEndEvent_;
	HANDLE hShutDownEvent_;
	SOCKET hListenSock_;
	LONG updateThreadSendCounter_ = 0;

	virtual BOOL RecvPost(NetSession* pSession);
	virtual BOOL SendPost(NetSession* pSession);
	virtual BOOL SendPostAccum(NetSession* pSession);
	virtual void ReleaseSession(NetSession* pSession);
	void RecvProc(NetSession* pSession, int numberOfBytesTransferred);
	void SendProc(NetSession* pSession, DWORD dwNumberOfBytesTransferred);
	void SendProcAccum(NetSession* pSession, DWORD dwNumberOfBytesTransferred);
	friend class Packet;

	int bAccSend = 0;
	// Monitoring 변수
	// Recv (Per MSG)

public:
	LONG lPlayerNum = 0;
	ULONGLONG acceptCounter_ = 0;
	alignas(64) ULONGLONG acceptTotal_ = 0;
	alignas(64) ULONGLONG recvTPS_ = 0;

	// Send (Per MSG)
	alignas(64) LONG sendTPS_ = 0;

	// Disconnect
	alignas(64) ULONGLONG disconnectTPS_ = 0;
};

