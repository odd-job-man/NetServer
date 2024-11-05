#include "HMonitor.h"
#include <string>

HMonitor::HMonitor()
{
	_hProcess = GetCurrentProcess();
	//------------------------------------------------------------------
	// ���μ��� ������ Ȯ���Ѵ�.
	//
	// ���μ��� (exe) ����� ���� cpu ������ �����⸦ �Ͽ� ���� ������ ����.
	//------------------------------------------------------------------
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	_iNumberOfProcessors = SystemInfo.dwNumberOfProcessors;

	_fProcessorTotal = 0;
	_fProcessorUser = 0;
	_fProcessorKernel = 0;
	_fProcessTotal = 0;
	_fProcessUser = 0;
	_fProcessKernel = 0;
	_ftProcessor_LastKernel.QuadPart = 0;
	_ftProcessor_LastUser.QuadPart = 0;
	_ftProcessor_LastIdle.QuadPart = 0;
	_ftProcess_LastUser.QuadPart = 0;
	_ftProcess_LastKernel.QuadPart = 0;
	_ftProcess_LastTime.QuadPart = 0;

	WCHAR programPath[MAX_PATH];
	GetModuleFileName(NULL, programPath, MAX_PATH);

	std::wstring path(programPath);
	size_t pos = path.find_last_of(L"\\");
	std::wstring processName = path.substr(pos + 1);
	//.exe �����ϱ�
	size_t dotPos = processName.find_last_of(L".");
	processName.erase(dotPos, 4);

	//���� �����
	WCHAR PPBQueryStr[MAX_PATH];
	WCHAR PNPBQueryStr[MAX_PATH];

	PdhOpenQuery(NULL, NULL, &PPBQuery);
	if (ERROR_SUCCESS != PdhOpenQuery(NULL, NULL, &PNPBQuery))
		__debugbreak();
	PdhOpenQuery(NULL, NULL, &MABQuery);
	PdhOpenQuery(NULL, NULL, &NPBQuery);

	wsprintf(PPBQueryStr, L"\\Process(%s)\\Private Bytes", processName.c_str());
	PdhAddCounter(PPBQuery, PPBQueryStr, NULL, &PPBCounter);

	wsprintf(PNPBQueryStr,L"\\Process(%s)\\Pool Nonpaged Bytes" , processName.c_str());
	if (ERROR_SUCCESS != PdhAddCounter(PNPBQuery,PNPBQueryStr, NULL, &PNPBCounter))
		__debugbreak();
	if (ERROR_SUCCESS != PdhAddCounter(MABQuery, L"\\Memory\\Available MBytes", NULL, &MABCounter))
		__debugbreak();
	if (ERROR_SUCCESS != PdhAddCounter(NPBQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &NPBCounter))
		__debugbreak();


	UpdateCpuTime();
}

HMonitor::~HMonitor()
{
	PdhCloseQuery(PPBQuery);
	PdhCloseQuery(PNPBQuery);
	PdhCloseQuery(MABQuery);
	PdhCloseQuery(NPBQuery);
}

void HMonitor::UpdateCpuTime()
{
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//---------------------------------------------------------
	// �ý��� ��� �ð��� ���Ѵ�.
	//
	// ���̵� Ÿ�� / Ŀ�� ��� Ÿ�� (���̵�����) / ���� ��� Ÿ��
	//---------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
	{
		return;
	}

	// Ŀ�� Ÿ�ӿ��� ���̵� Ÿ���� ���Ե�.
	ULONGLONG KernelDiff = Kernel.QuadPart - _ftProcessor_LastKernel.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - _ftProcessor_LastUser.QuadPart;
	ULONGLONG IdleDiff = Idle.QuadPart - _ftProcessor_LastIdle.QuadPart;
	ULONGLONG Total = KernelDiff + UserDiff;
	ULONGLONG TimeDiff;

	if (Total == 0)
	{
		_fProcessorUser = 0.0f;
		_fProcessorKernel = 0.0f;
		_fProcessorTotal = 0.0f;
	}
	else
	{
		// Ŀ�� Ÿ�ӿ� ���̵� Ÿ���� �����Ƿ� ���� ���.
		_fProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		_fProcessorUser = (float)((double)UserDiff / Total * 100.0f);
		_fProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f);
	}
	_ftProcessor_LastKernel = Kernel;
	_ftProcessor_LastUser = User;
	_ftProcessor_LastIdle = Idle;

	//---------------------------------------------------------
	// ������ ���μ��� ������ �����Ѵ�.
	//---------------------------------------------------------
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;
	//---------------------------------------------------------
	// ������ 100 ���뼼���� ���� �ð��� ���Ѵ�. UTC �ð�.
	//
	// ���μ��� ���� �Ǵ��� ����
	//
	// a = ���ð����� �ý��� �ð��� ����. (�׳� ������ ������ �ð�)
	// b = ���μ����� CPU ��� �ð��� ����.
	//
	// a : 100 = b : ���� �������� ������ ����.
	//---------------------------------------------------------
	//---------------------------------------------------------
	// ���� �ð��� �������� 100 ���뼼���� �ð��� ����,
	//---------------------------------------------------------
	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);
	//---------------------------------------------------------
	// �ش� ���μ����� ����� �ð��� ����.
	//
	// �ι�°, ����°�� ����,���� �ð����� �̻��.
	//---------------------------------------------------------
	GetProcessTimes(_hProcess, (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&Kernel, (LPFILETIME)&User);
	//---------------------------------------------------------
	// ������ ����� ���μ��� �ð����� ���� ���ؼ� ������ ���� �ð��� �������� Ȯ��.
	//
	// �׸��� ���� ������ �ð����� ������ ������ ����.
	//---------------------------------------------------------
	TimeDiff = NowTime.QuadPart - _ftProcess_LastTime.QuadPart;
	UserDiff = User.QuadPart - _ftProcess_LastUser.QuadPart;
	KernelDiff = Kernel.QuadPart - _ftProcess_LastKernel.QuadPart;
	Total = KernelDiff + UserDiff;

	_fProcessTotal = (float)(Total / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_fProcessKernel = (float)(KernelDiff / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_fProcessUser = (float)(UserDiff / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_ftProcess_LastTime = NowTime;
	_ftProcess_LastKernel = Kernel;
	_ftProcess_LastUser = User;
}

double HMonitor::GetPPB()
{
	PdhCollectQueryData(PPBQuery);
	PDH_FMT_COUNTERVALUE ret;
	PdhGetFormattedCounterValue(PPBCounter, PDH_FMT_DOUBLE, NULL, &ret);
	return ret.doubleValue;
}

double HMonitor::GetPNPB()
{
	PdhCollectQueryData(PNPBQuery);
	PDH_FMT_COUNTERVALUE ret;
	PdhGetFormattedCounterValue(PNPBCounter, PDH_FMT_DOUBLE, NULL, &ret);
	return ret.doubleValue;
}

double HMonitor::GetAB()
{
	PdhCollectQueryData(MABQuery);
	PDH_FMT_COUNTERVALUE ret;
	PdhGetFormattedCounterValue(MABCounter, PDH_FMT_DOUBLE, NULL, &ret);
	return ret.doubleValue;
}

double HMonitor::GetNPB()
{
	PdhCollectQueryData(NPBQuery);
	PDH_FMT_COUNTERVALUE ret;
	PdhGetFormattedCounterValue(NPBCounter, PDH_FMT_DOUBLE, NULL, &ret);
	return ret.doubleValue;
}
