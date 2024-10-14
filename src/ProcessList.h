#pragma once
#ifndef _PROCESS_LIST
#define _PROCESS_LIST
#include <Windows.h>
#include <vector>

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define STATUS_SUCCESS              ((NTSTATUS) 0x00000000)
#define SystemProcessesAndThreadsInformation    5 // ���ܺ�
#define NTAPI    __stdcall

// �߳�״̬��ö�ٳ���
typedef enum _THREADINFOCLASS
{
    ThreadBasicInformation,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,
    ThreadIsIoPending,
    ThreadHideFromDebugger,
    ThreadBreakOnTermination,
    MaxThreadInfoClass
}THREADINFOCLASS;

// �̴߳��ڵȴ���ԭ���ö�ٳ���
typedef enum _KWAIT_REASON
{
    Executive,
    FreePage,
    PageIn,
    PoolAllocation,
    DelayExecution,
    Suspended,
    UserRequest,
    WrExecutive,
    WrFreePage,
    WrPageIn,
    WrPoolAllocation,
    WrDelayExecution,
    WrSuspended,
    WrUserRequest,
    WrEventPair,
    WrQueue,
    WrLpcReceive,
    WrLpcReply,
    WrVirtualMemory,
    WrPageOut,
    WrRendezvous,
    Spare2,
    Spare3,
    Spare4,
    Spare5,
    Spare6,
    WrKernel,
    MaximumWaitReason
}KWAIT_REASON;

typedef LONG   NTSTATUS;
typedef LONG   KPRIORITY;

typedef struct _CLIENT_ID
{
    HANDLE         UniqueProcess;
    HANDLE         UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _VM_COUNTERS
{
    SIZE_T        PeakVirtualSize;
    SIZE_T        VirtualSize;
    ULONG         PageFaultCount;
    SIZE_T        PeakWorkingSetSize;
    SIZE_T        WorkingSetSize;
    SIZE_T        QuotaPeakPagedPoolUsage;
    SIZE_T        QuotaPagedPoolUsage;
    SIZE_T        QuotaPeakNonPagedPoolUsage;
    SIZE_T        QuotaNonPagedPoolUsage;
    SIZE_T        PagefileUsage;
    SIZE_T        PeakPagefileUsage;
} VM_COUNTERS;

// �߳���Ϣ�ṹ��
typedef struct _THREAD_BASIC_INFORMATION
{
    LONG ExitStatus;              //�˳�״̬
    PVOID TebBaseAddress;         //Teb����ַ
    CLIENT_ID ClientId;           //�ͻ���ID
    KAFFINITY AffinityMask;       //��������
    KPRIORITY Priority;           //���ȼ�
    KPRIORITY BasePriority;       //�������ȼ�
}THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;

typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, * PUNICODE_STRING;


// ������Ϣ�ṹ��
typedef struct _SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;//����ͷ
    ULONG NumberOfThreads;//�����е��߳�����
    BYTE Reserved1[48];
    UNICODE_STRING ImageName;//���̵�ͼ������
    KPRIORITY BasePriority;//�����д������̵߳��������ȼ�
    HANDLE UniqueProcessId;//�������̵�Ψһ���� ID
    PVOID Reserved2;
    ULONG HandleCount;//��������ʹ�õľ������
    ULONG SessionId;//���̻Ự�ĻỰ��ʶ��
    PVOID Reserved3;
    SIZE_T PeakVirtualSize;//�����ڴ�ķ�ֵ��С�����ֽ�Ϊ��λ��
    SIZE_T VirtualSize;//�����ڴ�ĵ�ǰ��С�����ֽ�Ϊ��λ��
    ULONG Reserved4;
    SIZE_T PeakWorkingSetSize;//���̹������ķ�ֵ��С����ǧ�ֽ�Ϊ��λ��
    SIZE_T WorkingSetSize;
    PVOID Reserved5;
    SIZE_T QuotaPagedPoolUsage;//������ȡ�ķ�ҳ��ʹ�õĵ�ǰ���
    PVOID Reserved6;
    SIZE_T QuotaNonPagedPoolUsage;//������ȡ�ķǷ�ҳ��ʹ�õĵ�ǰ���
    SIZE_T PagefileUsage;//ҳ���ļ��洢���ֽ���
    SIZE_T PeakPagefileUsage;//ҳ���ļ��洢������ֽ���
    SIZE_T PrivatePageCount;//����ʹ�õ��ڴ�ҳ��
    LARGE_INTEGER Reserved7[6];
}SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;
// NtQuerySystemInformation ������ԭ��
// ���ڸ�û�е���,���Ե��Լ����庯����ԭ��
typedef DWORD(WINAPI* MyNtQuerySystemInformation)(UINT, PVOID, DWORD, PDWORD);


typedef NTSTATUS(WINAPI* MyZwQueryInformationThread)(
    HANDLE ThreadHandle,
    THREADINFOCLASS ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
    );

class ProcessList
{
	HMODULE m_ntAdr;
	ULONG m_bufferSize{};
	MyNtQuerySystemInformation NtQuerySystemInformation;
    MyZwQueryInformationThread ZwQueryInformationThread;
    PVOID m_startaddr;                    // ���������߳���ڵ�ַ
    NTSTATUS status;
    std::vector<DWORD> m_threadIds; //�߳�����
    HANDLE m_hThread;
    THREAD_BASIC_INFORMATION m_threadBasicInfo{};
public:
	ProcessList() {
        m_ntAdr = GetModuleHandle(TEXT("ntdll.dll"));
		this->NtQuerySystemInformation = reinterpret_cast<MyNtQuerySystemInformation>(GetProcAddress(this->m_ntAdr, "NtQuerySystemInformation"));
        this->ZwQueryInformationThread = reinterpret_cast<MyZwQueryInformationThread>(GetProcAddress(this->m_ntAdr, "ZwQueryInformationThread"));
    };
	~ProcessList() {};
    void EnumerateThreadsInModule(DWORD dwProcessId, std::vector<DWORD>& m_threadIds);
	PSYSTEM_PROCESS_INFORMATION GetSystemInfo();
    void GetProcessThreadStruct(DWORD _pid , THREAD_BASIC_INFORMATION& _threadinfo, PVOID& _threadadr);
    void GetThreadInfo(DWORD _pid, DWORD _dwThreadID, THREAD_BASIC_INFORMATION& _threadinfo, PVOID& _threadadr);
};
#endif // !_PROCESS_LIST