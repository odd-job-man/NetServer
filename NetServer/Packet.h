#pragma once
#include <memory.h>
#include "CLockFreeQueue.h"
#include <stddef.h>
#include "Session.h"
//#include "LanServer.h"
#include "NetServer.h"

#define QUEUE
#include "CTlsObjectPool.h"


//#define DEBUG_LEAK

using NET_HEADER = short;

enum RecvType 
{
	JOB,
	RECVED_PACKET
};

enum ServerType
{
	Net,
	Lan
};


#ifdef DEBUG_LEAK
#include <list>
#include "CLinkedList.h"
#define PACKET_ALLOC(type) Packet::AllocForDebugLeak<type>(__func__)
#define PACKET_FREE(pPacket) Packet::FreeForDebugLeak(pPacket);
#else
#define PACKET_ALLOC(type) Packet::Alloc<type>()
#define PACKET_FREE(pPacket) Packet::Free(pPacket)
#endif

class Packet
{
public:
#pragma pack(push,1)
	struct LanHeader
	{
		short payloadLen_;
	};

	struct NetHeader
	{
		unsigned char code_;
		unsigned short payloadLen_;
		unsigned char randomKey_;
		unsigned char checkSum_;
	};
#pragma pack(pop)

	static inline unsigned char PACKET_CODE;
	static inline unsigned char FIXED_KEY;
	static constexpr int RINGBUFFER_SIZE = 10000;
	static constexpr int BUFFER_SIZE = (RINGBUFFER_SIZE / 8) + sizeof(NetHeader);

	template<ServerType type>
	void Clear(void)
	{
		bEncoded_ = false;
		if constexpr (type == Net)
		{
			front_ = rear_ = sizeof(NetHeader);
		}
		else
		{
			front_ = rear_ = sizeof(LanHeader);
		}
	}

	int GetData(char* pDest, int sizeToGet)
	{
		if (rear_ - front_ < sizeToGet)
		{
			return 0;
		}
		else
		{
			memcpy_s(pDest, sizeToGet, pBuffer_ + front_, sizeToGet);
			front_ += sizeToGet;
			return sizeToGet;
		}
	}

	char* GetPointer(int sizeToGet)
	{
		if (rear_ - front_ < sizeToGet)
		{
			return 0;
		}
		else
		{
			char* pRet = pBuffer_ + front_;
			front_ += sizeToGet;
			return pRet;
		}
	}

	int PutData(char* pSrc, int sizeToPut)
	{
		memcpy_s(pBuffer_ + rear_, sizeToPut, pSrc, sizeToPut);
		rear_ += sizeToPut;
		return sizeToPut;
	}

	int GetUsedDataSize(void)
	{
		return rear_ - front_;
	}

	template<ServerType st>
	char* GetPayloadStartPos(void)
	{
		if constexpr (st == Lan)
		{
			return pBuffer_ + sizeof(LanHeader);
		}
		else
		{
			return pBuffer_ + sizeof(NetHeader);
		}
	}

	int MoveWritePos(int sizeToWrite)
	{
		rear_ += sizeToWrite;
		return sizeToWrite;
	}

	int MoveReadPos(int sizeToRead)
	{
		front_ += sizeToRead;
		return sizeToRead;
	}


	Packet& operator <<(const unsigned char value)
	{
		*(unsigned char*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(unsigned char& value)
	{
		value = *(unsigned char*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const char value)
	{
		*(char*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(char& value)
	{
		value = *(char*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const short value)
	{
		*(short*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(short& value)
	{
		value = *(short*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const unsigned short value)
	{
		*(unsigned short*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(unsigned short& value)
	{
		value = *(unsigned short*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const int value)
	{
		*(int*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(int& value)
	{
		value = *(int*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const unsigned int value)
	{
		*(unsigned int*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(unsigned int& value)
	{
		value = *(unsigned int*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const long value)
	{
		*(long*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}

	Packet& operator >>(long& value)
	{
		value = *(long*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const unsigned long value)
	{
		*(unsigned long*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(unsigned long& value)
	{
		value = *(unsigned long*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const __int64 value)
	{
		*(__int64*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}

	Packet& operator >>(__int64& value)
	{
		value = *(__int64*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const unsigned __int64 value)
	{
		*(unsigned __int64*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(unsigned __int64& value)
	{
		value = *(unsigned __int64*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const float value)
	{
		*(float*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}
	Packet& operator >>(float& value)
	{
		value = *(float*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	Packet& operator <<(const double value)
	{
		*(double*)(pBuffer_ + rear_) = value;
		rear_ += sizeof(value);
		return *this;
	}

	Packet& operator >>(double& value)
	{
		value = *(double*)(pBuffer_ + front_);
		front_ += sizeof(value);
		return *this;
	}

	char* pBuffer_;
	int front_;
	int rear_;
	LONG refCnt_ = 0;
	bool bEncoded_;
public:
	unsigned short playerIdx_;
	RecvType recvType_;

#pragma warning(disable : 26495)
	Packet()
#ifdef DEBUG_LEAK
#ifndef DEBUG_LEAK_STD_LIST
		:pDebugLink{ offsetof(Packet,pDebugLink) },
		logList_{ offsetof(LEAK_LOG,logListLink) }
#endif
#endif
	{
		pBuffer_ = new char[BUFFER_SIZE];
#ifdef DEBUG_LEAK
#ifndef DEBUG_LEAK_STD_LIST
		InitializeCriticalSection(&cs_for_log_list_);
#endif
#endif
	}
#pragma warning(default : 26495)

	~Packet()
	{
		delete[] pBuffer_;
	}

	__forceinline LONG IncreaseRefCnt()
	{
		return InterlockedIncrement((LONG*)&refCnt_);
	}
	__forceinline LONG DecrementRefCnt()
	{
		return InterlockedDecrement((LONG*)&refCnt_);
	}

	static unsigned char GetCheckSum(unsigned char* const pPayload, const int payloadLen)
	{
		unsigned char sum = 0;
		for (int i = 0; i < payloadLen; ++i)
		{
			sum += pPayload[i];
		}
		return sum % 256;
	}

	void Encode(NetHeader* pHeader)
	{
		char d = pHeader->checkSum_;
		char p = d ^ (pHeader->randomKey_ + 1);
		char e = p ^ (FIXED_KEY + 1);
		pHeader->checkSum_ = e;

		char* payload = pBuffer_ + sizeof(NetHeader);
		for (int i = 0; i < pHeader->payloadLen_; i++)
		{
			d = payload[i];
			p = d ^ (p + pHeader->randomKey_ + 2 + i);
			e = p ^ (e + FIXED_KEY + 2 + i);
			payload[i] = e;
		}
	}

	void Decode(NetHeader* pHeader)
	{
		int len = pHeader->payloadLen_ + sizeof(NetHeader::checkSum_);
		unsigned char* const pDecodeTarget = (unsigned char*)pHeader + offsetof(NetHeader, checkSum_);

		for (int i = len - 1; i >= 1; --i)
		{
			pDecodeTarget[i] = (pDecodeTarget[i - 1] + FIXED_KEY + i + 1) ^ pDecodeTarget[i];
		}

		pDecodeTarget[0] ^= (FIXED_KEY + 1);

		for (int i = len - 1; i >= 1; --i)
		{
			pDecodeTarget[i] = (pDecodeTarget[i - 1] + pHeader->randomKey_ + i + 1) ^ pDecodeTarget[i];
		}

		pDecodeTarget[0] ^= (pHeader->randomKey_ + 1);
	}

	template<ServerType st>
	void SetHeader()
	{
		if constexpr (st == Net)
		{
			if (bEncoded_ == true)
				return;

			// ��� ����
			NetHeader* pHeader = (NetHeader*)pBuffer_;
			pHeader->code_ = PACKET_CODE;
			pHeader->payloadLen_ = rear_ - front_;
			pHeader->randomKey_ = rand();
			pHeader->checkSum_ = GetCheckSum((unsigned char*)pHeader + sizeof(NetHeader), pHeader->payloadLen_);

			Encode(pHeader);
			bEncoded_ = true;
		}
		else
		{
			((LanHeader*)pBuffer_)->payloadLen_ = rear_ - front_;
		}
	}

	// �ݼ��������� ȣ��
	bool ValidateReceived()
	{
		// �ƿ� ���������� ��Ŷ�� �������� ���� ����
		NetHeader* pHeader = (NetHeader*)pBuffer_;
		if (pHeader->code_ != PACKET_CODE)
			return false;

		Decode(pHeader);
		if (pHeader->checkSum_ != GetCheckSum((unsigned char*)pHeader + sizeof(NetHeader), pHeader->payloadLen_))
			return false;

		return true;
	}

	template<ServerType type>
	static Packet* Alloc()
	{
		Packet* pPacket = packetPool_.Alloc();
		pPacket->Clear<type>();
		return pPacket;
	}

	static void Free(Packet* pPacket)
	{
		packetPool_.Free(pPacket);
	}

	bool IsBufferEmpty()
	{
		return front_ == rear_;
	}

#ifdef DEBUG_LEAK
	enum LEAK_EVENT
	{
		ALLOC_CS_CHAT_REQ_MESSAGE,
		SEND_PACKET,
		SEND_ERROR,
		SEND_PENDING_DISCONNECTED,
		SMART_PACKET_DESTRUCTOR,
		RELEASE_SESSION,
		SEND_PROC,
		WRITE_SEND_PACKET_ARR,
	};
	struct LEAK_LOG
	{
		LEAK_EVENT event;
		int refCnt;
		LINKED_NODE logListLink = offsetof(LEAK_LOG,logListLink);
		Packet* pPacket;
		Session* pSession;
		ID sessionID;
		int sendBufIndex;
	};
	char funcName_[100];
	//// Start�Լ����� �ʱ�ȭ
	static inline CRITICAL_SECTION cs_for_debug_leak;
	CRITICAL_SECTION cs_for_log_list_;
	CLinkedList logList_;
	static inline CTlsObjectPool<LEAK_LOG, false> leakLogPool;
#ifdef DEBUG_LEAK_STD_LIST
	static inline std::list<Packet*> debugList;
#else
	LINKED_NODE pDebugLink;
	static inline CLinkedList debugList{ 200 };
#endif

	template<ServerType type>
	static Packet* AllocForDebugLeak(const char* pFuncName)
	{
		Packet* pPacket = packetPool_.Alloc();
		strcpy_s(pPacket->funcName_, _countof(pPacket->funcName_), pFuncName);
		pPacket->Clear<type>();
		EnterCriticalSection(&cs_for_debug_leak);
		debugList.push_back(pPacket);
		LeaveCriticalSection(&cs_for_debug_leak);
		return pPacket;
	}

	static void FreeForDebugLeak(Packet* pPacket)
	{
		EnterCriticalSection(&cs_for_debug_leak);
		debugList.remove(pPacket);
		LeaveCriticalSection(&cs_for_debug_leak);

		EnterCriticalSection(&pPacket->cs_for_log_list_);
		LEAK_LOG* pLeakLog = (LEAK_LOG*)pPacket->logList_.GetFirst();
		while (pLeakLog != nullptr)
		{
			LEAK_LOG* pTemp = pLeakLog;
			pLeakLog = (LEAK_LOG*)pPacket->logList_.remove(pLeakLog);
			Packet::leakLogPool.Free(pTemp);
		}
		LeaveCriticalSection(&pPacket->cs_for_log_list_);
		packetPool_.Free(pPacket);
	}

	LEAK_LOG* WRITE_PACKET_LOG(LEAK_EVENT event, int refCnt, Session* pSession, ID sessionID, int sendBufIdx = -1)
	{
		LEAK_LOG* pLeakLog = leakLogPool.Alloc();
		pLeakLog->event = event;
		pLeakLog->refCnt = refCnt;
		pLeakLog->pPacket = this;
		pLeakLog->pSession = pSession;
		pLeakLog->sessionID = sessionID;
		pLeakLog->sendBufIndex = sendBufIdx;
		EnterCriticalSection(&cs_for_log_list_);
		logList_.push_back(pLeakLog);
		LeaveCriticalSection(&cs_for_log_list_);
		return pLeakLog;
	}
#endif
	static inline CTlsObjectPool<Packet, false> packetPool_;
	friend class SmartPacket;
	friend void NetServer::RecvProc(Session* pSession, int numberOfBytesTransferred);
	friend BOOL NetServer::SendPost(Session* pSession);
	friend void NetServer::SendProc(Session* pSession, DWORD dwNumberOfBytesTransferred);
	friend void NetServer::ReleaseSession(Session* pSession);
};

class SmartPacket
{
public:
	Packet* pPacket_;
	SmartPacket(Packet*&& pPacket)
		:pPacket_{pPacket}
	{
		if (pPacket == nullptr)
			return;

		pPacket_->IncreaseRefCnt();
	}

	~SmartPacket()
	{
		if (pPacket_ == nullptr)
			return;

		if (pPacket_->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket_);
		}
	}

	__forceinline Packet& operator*()
	{
		return *pPacket_;
	}

	__forceinline Packet* operator->()
	{
		return pPacket_;
	}

	__forceinline Packet* GetPacket() 
	{
		return pPacket_;
	}
};

#undef QUEUE
