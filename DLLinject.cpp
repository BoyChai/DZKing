#include "DLLinject.h"
// ��ȡĿ����̵�ģ�����ַ
DWORD_PTR GetModuleBaseAddress(DWORD dwProcessID, const wchar_t* szModuleName) {
	DWORD_PTR dwModuleBaseAddress = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, dwProcessID);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		MODULEENTRY32W ModuleEntry32;
		ModuleEntry32.dwSize = sizeof(MODULEENTRY32W);
		if (Module32FirstW(hSnapshot, &ModuleEntry32)) {
			do {
				if (_wcsicmp(ModuleEntry32.szModule, szModuleName) == 0) {
					dwModuleBaseAddress = (DWORD_PTR)ModuleEntry32.modBaseAddr;
					break;
				}
			} while (Module32NextW(hSnapshot, &ModuleEntry32));
		}
		CloseHandle(hSnapshot);
	}
	return dwModuleBaseAddress;
}
// �ӵ������л�ȡ������ַ
DWORD_PTR GetFunctionAddressFromExportTable(HANDLE hProcess, DWORD_PTR dwModuleBase, const char* szFunctionName) {
    // ��ȡPEͷ
    IMAGE_DOS_HEADER dosHeader;
    SIZE_T bytesRead;


    if (!ReadProcessMemory(hProcess, (LPCVOID)dwModuleBase, &dosHeader, sizeof(dosHeader), &bytesRead)) {
        //std::cerr << "�޷���ȡPEͷ: " << GetLastError() << std::endl;
        return 0;
    }

    // ����Ƿ�Ϊ��Ч��MZͷ
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        //std::cerr << "��Ч��DOSͷ" << std::endl;
        return 0;
    }

    // ��ȡPEͷ
    IMAGE_NT_HEADERS32 ntHeaders;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(dwModuleBase + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), &bytesRead)) {
       // std::cerr << "�޷���ȡNTͷ: " << GetLastError() << std::endl;
        return 0;
    }

    // ����Ƿ�Ϊ��Ч��PEͷ
    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
        //std::cerr << "��Ч��PEͷ" << std::endl;
        return 0;
    }

    // ��ȡ�������ַ
    DWORD exportDirRVA = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (exportDirRVA == 0) {
        //std::cerr << "û�е�����" << std::endl;
        return 0;
    }

    // ��ȡ������
    IMAGE_EXPORT_DIRECTORY exportDirectory;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(dwModuleBase + exportDirRVA), &exportDirectory, sizeof(exportDirectory), &bytesRead)) {
        //std::cerr << "�޷���ȡ������: " << GetLastError() << std::endl;
        return 0;
    }

    // ��ȡ��������ͺ�����ַ��
    DWORD* nameRVAs = new DWORD[exportDirectory.NumberOfNames];
    if (!ReadProcessMemory(hProcess, (LPCVOID)(dwModuleBase + exportDirectory.AddressOfNames), nameRVAs, exportDirectory.NumberOfNames * sizeof(DWORD), &bytesRead)) {
        //std::cerr << "�޷���ȡ��������: " << GetLastError() << std::endl;
        delete[] nameRVAs;
        return 0;
    }

    WORD* nameOrdinals = new WORD[exportDirectory.NumberOfNames];
    if (!ReadProcessMemory(hProcess, (LPCVOID)(dwModuleBase + exportDirectory.AddressOfNameOrdinals), nameOrdinals, exportDirectory.NumberOfNames * sizeof(WORD), &bytesRead)) {
        //std::cerr << "�޷���ȡ��������ű�: " << GetLastError() << std::endl;
        delete[] nameRVAs;
        delete[] nameOrdinals;
        return 0;
    }

    DWORD* functionRVAs = new DWORD[exportDirectory.NumberOfFunctions];
    if (!ReadProcessMemory(hProcess, (LPCVOID)(dwModuleBase + exportDirectory.AddressOfFunctions), functionRVAs, exportDirectory.NumberOfFunctions * sizeof(DWORD), &bytesRead)) {
        //std::cerr << "�޷���ȡ������ַ��: " << GetLastError() << std::endl;
        delete[] nameRVAs;
        delete[] nameOrdinals;
        delete[] functionRVAs;
        return 0;
    }

    // ���Һ�����
    DWORD_PTR functionAddress = 0;
    for (DWORD i = 0; i < exportDirectory.NumberOfNames; ++i) {
        char functionName[256];
        if (!ReadProcessMemory(hProcess, (LPCVOID)(dwModuleBase + nameRVAs[i]), functionName, sizeof(functionName), &bytesRead)) {
            //std::cerr << "�޷���ȡ������: " << GetLastError() << std::endl;
            continue;
        }
        if (strcmp(functionName, szFunctionName) == 0) {
            // �ҵ�����������ȡ��Ӧ�ĵ�ַ
            WORD ordinal = nameOrdinals[i];
            functionAddress = dwModuleBase + functionRVAs[ordinal];
            break;
        }
    }

    // ����
    delete[] nameRVAs;
    delete[] nameOrdinals;
    delete[] functionRVAs;

    return functionAddress;
}

BOOL DLLinject::NTThreadInject(DWORD _pid, LPCSTR _DLLPath)
{
	PVOID pfnLoadLibraryW = NULL;
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;
	HMODULE hMod = NULL;
	LPVOID pRemoteBuf = NULL; //�洢dll·���ַ�������ʼ��ַ
	DWORD dwBufSize = strlen(_DLLPath) * sizeof(char); //DLL·���Ĵ�С

	LPTHREAD_START_ROUTINE pThreadProc; //�洢LoadLibrary������ַ
	//��ȡĿ����
	if (!(hProcess = this->LZwOpenProcess(_pid)))
	{
		MessageBox(NULL, std::to_string(GetLastError()).c_str(), "error num", MB_OK);
		MessageBox(NULL, "�򿪽���ʧ��", "Caption", MB_OK);
		return FALSE;
	}
	//��Ŀ�����exe�ڴ��з���szDllname��С���ڴ�
	pRemoteBuf = VirtualAllocEx(hProcess, NULL, dwBufSize, MEM_COMMIT, PAGE_READWRITE);

	//this->LNtWriteVirtualMemory<const CHAR>(_pid, pRemoteBuf, *_DLLPath, dwBufSize);	//dll write in memory
    WriteProcessMemory(hProcess, (LPVOID)pRemoteBuf, &_DLLPath, dwBufSize, NULL);

    MessageBox(NULL, std::to_string((uint64_t)pRemoteBuf).c_str(), "pRemoteBuf", MB_OK);
    MessageBox(NULL, _DLLPath, "Str", MB_OK);
    if (this->m_isX64)
    {
        hMod = GetModuleHandle("kernel32.dll");
        if (!hMod) {
			MessageBox(NULL, std::to_string(GetLastError()).c_str(), "error num", MB_OK);
			MessageBox(NULL, "��ȡkernel32ģ����ʧ��", "Caption", MB_OK);
			return FALSE;
        }
        MessageBox(NULL, std::to_string((uint64_t)hMod).c_str(), "hMod", MB_OK);
        pfnLoadLibraryW = GetProcAddress(hMod,"LoadLibraryA");
		if (!pfnLoadLibraryW) {
			MessageBox(NULL, std::to_string(GetLastError()).c_str(), "error num", MB_OK);
			MessageBox(NULL, "��ȡLoadLibraryA������ַʧ��", "Caption", MB_OK);
			return FALSE;
		}
        MessageBox(NULL, std::to_string((uint64_t)pfnLoadLibraryW).c_str(), "pfnLoadLibraryW", MB_OK);
        this->LZwCreatThread(hProcess, (LPTHREAD_START_ROUTINE)pfnLoadLibraryW, pRemoteBuf, &hThread);
        if (!hThread)
        {
            MessageBox(NULL, std::to_string(GetLastError()).c_str(), "error num", MB_OK);
            MessageBox(NULL, "�����߳�ʧ��", "Caption", MB_OK);
            return FALSE;
        }
        //WaitForSingleObject(hThread, 0xFFFFFFFF);
        //Close Handle
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return TRUE;
    }
    DWORD_PTR dwModuleBase = GetModuleBaseAddress(_pid, L"kernel32.dll");
    pfnLoadLibraryW = (PVOID)GetFunctionAddressFromExportTable(hProcess, dwModuleBase, "LoadLibraryA");
	this->LZwCreatThread(hProcess, (LPTHREAD_START_ROUTINE)pfnLoadLibraryW, pRemoteBuf, &hThread);
	//WaitForSingleObject(hThread, 0xFFFFFFFF);
	//Close Handle
	CloseHandle(hThread);
	CloseHandle(hProcess);
	return TRUE;
}

BOOL DLLinject::ThreadInject(DWORD _pid, LPCSTR _DLLPath)
{
    PVOID pfnLoadLibraryW = NULL;
    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;
    HMODULE hMod = NULL;
    LPVOID pRemoteBuf = NULL; //�洢dll·���ַ�������ʼ��ַ
    DWORD dwBufSize = strlen(_DLLPath) * sizeof(char); //DLL·���Ĵ�С

    LPTHREAD_START_ROUTINE pThreadProc; //�洢LoadLibrary������ַ
    //��ȡĿ����
    if (!(hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _pid)))
    {
        printf("OpenProcess(%d) failed!!![%d]\n", _pid, GetLastError());
        return FALSE;
    }
    //��Ŀ�����exe�ڴ��з���szDllname��С���ڴ�
    pRemoteBuf = VirtualAllocEx(hProcess, NULL, dwBufSize, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pRemoteBuf, &*_DLLPath, dwBufSize, NULL);	//dll write in memory

    if (this->m_isX64)
    {
        hMod = GetModuleHandle("kernel32.dll");
        if (hMod == NULL)
            return FALSE;
        pfnLoadLibraryW = GetProcAddress(hMod, "LoadLibraryA");
        CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pfnLoadLibraryW, pRemoteBuf, NULL, NULL);

        //WaitForSingleObject(hThread, 0xFFFFFFFF);
        //Close Handle
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return TRUE;
    }
    DWORD_PTR dwModuleBase = GetModuleBaseAddress(_pid, L"kernel32.dll");
    pfnLoadLibraryW = (PVOID)GetFunctionAddressFromExportTable(hProcess, dwModuleBase, "LoadLibraryA");
    CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pfnLoadLibraryW, pRemoteBuf, NULL, NULL);
    //WaitForSingleObject(hThread, 0xFFFFFFFF);
    //Close Handle
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return TRUE;
}

BOOL DLLinject::ReflectInject(DWORD _pid, LPCSTR _DLLPath)
{
    SupInject::ReflectInject mem;
    LPCSTR DllFilePath = _DLLPath;
    DWORD dwProcessId = _pid;
    HANDLE ProcessHandle = NULL;
    HANDLE hModule = NULL;
    HANDLE hFile = NULL;
    DWORD dwLength = 0;
    DWORD dwBytesRead = 0;
    LPVOID lpBuffer = NULL;
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES priv = { 0 };
    do
    {
        //dwProcessId = GetCurrentProcessId();
        hFile = CreateFileA(DllFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            printf("Open File Failed!\r\n");
            return 0;
        }

        dwLength = GetFileSize(hFile, NULL);
        if (dwLength == INVALID_FILE_SIZE || dwLength == 0)
        {
            printf("Get FileSize Failed\r\n");
            return 0;
        }

        lpBuffer = HeapAlloc(GetProcessHeap(), 0, dwLength);
        if (!lpBuffer)
        {
            printf("Alloc Buffer Failed!\r\n");
        }

        if (ReadFile(hFile, lpBuffer, dwLength, &dwBytesRead, NULL) == FALSE)
        {
            printf("Failed to alloc a buffer!");
        }

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            priv.PrivilegeCount = 1;
            priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
            {
                AdjustTokenPrivileges(hToken, FALSE, &priv, 0, NULL, NULL);
            }
            CloseHandle(hToken);
        }

        //ProcessHandle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        //    PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwProcessId);
        ProcessHandle = LZwOpenProcess(dwProcessId, PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ);
        if (!ProcessHandle)
        {
            printf("Open Target Process Failed\r\n");
            return 0;
        }
        //�Լ�ʵ��
        hModule = mem.LoadRemoteLibraryR(ProcessHandle, lpBuffer, dwLength, NULL);
        if (!hModule)
        {
            printf("Failed to inject the DLL");
            break;
        }

        printf("Injected the '%s' DLL into process %d.\r\n", DllFilePath, dwProcessId);
        WaitForSingleObject(hModule, -1);
    } while (0);

    if (lpBuffer)
        HeapFree(GetProcessHeap(), 0, lpBuffer);
    if (ProcessHandle)
        CloseHandle(ProcessHandle);
    if (hFile)
        CloseHandle(hFile);
    return 0;
}

BOOL DLLinject::ReflectInject(DWORD _pid, LPVOID _lpBuffer,size_t _bufsize)
{
    SupInject::ReflectInject mem;
    DWORD dwProcessId = _pid;
    HANDLE ProcessHandle = NULL;
    HANDLE hModule = NULL;
    LPVOID lpBuffer = NULL;
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES priv = { 0 };
    do
    {
        lpBuffer = HeapAlloc(GetProcessHeap(), 0, _bufsize);
        if (!lpBuffer)
        {
            printf("Alloc Buffer Failed!\r\n");
        }
        memcpy(lpBuffer, _lpBuffer, _bufsize);
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            priv.PrivilegeCount = 1;
            priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
            {
                AdjustTokenPrivileges(hToken, FALSE, &priv, 0, NULL, NULL);
            }
            CloseHandle(hToken);
        }

        //ProcessHandle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        //    PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwProcessId);
        ProcessHandle = LZwOpenProcess(dwProcessId, PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ);
        if (!ProcessHandle)
        {
            printf("Open Target Process Failed\r\n");
            return 0;
        }
        //�Լ�ʵ��
        hModule = mem.LoadRemoteLibraryR(ProcessHandle, lpBuffer, _bufsize, NULL);
        if (!hModule)
        {
            printf("Failed to inject the DLL");
            break;
        }
        WaitForSingleObject(hModule, -1);
    } while (0);

    if (lpBuffer)
        HeapFree(GetProcessHeap(), 0, lpBuffer);
    if (ProcessHandle)
        CloseHandle(ProcessHandle);
    return 0;
}
bool ReadFileToMemory(const char* filePath, uint8_t** buffer, size_t* size) {
    // 1. ���ļ�
    HANDLE hFile = CreateFile(
        filePath,                  // �ļ�·��
        GENERIC_READ,              // ������Ȩ��
        FILE_SHARE_READ,           // �����������̶�ȡ
        NULL,                      // Ĭ�ϰ�ȫ����
        OPEN_EXISTING,             // �����Ѵ��ڵ��ļ�
        FILE_ATTRIBUTE_NORMAL,     // ��ͨ�ļ�����
        NULL);                     // ������ģ���ļ�

    if (hFile == INVALID_HANDLE_VALUE) {
        //std::cerr << "Failed to open file: " << filePath << " with error: " << GetLastError() << std::endl;
        return false;
    }

    // 2. ��ȡ�ļ���С
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        //std::cerr << "Failed to get file size with error: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }

    // ȷ���ļ���С���ᳬ�� `size_t` �ķ�Χ
    if (fileSize.QuadPart > SIZE_MAX) {
        //std::cerr << "File is too large to fit in memory." << std::endl;
        CloseHandle(hFile);
        return false;
    }

    // ������������ļ���С
    *size = static_cast<size_t>(fileSize.QuadPart);

    // 3. �����ڴ滺�������洢�ļ�����
    *buffer = new uint8_t[*size];
    if (*buffer == nullptr) {
        //std::cerr << "Memory allocation failed for file buffer." << std::endl;
        CloseHandle(hFile);
        return false;
    }

    // 4. ��ȡ�ļ����ݵ��ڴ滺����
    DWORD bytesRead;
    if (!ReadFile(hFile, *buffer, static_cast<DWORD>(*size), &bytesRead, NULL)) {
        //std::cerr << "Failed to read file with error: " << GetLastError() << std::endl;
        delete[] * buffer;
        *buffer = nullptr;
        CloseHandle(hFile);
        return false;
    }

    // ȷ����ȡ���ֽ�����Ԥ�ڵĴ�С��ͬ
    if (bytesRead != *size) {
        //std::cerr << "Read size mismatch: expected " << *size << " bytes, but read " << bytesRead << " bytes." << std::endl;
        delete[] * buffer;
        *buffer = nullptr;
        CloseHandle(hFile);
        return false;
    }

    // 5. �ر��ļ����
    CloseHandle(hFile);

    return true;
}

BOOL DLLinject::MemInject(DWORD _pid, LPCSTR _DLLPath) {
    while (*SupInject::MemInject::Memx != 0xCCCC) //��麯������ x86 �� x64 �ǲ�ͬ��
    {
        SupInject::MemInject::Memx++;
        SupInject::MemInject::size += 2;
    }
    uint8_t* FileAdr;
    size_t fileSize, ShellSize = 0;
    ReadFileToMemory(_DLLPath, &FileAdr, &fileSize);
    VOID* DllBuffer = malloc(fileSize);//�����ڴ���������DLL
    ZeroMemory(DllBuffer, fileSize);
    memcpy(DllBuffer, FileAdr, fileSize); //����DLL�ڴ浽�հ׵�ַ

    //��ȡ������shell
    ShellSize = SupInject::MemInject::size;
    VOID* ShellBuffer = malloc(ShellSize);//�����ڴ��������溯��
    ZeroMemory(ShellBuffer, ShellSize);
    memcpy(ShellBuffer, SupInject::MemInject::MemLoadLibrary, ShellSize);//�����������ݵ��հ׵�ַ

    HMODULE NTDLL = GetModuleHandleA("ntdll");//��ȡntDLL���
    SupInject::MemInject::PARAMX param;
    RtlZeroMemory(&param, sizeof(SupInject::MemInject::PARAMX));
    param.lpFileData = DllBuffer;
    param.DataLength = fileSize;
    param.LdrGetProcedureAddress = (SupInject::MemInject::LdrGetProcedureAddressT)GetProcAddress(NTDLL, "LdrGetProcedureAddress");;
    param.dwNtAllocateVirtualMemory = (SupInject::MemInject::NtAllocateVirtualMemoryT)GetProcAddress(NTDLL, "NtAllocateVirtualMemory");
    param.pLdrLoadDll = (SupInject::MemInject::LdrLoadDllT)GetProcAddress(NTDLL, "LdrLoadDll");
    param.RtlInitAnsiString = (SupInject::MemInject::RtlInitAnsiStringT)GetProcAddress(NTDLL, "RtlInitAnsiString");
    param.RtlAnsiStringToUnicodeString = (SupInject::MemInject::RtlAnsiStringToUnicodeStringT)GetProcAddress(NTDLL, "RtlAnsiStringToUnicodeString");
    param.RtlFreeUnicodeString = (SupInject::MemInject::RtlFreeUnicodeStringT)GetProcAddress(NTDLL, "RtlFreeUnicodeString");

    //��ʼԶ��ע��
    HANDLE hProcess = LZwOpenProcess(_pid);//Ŀ����̾��

    //�����ڴ�,��shellcode��DLL����,�Ͳ������Ƶ�Ŀ����� ��ȫ��� ��С���0x100
    PBYTE  pAddress = (PBYTE)VirtualAllocEx(hProcess, 0, fileSize + ShellSize + sizeof(SupInject::MemInject::PARAMX) + 0x100, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    param.lpFileData = pAddress;//�޳���DLL���ݵĵ�ַ


    LNtWriteVirtualMemory(_pid, pAddress, (LPCVOID)DllBuffer, fileSize); //dll data
    LNtWriteVirtualMemory(_pid, pAddress + fileSize, (LPCVOID)ShellBuffer, ShellSize);//shell code data
    LNtWriteVirtualMemory(_pid, pAddress + fileSize + ShellSize, (LPCVOID)&param, sizeof(SupInject::MemInject::PARAMX));//param data 

    HANDLE hThread;
    //��ʼִ��
    LZwCreatThread(hProcess, (LPTHREAD_START_ROUTINE)(pAddress + fileSize), pAddress + fileSize + ShellSize, &hThread);
    if (hThread) //�����Ĳ���
    {
        DWORD dExecCode = 0;
        WaitForSingleObject(hThread, INFINITE);
        GetExitCodeThread(hThread, &dExecCode);
        VirtualFreeEx(hProcess, pAddress, 0, MEM_FREE);
        CloseHandle(hThread);
        CloseHandle(hProcess);
    }
    free(DllBuffer);
    free(ShellBuffer);
    return true;
}

FARPROC WINAPI SupInject::ReflectInject::GetProcAddressR(HANDLE hModule, LPCSTR lpProcName)
{
    UINT_PTR uiLibraryAddress = 0;
    FARPROC fpResult = NULL;

    if (hModule == NULL)
    {
        return NULL;
    }

    uiLibraryAddress = (UINT_PTR)hModule;

    __try
    {
        UINT_PTR uiAddressArray = 0;
        UINT_PTR uiNameArray = 0;
        UINT_PTR uiNameOrdinals = 0;
        PIMAGE_NT_HEADERS pNtHeaders = NULL;
        PIMAGE_DATA_DIRECTORY pDataDirectory = NULL;
        PIMAGE_EXPORT_DIRECTORY pExportDirectory = NULL;

        // get the VA of the modules NT Header
        pNtHeaders = (PIMAGE_NT_HEADERS)(uiLibraryAddress + ((PIMAGE_DOS_HEADER)uiLibraryAddress)->e_lfanew);

        pDataDirectory = (PIMAGE_DATA_DIRECTORY)&pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

        // get the VA of the export directory
        pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(uiLibraryAddress + pDataDirectory->VirtualAddress);

        // get the VA for the array of addresses
        uiAddressArray = (uiLibraryAddress + pExportDirectory->AddressOfFunctions);

        // get the VA for the array of name pointers
        uiNameArray = (uiLibraryAddress + pExportDirectory->AddressOfNames);

        // get the VA for the array of name ordinals
        uiNameOrdinals = (uiLibraryAddress + pExportDirectory->AddressOfNameOrdinals);

        // test if we are importing by name or by ordinal...
        if (((DWORD)lpProcName & 0xFFFF0000) == 0x00000000)
        {
            // import by ordinal...

            // use the import ordinal (- export ordinal base) as an index into the array of addresses
            uiAddressArray += ((IMAGE_ORDINAL((DWORD)lpProcName) - pExportDirectory->Base) * sizeof(DWORD));

            // resolve the address for this imported function
            fpResult = (FARPROC)(uiLibraryAddress + DEREF_32(uiAddressArray));
        }
        else
        {
            // import by name...
            DWORD dwCounter = pExportDirectory->NumberOfNames;
            while (dwCounter--)
            {
                char* cpExportedFunctionName = (char*)(uiLibraryAddress + DEREF_32(uiNameArray));

                // test if we have a match...
                if (strcmp(cpExportedFunctionName, lpProcName) == 0)
                {
                    // use the functions name ordinal as an index into the array of name pointers
                    uiAddressArray += (DEREF_16(uiNameOrdinals) * sizeof(DWORD));

                    // calculate the virtual address for the function
                    fpResult = (FARPROC)(uiLibraryAddress + DEREF_32(uiAddressArray));

                    // finish...
                    break;
                }

                // get the next exported function name
                uiNameArray += sizeof(DWORD);

                // get the next exported function name ordinal
                uiNameOrdinals += sizeof(WORD);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        fpResult = NULL;
    }

    return fpResult;
}

HANDLE SupInject::ReflectInject::LoadRemoteLibraryR(HANDLE hProcess, LPVOID lpBuffer, DWORD dwLength, LPVOID lpParameter)
{
    BOOL bSuccess = FALSE;
    LPVOID lpRemoteLibraryBuffer = NULL;
    LPTHREAD_START_ROUTINE lpReflectiveLoader = NULL;
    HANDLE hThread = NULL;
    DWORD dwReflectiveLoaderOffset = 0;
    DWORD dwThreadId = 0;

    __try
    {
        do
        {
            if (!hProcess || !lpBuffer || !dwLength)
            {
                break;
            }

            // �õ����ض�̬�⺯����dll�ļ��е�ƫ��
            dwReflectiveLoaderOffset = GetLoaderOffset(lpBuffer);
            if (!dwReflectiveLoaderOffset)
            {
                break;
            }


            // alloc memory (RWX) in the host process for the image...
            lpRemoteLibraryBuffer = VirtualAllocEx(hProcess, NULL, dwLength, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (!lpRemoteLibraryBuffer)
            {
                break;
            }

            // write the image into the host process...
            if (!WriteProcessMemory(hProcess, lpRemoteLibraryBuffer, lpBuffer, dwLength, NULL))
            {
                break;
            }

            // add the offset to ReflectiveLoader() to the remote library address...
            lpReflectiveLoader = (LPTHREAD_START_ROUTINE)((ULONG_PTR)lpRemoteLibraryBuffer + dwReflectiveLoaderOffset);
            //����Զ���̼߳��ض�̬�⣬�����ض�̬�⺯�� ִ��
            // create a remote thread in the host process to call the ReflectiveLoader!
            hThread = CreateRemoteThread(hProcess, NULL, 1024 * 1024, lpReflectiveLoader, lpParameter, (DWORD)NULL, &dwThreadId);

        } while (0);

    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        hThread = NULL;
    }
    return hThread;
}

HMODULE SupInject::ReflectInject::LoadLibraryR(LPVOID lpBuffer, DWORD dwLength)
{
    HMODULE hResult = NULL;
    DWORD  dwLoaderOffset = 0;
    REFLECTIVELOADER  dwReflectLoader = NULL;

    DWORD dwOldProtect1 = 0;
    DWORD dwOldProtect2 = 0;

    DLLMAIN pDllMain = NULL;

    if (lpBuffer == NULL || dwLength == 0)
    {
        return 0;
    }

    __try
    {
        dwLoaderOffset = GetLoaderOffset(lpBuffer);
        if (dwLoaderOffset != 0)
        {
            dwReflectLoader = (REFLECTIVELOADER)((UINT_PTR)lpBuffer + dwLoaderOffset);

            if (VirtualProtect(lpBuffer, dwLength, PAGE_EXECUTE_READWRITE, &dwOldProtect1))
            {
                pDllMain = (DLLMAIN)dwReflectLoader();

                if (pDllMain != NULL)
                {
                    if (pDllMain != NULL)
                    {
                        // call the loaded librarys DllMain to get its HMODULE
                        if (!pDllMain(NULL, DLL_QUERY_HMODULE, &hResult))
                        {
                            hResult = NULL;
                        }
                    }
                    VirtualProtect(lpBuffer, dwLength, dwOldProtect1, &dwOldProtect2);
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        hResult = NULL;
    }
    return hResult;
}

DWORD SupInject::ReflectInject::GetLoaderOffset(VOID* lpReflectiveDllBuffer) {
    UINT_PTR uiBaseAddress = 0;
    UINT_PTR uiBaseAddressOfNtHeader = 0;
    UINT_PTR uiDictArray = 0;
    UINT_PTR uiExportFOA = 0;
    UINT_PTR uiNameArray = 0;
    UINT_PTR uiAddressArray = 0;
    UINT_PTR uiNameOrdinals = 0;
    DWORD iCounter = 0;
#ifdef _WIN64
    DWORD dwCompiledArch = 2;
#else
    DWORD dwCompiledArch = 1;
#endif

    uiBaseAddress = (UINT_PTR)lpReflectiveDllBuffer;
    /*Get NT Header Base*/
    uiBaseAddressOfNtHeader = uiBaseAddress + ((PIMAGE_DOS_HEADER)uiBaseAddress)->e_lfanew;

    if (((PIMAGE_NT_HEADERS)uiBaseAddressOfNtHeader)->OptionalHeader.Magic == 0x10B)
    {
        if (dwCompiledArch != 1)
        {
            return 0;
        }
    }
    else if (((PIMAGE_NT_HEADERS)uiBaseAddressOfNtHeader)->OptionalHeader.Magic == 0x020B) // PE64
    {
        if (dwCompiledArch != 2)
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }

    uiDictArray = (UINT_PTR) & ((PIMAGE_NT_HEADERS)uiBaseAddressOfNtHeader)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    //���������ļ��е�ƫ��
    uiExportFOA = uiBaseAddress + RvaToOffset(((PIMAGE_DATA_DIRECTORY)uiDictArray)->VirtualAddress, uiBaseAddress);

    // get the File Offset for the array of name pointers
    uiNameArray = uiBaseAddress + RvaToOffset(((PIMAGE_EXPORT_DIRECTORY)uiExportFOA)->AddressOfNames, uiBaseAddress);

    // get the File Offset for the array of addresses
    uiAddressArray = uiBaseAddress + RvaToOffset(((PIMAGE_EXPORT_DIRECTORY)uiExportFOA)->AddressOfFunctions, uiBaseAddress);

    // get the File Offset for the array of name ordinals
    uiNameOrdinals = uiBaseAddress + RvaToOffset(((PIMAGE_EXPORT_DIRECTORY)uiExportFOA)->AddressOfNameOrdinals, uiBaseAddress);

    // get a counter for the number of exported functions...
    iCounter = ((PIMAGE_EXPORT_DIRECTORY)uiExportFOA)->NumberOfNames;

    while (iCounter--)
    {
        char* cpExportFunctionName = (char*)(uiBaseAddress + RvaToOffset(DEREF_32(uiNameArray), uiBaseAddress));

        if (strstr(cpExportFunctionName, "ReflectiveLoader") != NULL)
        {
            // get the File Offset for the array of addresses
            uiAddressArray = uiBaseAddress + RvaToOffset(((PIMAGE_EXPORT_DIRECTORY)uiExportFOA)->AddressOfFunctions, uiBaseAddress);

            // use the functions name ordinal as an index into the array of name pointers
            uiAddressArray += (DEREF_16(uiNameOrdinals) * sizeof(DWORD));

            // return the File Offset to the ReflectiveLoader() functions code...
            return RvaToOffset(DEREF_32(uiAddressArray), uiBaseAddress);
        }
        // get the next exported function name
        uiNameArray += sizeof(DWORD);

        // get the next exported function name ordinal
        uiNameOrdinals += sizeof(WORD);
    }
    return 0;
}
//��RVA��FOA
DWORD SupInject::ReflectInject::RvaToOffset(DWORD dwRva, UINT_PTR uiBaseAddress)
{
    WORD wIndex = 0;
    PIMAGE_SECTION_HEADER pSectionHeaderVA = NULL;
    PIMAGE_NT_HEADERS pNtHeadersVA = NULL;

    pNtHeadersVA = (PIMAGE_NT_HEADERS)(uiBaseAddress + ((PIMAGE_DOS_HEADER)uiBaseAddress)->e_lfanew);
    pSectionHeaderVA = (PIMAGE_SECTION_HEADER)((UINT_PTR)(&pNtHeadersVA->OptionalHeader) + pNtHeadersVA->FileHeader.SizeOfOptionalHeader);
    //������ʼ�������ļ��е�ƫ��
    if (dwRva < pSectionHeaderVA[0].PointerToRawData)
    {
        return dwRva;
    }
    for (wIndex = 0; wIndex < pNtHeadersVA->FileHeader.NumberOfSections; wIndex++)
    {
        if (dwRva >= pSectionHeaderVA[wIndex].VirtualAddress && dwRva < (pSectionHeaderVA[wIndex].VirtualAddress + pSectionHeaderVA[wIndex].SizeOfRawData))
        {
            return (dwRva - pSectionHeaderVA[wIndex].VirtualAddress + pSectionHeaderVA[wIndex].PointerToRawData);
        }
    }
}

SupInject::MemInject::DWORDX __stdcall SupInject::MemInject::MemLoadLibrary(PARAMX* X)
{
    LPCVOID lpFileData = X->lpFileData;
    DWORDX DataLength = X->DataLength;

    /****************��ʼ�����ú���********************/
    LdrGetProcedureAddressT LdrGetProcedureAddress = (X->LdrGetProcedureAddress);

    NtAllocateVirtualMemoryT pNtAllocateVirtualMemory = (X->dwNtAllocateVirtualMemory);
    LdrLoadDllT pLdrLoadDll = (X->pLdrLoadDll);
    RtlInitAnsiStringT RtlInitAnsiString = X->RtlInitAnsiString;
    RtlAnsiStringToUnicodeStringT RtlAnsiStringToUnicodeString = X->RtlAnsiStringToUnicodeString;
    RtlFreeUnicodeStringT RtlFreeUnicodeString = X->RtlFreeUnicodeString;

    ProcDllMain pDllMain = NULL;
    void* pMemoryAddress = NULL;

    ANSI_STRING ansiStr;
    UNICODE_STRING UnicodeString;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNTHeader;
    PIMAGE_SECTION_HEADER pSectionHeader;
    int ImageSize = 0;

    int nAlign = 0;
    int i = 0;


    //���������Ч�ԣ�����ʼ��
    /*********************CheckDataValide**************************************/
    //	PIMAGE_DOS_HEADER pDosHeader;
    //��鳤��
    if (DataLength > sizeof(IMAGE_DOS_HEADER))
    {
        pDosHeader = (PIMAGE_DOS_HEADER)lpFileData; // DOSͷ
        //���dosͷ�ı��
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) goto CODEEXIT; //0��5A4D : MZ

        //��鳤��
        if ((DWORDX)DataLength < (pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS))) goto CODEEXIT;
        //ȡ��peͷ
        pNTHeader = (PIMAGE_NT_HEADERS)((DWORDX)lpFileData + pDosHeader->e_lfanew); // PEͷ
        //���peͷ�ĺϷ���
        if (pNTHeader->Signature != IMAGE_NT_SIGNATURE) goto CODEEXIT; //0��00004550: PE00
        if ((pNTHeader->FileHeader.Characteristics & IMAGE_FILE_DLL) == 0) //0��2000: File is a DLL
            goto CODEEXIT;
        if ((pNTHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) //0��0002: ָ���ļ���������
            goto CODEEXIT;
        if (pNTHeader->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER))
            goto CODEEXIT;


        //ȡ�ýڱ��α�
        pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORDX)pNTHeader + sizeof(IMAGE_NT_HEADERS));
        //��֤ÿ���ڱ�Ŀռ�
        for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
        {
            if ((pSectionHeader[i].PointerToRawData + pSectionHeader[i].SizeOfRawData) > (DWORD)DataLength) goto CODEEXIT;
        }

        /**********************************************************************/
        nAlign = pNTHeader->OptionalHeader.SectionAlignment; //�ζ����ֽ���

        //ImageSize = pNTHeader->OptionalHeader.SizeOfImage;
        //// ��������ͷ�ĳߴ硣����dos, coff, peͷ �� �α�Ĵ�С
        ImageSize = (pNTHeader->OptionalHeader.SizeOfHeaders + nAlign - 1) / nAlign * nAlign;
        // �������нڵĴ�С
        for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i)
        {
            //�õ��ýڵĴ�С
            int CodeSize = pSectionHeader[i].Misc.VirtualSize;
            int LoadSize = pSectionHeader[i].SizeOfRawData;
            int MaxSize = (LoadSize > CodeSize) ? (LoadSize) : (CodeSize);

            int SectionSize = (pSectionHeader[i].VirtualAddress + MaxSize + nAlign - 1) / nAlign * nAlign;
            if (ImageSize < SectionSize)
                ImageSize = SectionSize; //Use the Max;
        }
        if (ImageSize == 0) goto CODEEXIT;

        // ���������ڴ�
        SIZE_T uSize = ImageSize;
        pNtAllocateVirtualMemory((HANDLE)-1, &pMemoryAddress, 0, &uSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

        if (pMemoryAddress != NULL)
        {

            // ������Ҫ���Ƶ�PEͷ+�α��ֽ���
            int HeaderSize = pNTHeader->OptionalHeader.SizeOfHeaders;
            int SectionSize = pNTHeader->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
            int MoveSize = HeaderSize + SectionSize;
            //����ͷ�Ͷ���Ϣ
            for (i = 0; i < MoveSize; i++)
            {
                *((PCHAR)pMemoryAddress + i) = *((PCHAR)lpFileData + i);
            }
            //memmove(pMemoryAddress, lpFileData, MoveSize);//Ϊ������һ��API,ֱ��������ĵ��ֽڸ������ݾ�����

            //����ÿ����
            for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i)
            {
                if (pSectionHeader[i].VirtualAddress == 0 || pSectionHeader[i].SizeOfRawData == 0)continue;
                // ��λ�ý����ڴ��е�λ��
                void* pSectionAddress = (void*)((DWORDX)pMemoryAddress + pSectionHeader[i].VirtualAddress);
                // ���ƶ����ݵ������ڴ�
            //	memmove((void *)pSectionAddress,(void *)((DWORDX)lpFileData + pSectionHeader[i].PointerToRawData),	pSectionHeader[i].SizeOfRawData);
                //Ϊ������һ��API,ֱ��������ĵ��ֽڸ������ݾ�����
                for (size_t k = 0; k < pSectionHeader[i].SizeOfRawData; k++)
                {
                    *((PCHAR)pSectionAddress + k) = *((PCHAR)lpFileData + pSectionHeader[i].PointerToRawData + k);
                }
            }
            /*******************�ض�λ��Ϣ****************************************************/

            if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress > 0
                && pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0)
            {

                DWORDX Delta = (DWORDX)pMemoryAddress - pNTHeader->OptionalHeader.ImageBase;
                DWORDX* pAddress;
                //ע���ض�λ���λ�ÿ��ܺ�Ӳ���ļ��е�ƫ�Ƶ�ַ��ͬ��Ӧ��ʹ�ü��غ�ĵ�ַ
                PIMAGE_BASE_RELOCATION pLoc = (PIMAGE_BASE_RELOCATION)((DWORDX)pMemoryAddress
                    + pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
                while ((pLoc->VirtualAddress + pLoc->SizeOfBlock) != 0) //��ʼɨ���ض�λ��
                {
                    WORD* pLocData = (WORD*)((DWORDX)pLoc + sizeof(IMAGE_BASE_RELOCATION));
                    //���㱾����Ҫ�������ض�λ���ַ������Ŀ
                    int NumberOfReloc = (pLoc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                    for (i = 0; i < NumberOfReloc; i++)
                    {
                        if ((DWORDX)(pLocData[i] & 0xF000) == 0x00003000 || (DWORDX)(pLocData[i] & 0xF000) == 0x0000A000) //����һ����Ҫ�����ĵ�ַ
                        {
                            // ������
                            // pLoc->VirtualAddress = 0��1000;
                            // pLocData[i] = 0��313E; ��ʾ����ƫ�Ƶ�ַ0��13E����Ҫ����
                            // ��� pAddress = ����ַ + 0��113E
                            // ����������� A1 ( 0c d4 02 10) �������ǣ� mov eax , [1002d40c]
                            // ��Ҫ����1002d40c�����ַ
                            pAddress = (DWORDX*)((DWORDX)pMemoryAddress + pLoc->VirtualAddress + (pLocData[i] & 0x0FFF));
                            *pAddress += Delta;
                        }
                    }
                    //ת�Ƶ���һ���ڽ��д���
                    pLoc = (PIMAGE_BASE_RELOCATION)((DWORDX)pLoc + pLoc->SizeOfBlock);
                }
                /***********************************************************************/
            }

            /******************* ���������ַ��**************/
            DWORDX Offset = pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
            if (Offset == 0)
                goto CODEEXIT; //No Import Table

            PIMAGE_IMPORT_DESCRIPTOR pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORDX)pMemoryAddress + Offset);

            PIMAGE_IMPORT_BY_NAME pByName = NULL;
            while (pID->Characteristics != 0)
            {
                PIMAGE_THUNK_DATA pRealIAT = (PIMAGE_THUNK_DATA)((DWORDX)pMemoryAddress + pID->FirstThunk);
                PIMAGE_THUNK_DATA pOriginalIAT = (PIMAGE_THUNK_DATA)((DWORDX)pMemoryAddress + pID->OriginalFirstThunk);
                //��ȡdll������
                char* pName = (char*)((DWORDX)pMemoryAddress + pID->Name);
                HANDLE hDll = 0;

                RtlInitAnsiString(&ansiStr, pName);

                RtlAnsiStringToUnicodeString(&UnicodeString, &ansiStr, true);
                pLdrLoadDll(NULL, NULL, &UnicodeString, &hDll);
                RtlFreeUnicodeString(&UnicodeString);

                if (hDll == NULL) {

                    goto CODEEXIT; //NOT FOUND DLL
                }

                //��ȡDLL��ÿ�����������ĵ�ַ������IAT
                //ÿ��IAT�ṹ�� ��
                // union { PBYTE ForwarderString;
                // PDWORDX Function;
                // DWORDX Ordinal;
                // PIMAGE_IMPORT_BY_NAME AddressOfData;
                // } u1;
                // ������һ��DWORDX ����������һ����ַ��
                for (i = 0; ; i++)
                {
                    if (pOriginalIAT[i].u1.Function == 0)break;
                    FARPROC lpFunction = NULL;
                    if (IMAGE_SNAP_BY_ORDINAL(pOriginalIAT[i].u1.Ordinal)) //�����ֵ�������ǵ������
                    {
                        if (IMAGE_ORDINAL(pOriginalIAT[i].u1.Ordinal))
                        {

                            LdrGetProcedureAddress(hDll, NULL, IMAGE_ORDINAL(pOriginalIAT[i].u1.Ordinal), &lpFunction);
                        }
                    }
                    else//�������ֵ���
                    {
                        //��ȡ��IAT���������ĺ�������
                        pByName = (PIMAGE_IMPORT_BY_NAME)((DWORDX)pMemoryAddress + (DWORDX)(pOriginalIAT[i].u1.AddressOfData));
                        if ((char*)pByName->Name)
                        {
                            RtlInitAnsiString(&ansiStr, (char*)pByName->Name);
                            LdrGetProcedureAddress(hDll, &ansiStr, 0, &lpFunction);

                        }

                    }

                    //���***********

                    if (lpFunction != NULL) //�ҵ��ˣ�
                        pRealIAT[i].u1.Function = (DWORDX)lpFunction;
                    else
                        goto CODEEXIT;
                }

                //move to next
                pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORDX)pID + sizeof(IMAGE_IMPORT_DESCRIPTOR));
            }

            /***********************************************************/
            //��������ַ
            pNTHeader->OptionalHeader.ImageBase = (DWORDX)pMemoryAddress;

            //NtProtectVirtualMemory((HANDLE)-1, &pMemoryAddress, (PSIZE_T)&ImageSize, PAGE_EXECUTE_READ, &oldProtect);
            pDllMain = (ProcDllMain)(pNTHeader->OptionalHeader.AddressOfEntryPoint + (DWORDX)pMemoryAddress);

            pDllMain(0, DLL_PROCESS_ATTACH, pMemoryAddress);//����Ĳ���1����Ӧ�ô�����(HMODULE)pMemoryAddress,����û��Ҫ,��Ϊ�޷�ʹ����Դ,����û��Ҫ,Ҫʹ����Դ,��̳��������˵�����ʹ��

        }
    }

CODEEXIT:

    return (DWORDX)pMemoryAddress;
}
