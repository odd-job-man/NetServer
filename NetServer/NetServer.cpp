#include <Winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <process.h>
#include "CLockFreeQueue.h"
#include "RingBuffer.h"
#include "Session.h"
#include "Packet.h"
#include "ErrType.h"
#include "Logger.h"
#include "Parser.h"
#include <locale>
#include "NetServer.h"
#pragma comment(lib,"LoggerMt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"TextParser.lib")

NetServer::NetServer()
{
	std::locale::global(std::locale(""));
	char* pStart;
	char* pEnd;
	PARSER psr = CreateParser(L"ServerConfig.txt");

	WCHAR ipStr[16];
	GetValue(psr, L"BIND_IP", (PVOID*)&pStart, (PVOID*)&pEnd);
	unsigned long long stringLen = (pEnd - pStart) / sizeof(WCHAR);
	wcsncpy_s(ipStr, _countof(ipStr) - 1, (const WCHAR*)pStart, stringLen);
	// Null terminated String ���� ������ InetPtonW��������
	ipStr[stringLen] = 0;

	ULONG ip;
	InetPtonW(AF_INET, ipStr, &ip);
	GetValue(psr, L"BIND_PORT", (PVOID*)&pStart, nullptr);
	short SERVER_PORT = (short)_wtoi((LPCWSTR)pStart);

	GetValue(psr, L"IOCP_WORKER_THREAD", (PVOID*)&pStart, nullptr);
	IOCP_WORKER_THREAD_NUM_ = (DWORD)_wtoi((LPCWSTR)pStart);

	GetValue(psr, L"IOCP_ACTIVE_THREAD", (PVOID*)&pStart, nullptr);
	DWORD IOCP_ACTIVE_THREAD_NUM = (DWORD)_wtoi((LPCWSTR)pStart);

	GetValue(psr, L"IS_ZERO_BYTE_SEND", (PVOID*)&pStart, nullptr);
	int bZeroByteSend = _wtoi((LPCWSTR)pStart);

	GetValue(psr, L"SESSION_MAX", (PVOID*)&pStart, nullptr);
	maxSession_ = _wtoi((LPCWSTR)pStart);

	GetValue(psr, L"PACKET_CODE", (PVOID*)&pStart, nullptr);
	Packet::PACKET_CODE = (unsigned char)_wtoi((LPCWSTR)pStart);

	GetValue(psr, L"PACKET_KEY", (PVOID*)&pStart, nullptr);
	Packet::FIXED_KEY = (unsigned char)_wtoi((LPCWSTR)pStart);

	GetValue(psr, L"TIME_OUT_MILLISECONDS", (PVOID*)&pStart, nullptr);
	TIME_OUT_MILLISECONDS_ = _wtoi((LPCWSTR)pStart);
	ReleaseParser(psr);

#ifdef DEBUG_LEAK
	InitializeCriticalSection(&Packet::cs_for_debug_leak);
#endif

	int retval;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp OK!");
	// NOCT�� 0���� �����μ��� ����ŭ�� ������
	hcp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, IOCP_ACTIVE_THREAD_NUM);
	if (!hcp_)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"CreateIoCompletionPort Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	hListenSock_ = socket(AF_INET, SOCK_STREAM, 0);
	if (hListenSock_ == INVALID_SOCKET)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET OK");

	// bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = ip;
	serveraddr.sin_port = htons(SERVER_PORT);
	retval = bind(hListenSock_, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind OK");

	// listen
	retval = listen(hListenSock_, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen() OK");

	linger linger;
	linger.l_linger = 0;
	linger.l_onoff = 1;
	setsockopt(hListenSock_, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"linger() OK");

	if (bZeroByteSend == 1)
	{
		DWORD dwSendBufSize = 0;
		setsockopt(hListenSock_, SOL_SOCKET, SO_SNDBUF, (char*)&dwSendBufSize, sizeof(dwSendBufSize));
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"ZeroByte Send OK");
	}
	else
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"NO ZeroByte Send");
	}

	hIOCPWorkerThreadArr_ = new HANDLE[IOCP_WORKER_THREAD_NUM_];
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
	{
		hIOCPWorkerThreadArr_[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, nullptr);
		if (!hIOCPWorkerThreadArr_[i])
		{
			LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE WorkerThread Fail ErrCode : %u", WSAGetLastError());
			__debugbreak();
		}
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", si.dwNumberOfProcessors);

	// ���� 17��Ʈ�� ������ ������Ʈ�� 16�� ���ϰ� �Ǵ³����� ������� ū�׸��̴�.
	if (!CAddressTranslator::CheckMetaCntBits())
		__debugbreak();

	pSessionArr_ = new Session[maxSession_];
	for (int i = maxSession_ - 1; i >= 0; --i)
		DisconnectStack_.Push(i);

	hAcceptThread_ = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, nullptr);
	if (!hAcceptThread_)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE AccpetThread Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE AccpetThread OK!");
}

void NetServer::SendPacket(ULONGLONG id, SmartPacket& sendPacket)
{
	Session* pSession = pSessionArr_ + Session::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->IoCnt_);

	// �̹� RELEASE �������̰ų� RELEASE�� ���
	if ((IoCnt & Session::RELEASE_FLAG) == Session::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE �Ϸ��� �ٽ� ���ǿ� ���� �ʱ�ȭ�� �Ϸ�Ȱ�� �� ��Ȱ��
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// ���ڵ�
	sendPacket->SetHeader<Net>();
	sendPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(sendPacket.GetPacket());
	SendPost(pSession);
	if (InterlockedDecrement(&pSession->IoCnt_) == 0)
		ReleaseSession(pSession);
}

void NetServer::SendPacket(ULONGLONG id, Packet* pPacket)
{
	Session* pSession = pSessionArr_ + Session::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->IoCnt_);

	// �̹� RELEASE �������̰ų� RELEASE�� ���
	if ((IoCnt & Session::RELEASE_FLAG) == Session::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE �Ϸ��� �ٽ� ���ǿ� ���� �ʱ�ȭ�� �Ϸ�Ȱ�� �� ��Ȱ��
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// ���ڵ�
	pPacket->SetHeader<Net>();
	pPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(pPacket);
	SendPost(pSession);
	if (InterlockedDecrement(&pSession->IoCnt_) == 0)
		ReleaseSession(pSession);
}

void NetServer::Disconnect(ULONGLONG id)
{
	Session* pSession = pSessionArr_ + Session::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->IoCnt_);

	// RELEASE������ Ȥ�� ����Ϸ�
	if ((IoCnt & Session::RELEASE_FLAG) == Session::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
		{
			ReleaseSession(pSession);
			return;
		}
	}

	// RELEASE�� ��Ȱ����� �Ǿ�����
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
		{
			ReleaseSession(pSession);
			return;
		}
	}

	// Disconnect 1ȸ ����
	if (pSession->bDisconnectCalled_ == true)
	{
		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
		{
			ReleaseSession(pSession);
			return;
		}
	}

	// ���� �����޴ٸ� ���� ���ǿ� ���ؼ� RELEASE ���� ȣ����� ������������ ����ȴ�
	pSession->bDisconnectCalled_ = true;
	CancelIoEx((HANDLE)pSession->sock_, nullptr);

	// CancelIoExȣ��� ���ؼ� RELEASE�� ȣ��Ǿ���� ������ �������� InterlockedIncrement ������ ȣ���� �ȵ� ��� ����û��
	if (InterlockedDecrement(&pSession->IoCnt_) == 0)
		ReleaseSession(pSession);
}

unsigned __stdcall NetServer::AcceptThread(LPVOID arg)
{
	SOCKET clientSock;
	SOCKADDR_IN clientAddr;
	int addrlen;
	NetServer* pNetServer = (NetServer*)arg;
	addrlen = sizeof(clientAddr);

	while (1)
	{
		clientSock = accept(pNetServer->hListenSock_, (SOCKADDR*)&clientAddr, &addrlen);
		if (clientSock == INVALID_SOCKET)
		{
			DWORD dwErrCode = WSAGetLastError();
			if (dwErrCode != WSAEINTR && dwErrCode != WSAENOTSOCK)
			{
				__debugbreak();
			}
			return 0;
		}

		if (!pNetServer->OnConnectionRequest())
		{
			closesocket(clientSock);
			continue;
		}

		InterlockedIncrement((LONG*)&pNetServer->lAcceptTotal_);
		InterlockedIncrement((LONG*)&pNetServer->lSessionNum_);

		short idx = pNetServer->DisconnectStack_.Pop().value();
		Session* pSession = pNetServer->pSessionArr_ + idx;
		pSession->Init(clientSock, pNetServer->ullIdCounter, idx);

		CreateIoCompletionPort((HANDLE)pSession->sock_, pNetServer->hcp_, (ULONG_PTR)pSession, 0);
		++pNetServer->ullIdCounter;

		InterlockedIncrement(&pSession->IoCnt_);
		InterlockedAnd(&pSession->IoCnt_, ~Session::RELEASE_FLAG);

		pNetServer->OnAccept(pSession->id_);
		pNetServer->RecvPost(pSession);

		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
			pNetServer->ReleaseSession(pSession);
	}
	return 0;
}

unsigned __stdcall NetServer::IOCPWorkerThread(LPVOID arg)
{
	NetServer* pNetServer = (NetServer*)arg;
	while (1)
	{
		WSAOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		Session* pSession = nullptr;
		bool bPost = false;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pNetServer->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);
		do
		{
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			//��������
			if (bGQCSRet && dwNOBT == 0)
				break;

			//������ ����
			//�α��� �Ϸ������� GQCS ���� WSAGetLastError ���� 64�� ���� �������.
			//���� WSASend�� WSARecv, �� �� �ϳ��� �ٷ� �����ϴ� ��쿡�� �α��ϴ°�����...
			if (!bGQCSRet && pOverlapped)
				break;

			// PQCS�� ���� �i���� ������
			if (dwNOBT == 1 && (ULONG_PTR)pSession == 1)
			{
#pragma warning(disable : 4311)
#pragma warning(disable : 4302)
				pNetServer->OnPost((int)pOverlapped);
				bPost = true;
				break;
#pragma warning(default: 4311)
#pragma warning(default: 4302)
			}

			if (&pSession->recvOverlapped == pOverlapped)
				pNetServer->RecvProc(pSession, dwNOBT);
			else
				pNetServer->SendProc(pSession, dwNOBT);

		} while (0);

		if (bPost == true)
			continue;

		if (InterlockedDecrement(&pSession->IoCnt_) == 0)
			pNetServer->ReleaseSession(pSession);
	}
	return 0;
}

BOOL NetServer::RecvPost(Session* pSession)
{
	if (pSession->bDisconnectCalled_ == true)
		return FALSE;

	WSABUF wsa[2];
	wsa[0].buf = pSession->recvRB_.GetWriteStartPtr();
	wsa[0].len = pSession->recvRB_.DirectEnqueueSize();
	wsa[1].buf = pSession->recvRB_.Buffer_;
	wsa[1].len = pSession->recvRB_.GetFreeSize() - wsa[0].len;

	ZeroMemory(&pSession->recvOverlapped, sizeof(WSAOVERLAPPED));
	DWORD flags = 0;
	InterlockedIncrement(&pSession->IoCnt_);
	int iRecvRet = WSARecv(pSession->sock_, wsa, 2, nullptr, &flags, &(pSession->recvOverlapped), nullptr);
	if (iRecvRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			if (pSession->bDisconnectCalled_ == true)
			{
				InterlockedDecrement(&(pSession->IoCnt_));
				CancelIoEx((HANDLE)pSession->sock_, nullptr);
				return FALSE;
			}
			return TRUE;
		}

		InterlockedDecrement(&(pSession->IoCnt_));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

BOOL NetServer::SendPost(Session* pSession)
{
	if (pSession->bDisconnectCalled_ == true)
		return TRUE;

	// ���� ���� TRUE�� �ٲ۴�. ���� TRUE���ٸ� ��ȯ���� TRUE�ϰ��̸� �׷��ٸ� ���� SEND �������̱� ������ �׳� ����������
	// �� ���ǹ��� ��ġ�� ���Ͽ� Out�� �ٲ��� ���������� ����ȴ�.
	// ������ SendPost ������ü�� Send�Ϸ����� �������� ��쿡�� in�� ��ġ�� SendPacket���� ���ؼ� �ٲ���� �ִ�.
	// iUseSize�� ���ϴ� ���������� DirectDequeueSize�� ���� �޶������ִ�.
	if (InterlockedExchange((LONG*)&pSession->bSendingInProgress_, TRUE) == TRUE)
		return TRUE;

	// SendPacket���� in�� �Űܼ� UseSize�� 0���� Ŀ���������� Send�Ϸ������� �����ؼ� Out�� �ű�� �÷��� ���� Recv�Ϸ����� �����尡 ���� SendPost�� ������ �÷��׸� �����Ѱ�� UseSize�� 0�̳��´�.
	// ���⼭ flag�� �ٽ� FALSE�� �ٲپ����� �ʾƼ� ����߻�
	DWORD dwBufferNum = pSession->sendPacketQ_.GetSize();
	if (dwBufferNum == 0)
	{
		InterlockedExchange((LONG*)&pSession->bSendingInProgress_, FALSE);
		return TRUE;
	}

	WSABUF wsa[50];
	DWORD i;
	for (i = 0; i < 50 && i < dwBufferNum; ++i)
	{
		Packet* pPacket = pSession->sendPacketQ_.Dequeue().value();
		wsa[i].buf = (char*)pPacket->pBuffer_;
		wsa[i].len = pPacket->GetUsedDataSize() + sizeof(Packet::NetHeader);
		pSession->pSendPacketArr_[i] = pPacket;
	}

	InterlockedExchange(&pSession->lSendBufNum_, i);
	InterlockedAdd(&lSendTPS_, i);
	InterlockedIncrement(&pSession->IoCnt_);
	ZeroMemory(&(pSession->sendOverlapped), sizeof(WSAOVERLAPPED));
	int iSendRet = WSASend(pSession->sock_, wsa, i, nullptr, 0, &(pSession->sendOverlapped), nullptr);
	if (iSendRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			if (pSession->bDisconnectCalled_ == true)
			{
				CancelIoEx((HANDLE)pSession->sock_, nullptr);
				return FALSE;
			}
			return TRUE;
		}

		//InterlockedExchange((LONG*)&pSession->bSendingInProgress_, FALSE);
		InterlockedDecrement(&(pSession->IoCnt_));

		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

void NetServer::ReleaseSession(Session* pSession)
{
	if (InterlockedCompareExchange(&pSession->IoCnt_, Session::RELEASE_FLAG | 0, 0) != 0)
		return;

	// Release �� Session�� ����ȭ ���� ����
	for (LONG i = 0; i < pSession->lSendBufNum_; ++i)
	{
		Packet* pPacket = pSession->pSendPacketArr_[i];
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	LONG size = pSession->sendPacketQ_.GetSize();
	for (LONG i = 0; i < size; ++i)
	{
		Packet* pPacket = pSession->sendPacketQ_.Dequeue().value();
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	closesocket(pSession->sock_);
	if (pSession->sendPacketQ_.GetSize() > 0)
		__debugbreak();

	// OnRelease�� idx Ǫ�� ������ �ٲ� JOB_OnRelease ���� ������ ���ο� �÷��̾ ���� JOB_On_ACCEPT�� �ߺ����� ��������
	OnRelease(pSession->id_);
	DisconnectStack_.Push((short)(pSession - pSessionArr_));
	InterlockedIncrement(&lDisconnectTPS_);
	InterlockedDecrement(&lSessionNum_);
}

void NetServer::RecvProc(Session* pSession, int numberOfBytesTransferred)
{
	using NetHeader = Packet::NetHeader;
	pSession->recvRB_.MoveInPos(numberOfBytesTransferred);
	while (1)
	{
		if (pSession->bDisconnectCalled_ == true)
			return;

		Packet* pPacket = PACKET_ALLOC(Net);
		if (pSession->recvRB_.Peek(pPacket->pBuffer_, sizeof(NetHeader)) == 0)
		{
			PACKET_FREE(pPacket);
			break;
		}

		int payloadLen = ((NetHeader*)pPacket->pBuffer_)->payloadLen_;

		if (pSession->recvRB_.GetUseSize() < sizeof(NetHeader) + payloadLen)
		{
			PACKET_FREE(pPacket);
			break;
		}

		pSession->recvRB_.MoveOutPos(sizeof(NetHeader));
		pSession->recvRB_.Dequeue(pPacket->GetPayloadStartPos<Net>(), payloadLen);
		pPacket->MoveWritePos(payloadLen);

		// �ݼ��������� ȣ��Ǵ� �Լ��� ���� �� ���ڵ��� üũ�� Ȯ��
		if (pPacket->ValidateReceived() == false)
		{
			PACKET_FREE(pPacket);
			Disconnect(pSession->id_);
			return;
		}
		OnRecv(pSession->id_, pPacket);
		InterlockedIncrement(&lRecvTPS_);
	}
	RecvPost(pSession);
}

void NetServer::SendProc(Session* pSession, DWORD dwNumberOfBytesTransferred)
{
	LONG sendBufNum = pSession->lSendBufNum_;
	pSession->lSendBufNum_ = 0;
	for (LONG i = 0; i < sendBufNum; ++i)
	{
		Packet* pPacket = pSession->pSendPacketArr_[i];
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	InterlockedExchange((LONG*)&pSession->bSendingInProgress_, FALSE);
	if (pSession->sendPacketQ_.GetSize() > 0)
		SendPost(pSession);
}
