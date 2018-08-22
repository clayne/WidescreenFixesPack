#include "stdafx.h"

static bool bAccessedPostRace;
uint32_t StuffToCompare = 0;
bool __stdcall memory_readable(void *ptr, size_t byteCount)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
        return false;

    if (mbi.State != MEM_COMMIT)
        return false;

    if (mbi.Protect == PAGE_NOACCESS || mbi.Protect == PAGE_EXECUTE)
        return false;

    // This checks that the start of memory block is in the same "region" as the
    // end. If it isn't you "simplify" the problem into checking that the rest of 
    // the memory is readable.
    size_t blockOffset = (size_t)((char *)ptr - (char *)mbi.AllocationBase);
    size_t blockBytesPostPtr = mbi.RegionSize - blockOffset;

    if (blockBytesPostPtr < byteCount)
        return memory_readable((char *)ptr + blockBytesPostPtr,
            byteCount - blockBytesPostPtr);

    return true;
}

uint32_t DamageModelFixExit;
void __declspec(naked) DamageModelMemoryCheck()
{
    _asm
    {
        push edi
        mov edi, [esp + 8]
        mov StuffToCompare, edi
    }

    if (memory_readable((void*)StuffToCompare, 8))
        _asm jmp DamageModelFixExit

    _asm
    {
        pop edi
        retn
    }
}

uint32_t loc_5C479A;
uint32_t loc_5C47B0;
void __declspec(naked) ExitPostRaceFixPropagator()
{
    if (bAccessedPostRace)
        _asm jmp loc_5C479A
    else
        _asm jmp loc_5C47B0
}

void __declspec(naked) ExitPostRaceFixPart2()
{
    _asm
    {
        mov bAccessedPostRace, 0
        pop esi
        retn 4
    }
}

void Init()
{
    FreeConsole();
    CIniReader iniReader("");
    auto bResDetect = iniReader.ReadInteger("MultiFix", "ResDetect", 1) != 0;
    auto bPostRaceFix = iniReader.ReadInteger("MultiFix", "PostRaceFix", 1) != 0;
    auto bFramerateUncap = iniReader.ReadInteger("MultiFix", "FramerateUncap", 1) != 0;
    auto bAntiTrackStreamerCrash = iniReader.ReadInteger("MultiFix", "AntiTrackStreamerCrash", 1);
    auto bAntiDamageModelCrash = iniReader.ReadInteger("MultiFix", "AntiDamageModelCrash", 1);

    if (bResDetect)
    {
        struct ScreenRes
        {
            uint32_t ResX;
            uint32_t ResY;
        };

        std::vector<std::string> list;
        GetResolutionsList(list);
        static std::vector<ScreenRes> list2;
        for each (auto s in list)
        {
            ScreenRes r;
            sscanf_s(s.c_str(), "%dx%d", &r.ResX, &r.ResY);
            list2.emplace_back(r);
        }

        auto pattern = hook::pattern("83 C0 08 83 F8 58 72 E8 E9 ? ? ? ? 8B 53 3C 8B 43 10");
        injector::WriteMemory(pattern.get_first(-6), &list2[0].ResX, true); //0x0070B3EA
        injector::WriteMemory(pattern.get_first(-14), &list2[0].ResY, true);
        injector::WriteMemory<uint8_t>(pattern.get_first(5), 0xFFi8, true); //0x70B3F3+2
    }

    // PostRaceStateManagerFix
    if (bPostRaceFix)
    {
        auto pattern = hook::pattern("C6 44 24 ? ? E8 ? ? ? ? 6A 0A");
        struct PostRaceFix1
        {
            void operator()(injector::reg_pack& regs)
            {
                bAccessedPostRace = true;
                *(uint8_t*)(regs.esp + 0x30) = 4;
            }
        }; injector::MakeInline<PostRaceFix1>(pattern.get_first(0));

        pattern = hook::pattern("C7 46 ? ? ? ? ? E8 ? ? ? ? C6 86 ? ? ? ? ? 5E C2 04 00");
        injector::MakeJMP(pattern.get_first(19), ExitPostRaceFixPart2, true); //0x005C47EA
        pattern = hook::pattern("80 BE ? ? ? ? ? E9");
        injector::MakeJMP(pattern.count_hint(6).get(2).get<void>(7), ExitPostRaceFixPropagator, true); //0x004CEAAD

        DamageModelFixExit = (uint32_t)hook::get_pattern("85 FF 74 2C 8B 44 24 0C 56 50 8B CF", 0); //0x0058DC15
        loc_5C479A = (uint32_t)hook::get_pattern("75 0B C6 86 ? ? ? ? ? 5E C2 04 00", 0);//0x5C479A
        loc_5C47B0 = (uint32_t)hook::get_pattern("8B 46 04 8B 4C 24 08 8B 16 89 46 14 8B 82 ? ? ? ? 89 4E 04 8B CE FF D0 8B 8E", 0); //0x5C47B0
    }

    if (bFramerateUncap) // Framerate unlock
    {
        auto pattern = hook::pattern("83 3D ? ? ? ? ? 75 05 E8 ? ? ? ? 8B 0D ? ? ? ? 85 C9 74 0F 6A 09");
        injector::MakeJMP(pattern.get_first(0), pattern.get_first(14), true); //0x004B42FA, 0x4B4308
    }

    if (bAntiTrackStreamerCrash)
    {
        auto pattern = hook::pattern("80 7C 24 ? ? 74 10 68 ? ? ? ? 55");
        injector::MakeJMP(pattern.get_first(0), pattern.get_first(23), true); //0x00748A09, 0x748A20
        pattern = hook::pattern("75 11 01 6C 24 1C 83 43 28 01");
        injector::MakeNOP(pattern.get_first(0), 2, true); //0x007489FD
    }

    if (bAntiDamageModelCrash)
    {
        auto pattern = hook::pattern("57 8B 7C 24 08 85 FF 74 2C 8B 44 24 0C 56");
        injector::MakeJMP(pattern.get_first(0), DamageModelMemoryCheck, true); //0x58DC10
    }


    bool bSkipIntro = iniReader.ReadInteger("MISC", "SkipIntro", 1) != 0;
    static int32_t nWindowedMode = iniReader.ReadInteger("MISC", "WindowedMode", 0);
    static int32_t nImproveGamepadSupport = iniReader.ReadInteger("MISC", "ImproveGamepadSupport", 0);
    static float fLeftStickDeadzone = iniReader.ReadFloat("MISC", "LeftStickDeadzone", 10.0f);
    bool bWriteSettingsToFile = iniReader.ReadInteger("MISC", "WriteSettingsToFile", 1) != 0;
    static auto szCustomUserFilesDirectoryInGameDir = iniReader.ReadString("MISC", "CustomUserFilesDirectoryInGameDir", "0");
    if (szCustomUserFilesDirectoryInGameDir.empty() || szCustomUserFilesDirectoryInGameDir == "0")
        szCustomUserFilesDirectoryInGameDir.clear();

    if (bSkipIntro)
    {
        auto pattern = hook::pattern("A3 ? ? ? ? A1 ? ? ? ? 6A 20 50"); //0xA9E6D8
        injector::WriteMemory(*pattern.count(3).get(2).get<uint32_t*>(1), 1, true);
        pattern = hook::pattern("A1 ? ? ? ? 85 C0 74 ? 57 8B F8 E8"); //0xA97BC0
        injector::WriteMemory(*pattern.count(1).get(0).get<uint32_t*>(1), 1, true);
    }

    if (nWindowedMode)
    {
        auto pattern = hook::pattern("89 5D 3C 89 5D 18 89 5D 44"); //0x708379
        struct WindowedMode
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint32_t*)(regs.ebp + 0x3C) = 1;
                *(uint32_t*)(regs.ebp + 0x18) = regs.ebx;
            }
        }; injector::MakeInline<WindowedMode>(pattern.get_first(0), pattern.get_first(6));

        static auto hGameHWND = *hook::get_pattern<HWND*>("8B 0D ? ? ? ? 51 8B C8 E8 ? ? ? ? EB 02 33 C0", 2); //0x00AC6ED8
        pattern = hook::pattern("A3 ? ? ? ? 89 35 ? ? ? ? 88 1D ? ? ? ? B9 ? ? ? ? 74 0B 6A 15");
        static auto Width = *pattern.get_first<int32_t*>(1);
        static auto Height = *pattern.get_first<int32_t*>(7);

        struct ResHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *Width = regs.eax;
                *Height = regs.esi;

                tagRECT rc;
                auto[DesktopResW, DesktopResH] = GetDesktopRes();
                rc.left = (LONG)(((float)DesktopResW / 2.0f) - ((float)*Width / 2.0f));
                rc.top = (LONG)(((float)DesktopResH / 2.0f) - ((float)*Height / 2.0f));
                rc.right = *Width;
                rc.bottom = *Height;
                SetWindowPos(*hGameHWND, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
                if (nWindowedMode > 1)
                {
                    SetWindowLong(*hGameHWND, GWL_STYLE, GetWindowLong(*hGameHWND, GWL_STYLE) | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU);
                    SetWindowPos(*hGameHWND, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
                }
                else
                {
                    SetWindowLong(*hGameHWND, GWL_STYLE, GetWindowLong(*hGameHWND, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);
                }
            }
        }; injector::MakeInline<ResHook>(pattern.get_first(0), pattern.get_first(11));

        pattern = hook::pattern("89 74 24 18 89 74 24 1C 74 08 8B 53 54 8B 4A 6C"); //0x70E303
        struct WindowedModeRect
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = 1;
                *(uint32_t*)(regs.ebx + 0x3C) = 1;

                tagRECT rc;
                auto[DesktopResW, DesktopResH] = GetDesktopRes();
                rc.left = (LONG)(((float)DesktopResW / 2.0f) - ((float)*Width / 2.0f));
                rc.top = (LONG)(((float)DesktopResH / 2.0f) - ((float)*Height / 2.0f));
                rc.right = *Width;
                rc.bottom = *Height;

                *(uint32_t*)(regs.esp + 0x18) = rc.left;
                *(uint32_t*)(regs.esp + 0x1C) = rc.top;
                *(uint32_t*)(regs.esp + 0x20) = rc.right;
                *(uint32_t*)(regs.esp + 0x24) = rc.bottom;
            }
        }; injector::MakeInline<WindowedModeRect>(pattern.get_first(0), pattern.get_first(8));
        pattern = hook::pattern("89 4C 24 20 74 08 8B 53 54 8B 4A 70");
        injector::MakeNOP(pattern.get_first(0), 4, true);
        pattern = hook::pattern("01 74 24 20 03 CE 3B C5");
        injector::MakeNOP(pattern.get_first(0), 4, true);
        injector::MakeNOP(pattern.get_first(8), 4, true);
    }

    if (nImproveGamepadSupport)
    {
        auto pattern = hook::pattern("6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 51 A1 ? ? ? ? 33 C4 50 8D 44 24 08 64 A3 ? ? ? ? A1 ? ? ? ? 50");
        static auto CreateResourceFile = (void*(*)(const char* ResourceFileName, int32_t ResourceFileType, int, int, int)) pattern.get_first(0); //0x006D6DE0
        pattern = hook::pattern("8B 44 24 04 56 8B F1 8B 4C 24 0C 89 46 38 89 4E 3C");
        static auto ResourceFileBeginLoading = (void(__thiscall *)(void* rsc, int a1, int a2)) pattern.get_first(0); //0x006D9430
        static auto LoadResourceFile = [](const char* ResourceFileName, int32_t ResourceFileType, int32_t nUnk1 = 0, int32_t nUnk2 = 0, int32_t nUnk3 = 0, int32_t nUnk4 = 0, int32_t nUnk5 = 0)
        {
            auto r = CreateResourceFile(ResourceFileName, ResourceFileType, nUnk1, nUnk2, nUnk3);
            ResourceFileBeginLoading(r, nUnk4, nUnk5);
        };

        static auto TPKPath = GetThisModulePath<std::string>().substr(GetExeModulePath<std::string>().length());

        if (nImproveGamepadSupport == 1)
            TPKPath += "buttons-xbox.tpk";
        else if (nImproveGamepadSupport == 2)
            TPKPath += "buttons-playstation.tpk";

        static injector::hook_back<void(__cdecl*)()> hb_6D98B0;
        auto LoadTPK = []()
        {
            LoadResourceFile(TPKPath.c_str(), 1);
            return hb_6D98B0.fun();
        };

        pattern = hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 35 ? ? ? ? 6A 04 FF D6 83 C4 04 85 C0"); //0x6DABDE
        hb_6D98B0.fun = injector::MakeCALL(pattern.get_first(0), static_cast<void(__cdecl*)()>(LoadTPK), true).get();

        struct PadState
        {
            int32_t LSAxis1;
            int32_t LSAxis2;
            int32_t LTRT;
            int32_t A;
            int32_t B;
            int32_t X;
            int32_t Y;
            int32_t LB;
            int32_t RB;
            int32_t Select;
            int32_t Start;
            int32_t LSClick;
            int32_t RSClick;
        };

        pattern = hook::pattern("C7 45 E0 01 00 00 00 89 5D E4"); //0x892BFD
        static uintptr_t ButtonsState = (uintptr_t)*hook::pattern("0F B6 54 24 04 33 C0 B9").get(0).get<uint32_t*>(8); //0xA5E058
        static int32_t* nGameState = (int32_t*)*hook::pattern("83 3D ? ? ? ? 06 ? ? A1").get(0).get<uint32_t*>(2); //0xA99BBC
        struct CatchPad
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uintptr_t*)(regs.ebp - 0x20) = 1; //mov     dword ptr [ebp-20h], 1

                PadState* PadKeyPresses = (PadState*)(regs.esi + 0x234); //dpad is PadKeyPresses+0x220

                //Keyboard
                //00A5E15C T
                //00A5E124 M
                //00A5E1B0 1
                //00A5E194 2
                //00A5E140 3
                //00A5E178 4
                //00A5E1CC 9
                //00A5E1E8 0
                //00A5E108 Space
                //00A5E0D0 ESC
                //00A5E0B4 Enter
                if (PadKeyPresses != nullptr && PadKeyPresses != (PadState*)0x1)
                {
                    if (PadKeyPresses->Select)
                    {
                        *(int32_t*)(ButtonsState + 0xE8) = 1;
                    }
                    else
                    {
                        *(int32_t*)(ButtonsState + 0xE8) = 0;
                    }

                    if (PadKeyPresses->X)
                    {
                        *(int32_t*)(ButtonsState + 0x13C) = 1;
                    }
                    else
                    {
                        *(int32_t*)(ButtonsState + 0x13C) = 0;
                    }
                    if (PadKeyPresses->LSClick && PadKeyPresses->RSClick)
                    {
                        if (*nGameState == 3)
                        {
                            keybd_event(BYTE(VkKeyScan('Q')), 0, 0, 0);
                            keybd_event(BYTE(VkKeyScan('Q')), 0, KEYEVENTF_KEYUP, 0);
                        }
                    }
                }
            }
        }; injector::MakeInline<CatchPad>(pattern.get_first(0), pattern.get_first(7));

        const char* ControlsTexts[] = { " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", " 10", " 1", " Up", " Down", " Left", " Right", "X Rotation", "Y Rotation", "X Axis", "Y Axis", "Z Axis", "Hat Switch" };
        const char* ControlsTextsXBOX[] = { "B", "X", "Y", "LB", "RB", "View (Select)", "Menu (Start)", "Left stick", "Right stick", "A", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Right stick Left/Right", "Right stick Up/Down", "Left stick Left/Right", "Left stick Up/Down", "LT / RT", "D-pad" };
        const char* ControlsTextsPS[] = { "Circle", "Square", "Triangle", "L1", "R1", "Select", "Start", "L3", "R3", "Cross", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Right stick Left/Right", "Right stick Up/Down", "Left stick Left/Right", "Left stick Up/Down", "L2 / R2", "D-pad" };

        static std::vector<std::string> Texts(ControlsTexts, std::end(ControlsTexts));
        static std::vector<std::string> TextsXBOX(ControlsTextsXBOX, std::end(ControlsTextsXBOX));
        static std::vector<std::string> TextsPS(ControlsTextsPS, std::end(ControlsTextsPS));

        pattern = hook::pattern("68 04 01 00 00 51 E8 ? ? ? ? 83 C4 10 5F 5E C3"); //0x679B53
        injector::WriteMemory<uint8_t>(pattern.get(0).get<uint32_t>(16 + 5), 0xC3, true);
        struct Buttons
        {
            void operator()(injector::reg_pack& regs)
            {
                auto pszStr = *(char**)(regs.esp + 4);
                auto it = std::find_if(Texts.begin(), Texts.end(), [&](const std::string& str) { std::string s(pszStr); return s.find(str) != std::string::npos; });
                auto i = std::distance(Texts.begin(), it);

                if (it != Texts.end())
                {
                    if (nImproveGamepadSupport != 2)
                        strcpy(pszStr, TextsXBOX[i].c_str());
                    else
                        strcpy(pszStr, TextsPS[i].c_str());
                }
            }
        }; injector::MakeInline<Buttons>(pattern.get_first(16));

        pattern = hook::pattern("8B 0F 8B 54 0E 08 DB 44 90 0C"); //0x6A5E4C
        static auto unk_A987EC = *hook::get_pattern<void**>("81 FE ? ? ? ? 0F 8C ? ? ? ? 8B 44 24 14 83 B8", 2);
        struct MenuRemap
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = *(uint32_t*)regs.edi;
                regs.edx = *(uint32_t*)(regs.esi + regs.ecx + 0x08);

                auto dword_A9882C = &unk_A987EC[16];
                auto dword_A98834 = &unk_A987EC[18];
                auto dword_A9883C = &unk_A987EC[20];
                auto dword_A98874 = &unk_A987EC[34];

                *(uint32_t*)(*(uint32_t*)dword_A9882C + 0x20) = 0; // "Enter"; changed B to A
                *(uint32_t*)(*(uint32_t*)dword_A98834 + 0x20) = 1; // "ESC"; changed X to B
                *(uint32_t*)(*(uint32_t*)dword_A9883C + 0x20) = 7; // "SPC"; changed RS to Start
                *(uint32_t*)(*(uint32_t*)dword_A98874 + 0x20) = 3; // "1"; changed A to Y
            }
        }; injector::MakeInline<MenuRemap>(pattern.get_first(0), pattern.get_first(6));
    }

    if (fLeftStickDeadzone)
    {
        // DInput [ 0 32767 | 32768 65535 ] 
        fLeftStickDeadzone /= 200.0f;

        auto pattern = hook::pattern("89 86 34 02 00 00 8D 45 D4"); //0x892ACD 0x892ADF
        struct DeadzoneHookX
        {
            void operator()(injector::reg_pack& regs)
            {
                double dStickState = (double)regs.eax / 65535.0;
                dStickState -= 0.5;
                if (std::abs(dStickState) <= fLeftStickDeadzone)
                    dStickState = 0.0;
                dStickState += 0.5;
                *(uint32_t*)(regs.esi + 0x234) = (uint32_t)(dStickState * 65535.0);
            }
        }; injector::MakeInline<DeadzoneHookX>(pattern.get_first(0), pattern.get_first(6));

        struct DeadzoneHookY
        {
            void operator()(injector::reg_pack& regs)
            {
                double dStickState = (double)regs.edx / 65535.0;
                dStickState -= 0.5;
                if (std::abs(dStickState) <= fLeftStickDeadzone)
                    dStickState = 0.0;
                dStickState += 0.5;
                *(uint32_t*)(regs.esi + 0x238) = (uint32_t)(dStickState * 65535.0);
            }
        }; injector::MakeInline<DeadzoneHookY>(pattern.get_first(18 + 0), pattern.get_first(18 + 6));
    }

    auto GetFolderPathpattern = hook::pattern("50 6A 00 6A 00 68 ? 80 00 00 6A 00");
    if (!szCustomUserFilesDirectoryInGameDir.empty())
    {
        szCustomUserFilesDirectoryInGameDir = GetExeModulePath<std::string>() + szCustomUserFilesDirectoryInGameDir;

        auto SHGetFolderPathAHook = [](HWND /*hwnd*/, int /*csidl*/, HANDLE /*hToken*/, DWORD /*dwFlags*/, LPSTR pszPath) -> HRESULT
        {
            CreateDirectoryA(szCustomUserFilesDirectoryInGameDir.c_str(), NULL);
            strcpy(pszPath, szCustomUserFilesDirectoryInGameDir.c_str());
            return S_OK;
        };

        for (size_t i = 0; i < GetFolderPathpattern.size(); i++)
        {
            uint32_t* dword_6CBF17 = GetFolderPathpattern.get(i).get<uint32_t>(12);
            if (*(BYTE*)dword_6CBF17 != 0xFF)
                dword_6CBF17 = GetFolderPathpattern.get(i).get<uint32_t>(14);

            injector::MakeCALL((uint32_t)dword_6CBF17, static_cast<HRESULT(WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR)>(SHGetFolderPathAHook), true);
            injector::MakeNOP((uint32_t)dword_6CBF17 + 5, 1, true);
        }
    }

    if (bWriteSettingsToFile)
    {
        static CIniReader RegistryReader;

        auto RegCreateKeyAHook = [](HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) -> LONG
        {
            return 1;
        };

        auto RegOpenKeyExAHook = [](HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) -> LONG
        {
            if (RegistryReader.ReadString("MAIN", "DisplayName", "").length() == 0)
            {
                RegistryReader.WriteString("MAIN", "@", "INSERTYOURCDKEYHERE");
                RegistryReader.WriteString("MAIN", "DisplayName", "Need for Speed ProStreet");
                RegistryReader.WriteString("MAIN", "Installed From", "C:\\");
                RegistryReader.WriteString("MAIN", "Registration", "SOFTWARE\\Electronic Arts\\Electronic Arts\\Need for Speed ProStreet\\ergc");
                RegistryReader.WriteString("MAIN", "Language", "1");
                RegistryReader.WriteString("MAIN", "Locale", "en_us");
                RegistryReader.WriteString("MAIN", "CD Drive", "C:\\");
                RegistryReader.WriteString("MAIN", "Product GUID", "{CC419DDC-E0F0-4013-B25A-6FA036516F0D}");
                RegistryReader.WriteString("MAIN", "Region", "NA");
            }
            return ERROR_SUCCESS;
        };

        auto RegCloseKeyHook = [](HKEY hKey) -> LONG
        {
            return ERROR_SUCCESS;
        };

        auto RegSetValueExAHook = [](HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData) -> LONG
        {
            RegistryReader.WriteInteger("MAIN", (char*)lpValueName, *(DWORD*)lpData);
            return ERROR_SUCCESS;
        };

        auto RegQueryValueExAHook = [](HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) -> LONG
        {
            if (!lpValueName) //cd key
            {
                auto str = RegistryReader.ReadString("MAIN", "@", "");
                strncpy((char*)lpData, str.c_str(), *lpcbData);
                return ERROR_SUCCESS;
            }

            if (*lpType == REG_SZ)
            {
                if (strstr(lpValueName, "Install Dir"))
                {
                    GetModuleFileNameA(NULL, (char*)lpData, MAX_PATH);
                    *(strrchr((char*)lpData, '\\') + 1) = '\0';
                    return ERROR_SUCCESS;
                }

                auto str = RegistryReader.ReadString("MAIN", (char*)lpValueName, "");
                strncpy((char*)lpData, str.c_str(), *lpcbData);
            }
            else
            {
                DWORD nValue = RegistryReader.ReadInteger("MAIN", (char*)lpValueName, 0);
                injector::WriteMemory(lpData, nValue, true);
            }

            return ERROR_SUCCESS;
        };

        char szSettingsSavePath[MAX_PATH];
        uintptr_t GetFolderPathCallDest = injector::GetBranchDestination(GetFolderPathpattern.get(0).get<uintptr_t>(14), true).as_int();
        injector::stdcall<HRESULT(HWND, int, HANDLE, DWORD, LPSTR)>::call(GetFolderPathCallDest, NULL, 0x8005, NULL, NULL, szSettingsSavePath);
        strcat(szSettingsSavePath, "\\NFS ProStreet");
        strcat(szSettingsSavePath, "\\Settings.ini");
        RegistryReader.SetIniPath(szSettingsSavePath);
        uintptr_t* RegIAT = *hook::pattern("FF 15 ? ? ? ? 8D 54 24 40 52 68 3F 00 0F 00").get(0).get<uintptr_t*>(2);
        injector::WriteMemory(&RegIAT[0], static_cast<LONG(WINAPI*)(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)>(RegCreateKeyAHook), true);
        injector::WriteMemory(&RegIAT[1], static_cast<LONG(WINAPI*)(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)>(RegOpenKeyExAHook), true);
        injector::WriteMemory(&RegIAT[2], static_cast<LONG(WINAPI*)(HKEY hKey)>(RegCloseKeyHook), true);
        injector::WriteMemory(&RegIAT[3], static_cast<LONG(WINAPI*)(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData)>(RegSetValueExAHook), true);
        injector::WriteMemory(&RegIAT[4], static_cast<LONG(WINAPI*)(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)>(RegQueryValueExAHook), true);
    }

}

CEXP void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init, hook::pattern("50 6A 00 6A 00 68 ? 80 00 00 6A 00").count_hint(2).empty());
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {

    }
    return TRUE;
}