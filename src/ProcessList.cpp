#include "ProcessList.h"
#include <tchar.h>
#include <thread>
#include <string>
#include <locale>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
PSYSTEM_PROCESS_INFORMATION ProcessList::GetSystemInfo()
{
    NTSTATUS Status = 0;
    std::string Temperror;
    if (this->NtQuerySystemInformation == NULL)
        return nullptr;

    PSYSTEM_PROCESS_INFORMATION pInfo = nullptr;
    DWORD dwSize = 0;

    // ��ȡ��Ϣ����Ļ�������С
    Status = NtQuerySystemInformation(SystemProcessesAndThreadsInformation, nullptr, 0, &dwSize);

    if (!NT_SUCCESS(Status) && Status != STATUS_INFO_LENGTH_MISMATCH) {
        Temperror = "Failed to get the buffer size for process information, Status: ";
        Temperror += std::to_string(Status);
        OutputDebugString(Temperror.c_str());
        return nullptr;
    }

    // ���뻺����
    char* pBuff = new char[dwSize];
    if (pBuff == nullptr) {
        Temperror = "Memory allocation failed.";
        OutputDebugString(Temperror.c_str());
        return nullptr;
    }
    pInfo = (PSYSTEM_PROCESS_INFORMATION)pBuff;

    // ������Ի��� Ϊ�˱���ϵͳ����ˢ�²��ϸ�
    int retryCount = 2;
    while (retryCount--) {
        Status = NtQuerySystemInformation(SystemProcessesAndThreadsInformation, pInfo, dwSize, &dwSize);
        if (NT_SUCCESS(Status)) {
            break;
        }
        else {
            Temperror = "Failed to get process information, Status: ";
            Temperror += std::to_string(Status);
            OutputDebugString(Temperror.c_str());
            if (retryCount > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // ����ӳ�
            }
        }
    }

    if (!NT_SUCCESS(Status)) {
        delete[] pBuff;
        return nullptr;
    }

    return pInfo;
}
void ProcessList::EnumerateThreadsInModule(DWORD dwProcessId, std::vector<DWORD>& m_threadIds) //ö�������߳� �ж��߳��Ƿ����ڸý���
{
    // �����߳̿���
    HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnapshot == INVALID_HANDLE_VALUE)
        return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(THREADENTRY32);

    // �����߳̿���
    if (Thread32First(hThreadSnapshot, &te))
    {
        do
        {
            if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
            {
                // ����߳��Ƿ�����ָ������
                if (te.th32OwnerProcessID == dwProcessId)
                {
                    m_threadIds.push_back(te.th32ThreadID);
                }
            }
            te.dwSize = sizeof(THREADENTRY32);
        } while (Thread32Next(hThreadSnapshot, &te));
    } 
    CloseHandle(hThreadSnapshot);
}
void ProcessList::GetProcessThreadStruct(DWORD _pid,THREAD_BASIC_INFORMATION& _threadinfo, PVOID& _threadadr)
{
    EnumerateThreadsInModule(_pid, m_threadIds);
    for (DWORD dwThreadId : m_threadIds) //ȡ��������ֵ
    {
        if (dwThreadId == 0)
            continue;
        this->m_hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadId);
        // ��������������Ϊ�Ӳ���ϵͳ��ȡ��ANSI����ҳ
        setlocale(LC_ALL, ".ACP");
        if (ZwQueryInformationThread == NULL) {
            MessageBoxA(NULL, "�ڲ����� δ�ܻ�ȡ��������ַ", "Caption", MB_OK);
            return;
        }
        // ��ȡ�̵߳�������Ϣ
        status = ZwQueryInformationThread(
            this->m_hThread,
            ThreadBasicInformation,
            &this->m_threadBasicInfo,
            sizeof(this->m_threadBasicInfo),
            NULL
        );
        if (status != 0) {
            return;
        }
        status = ZwQueryInformationThread(
            this->m_hThread,                            // �߳̾��
            ThreadQuerySetWin32StartAddress,            // �߳���Ϣ���ͣ�ThreadQuerySetWin32StartAddress ���߳���ڵ�ַ
            &this->m_startaddr,                         // ָ�򻺳�����ָ��
            sizeof(this->m_startaddr),                  // �������Ĵ�С
            NULL
        );
        if (status != 0) {
            return;
        }
        CloseHandle(this->m_hThread);
    }
    _threadinfo = this->m_threadBasicInfo;
    _threadadr = m_startaddr;
}
void ProcessList::GetThreadInfo(DWORD _pid, DWORD _dwThreadID, THREAD_BASIC_INFORMATION& _threadinfo, PVOID& _threadadr)
{
    this->m_hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, _dwThreadID);
    // ��������������Ϊ�Ӳ���ϵͳ��ȡ��ANSI����ҳ
    setlocale(LC_ALL, ".ACP");
    if (ZwQueryInformationThread == NULL) {
        MessageBoxA(NULL, "�ڲ����� δ�ܻ�ȡ��������ַ", "Caption", MB_OK);
        return;
    }
    // ��ȡ�̵߳�������Ϣ
    status = ZwQueryInformationThread(
        this->m_hThread,
        ThreadBasicInformation,
        &this->m_threadBasicInfo,
        sizeof(this->m_threadBasicInfo),
        NULL
    );
    if (status != 0) {
        return;
    }
    status = ZwQueryInformationThread(
        this->m_hThread,                            // �߳̾��
        ThreadQuerySetWin32StartAddress,            // �߳���Ϣ���ͣ�ThreadQuerySetWin32StartAddress ���߳���ڵ�ַ
        &this->m_startaddr,                         // ָ�򻺳�����ָ��
        sizeof(this->m_startaddr),                  // �������Ĵ�С
        NULL
    );
    if (status != 0) {
        return;
    }
    CloseHandle(this->m_hThread);
    _threadinfo = this->m_threadBasicInfo;
    _threadadr = m_startaddr;
}