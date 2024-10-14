#ifndef _PROCESS_MONITOR
#define _PROCESS_MONITOR

#include <afxcmn.h>
#include <string>
#include <atomic>
#include <vector>
#include "src/ProcessList.h"
#include "src/FileHelp.h"
#include "DLLinject.h"

typedef struct _processInfoex
{
    DWORD m_ThreadNum;              //�߳���
    DWORD m_Conversation;           //�Ự
    DWORD m_PorcessID;              //�߳�ID
    HANDLE m_Phandle;               //���̾��
    std::vector<DWORD>* m_ThreadID; // �����ó�Ա��Ϊָ��
    std::string m_WinFram;          //���λ��
    std::string m_ExePath;          //���·��
    std::string m_PName;            //�������
    _processInfoex(std::vector<DWORD>* _ThreadID) : m_ThreadID(_ThreadID) {} // ���캯����Ϊ��ʼ��ָ��
} PROCESS_INFO, * PPROCESS_INFO;

class ProcessMonitor
{
public:
    ProcessMonitor(CListCtrl* listHandle) : m_ListHandle(listHandle) {
        this->m_PL = new ProcessList;
        this->m_File = new FileHelp;
        this->m_DLLinject = new DLLinject;
    }

    ~ProcessMonitor() {
        if (m_PL != nullptr)
            delete this->m_PL;
            delete this->m_File;
            delete this->m_DLLinject;
    }

    ProcessMonitor() {
        this->m_PL = new ProcessList;
        this->m_File = new FileHelp;
        this->m_DLLinject = new DLLinject;
    }

    void monitorProcesses();//����������Ϣ
    void monitorThread(DWORD _pid, THREAD_BASIC_INFORMATION& _te, PVOID& _threadadr);//�߳�������Ϣ
    void GetProceeAllThreadID(DWORD pid, std::vector<DWORD>& _threadID);//���ڽ��̵������߳�ID
    //�����߳̽ṹ��
    void SetThreadStruct(DWORD _pid, DWORD _dwThreadID, THREAD_BASIC_INFORMATION& _te, PVOID& _threadadr);
    //ǿɾ���ļ�
    bool DelFile(std::string _DirPath);
    //nt ע��dll
    BOOL inject_nt(DWORD _pid, LPCSTR _path,BOOL _isX64);
    //ע��dll
    BOOL inject(DWORD _pid, LPCSTR _path, BOOL _isX64);
    //����ע��
    BOOL reflectinject(DWORD _pid, LPCSTR _path);
    BOOL reflectinject(DWORD _pid, LPVOID _buf, size_t _size);
    //�ڴ�ע��
    BOOL meminject(DWORD _pid, LPCSTR _path);
    //NT�򿪽���
    HANDLE OpenProcessNt(DWORD _pid, DWORD flags = PROCESS_ALL_ACCESS);
private:
    CListCtrl* m_ListHandle;
    ProcessList* m_PL;
    FileHelp* m_File;
    DLLinject* m_DLLinject;
};

#endif // !_PROCESS_MONITOR
