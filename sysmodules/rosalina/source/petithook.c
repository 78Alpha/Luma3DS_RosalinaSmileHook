/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include <arpa/inet.h>
#include "utils.h" // for makeARMBranch
#include "minisoc.h"
#include "petitcommemhook.h"
#include "menus.h"
#include "memory.h"
#include "draw.h"
#include "menu.h"
#include "ifile.h"

bool petithook_enabled = false;
Handle petithookStartedEvent;

static MyThread petithookthread;
static u8 ALIGN(8) petithookthreadStack[0x4000];

MyThread *petithookcreatethread(void)
{
        //The second parameter in MyThread_Create is the function name of the main thread...
                //Cool would have liked to know that sooner.
    if(R_FAILED(MyThread_Create(&petithookthread, petithookthreadmain, petithookthreadStack, 0x4000, 0x20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &petithookthread;
}

//                       local hid,  local tsrd  localcprd,  localtswr,  localcpwr,  remote hid, remote ts,  remote circle
static u32 hidData[] = { 0x00000FFF, 0x02000000, 0x007FF7FF, 0x00000000, 0x00000000, 0x00000FFF, 0x02000000, 0x007FF7FF };
//                      remote ir
static u32 irData[] = { 0x80800081 }; // Default: C-Stick at the center, no buttons.

int petithookstartresult;

void petithookthreadmain(void)
{
    Result res = 0;
    res = miniSocInit();
    if(R_FAILED(res))
        return;

    int sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    while(sock == -1)
    {
        svcSleepThread(1000 * 0000 * 0000LL);
        sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(4950);
    saddr.sin_addr.s_addr = gethostid();
    res = socBind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
    if(res != 0)
    {
        socClose(sock);
        miniSocExit();
        petithookstartresult = res;
        return;
    }

    petithook_enabled = true;
    svcSignalEvent(petithookStartedEvent);

    u32 *hidDataPhys = PA_FROM_VA_PTR(hidData);
    hidDataPhys += 5; // skip to +20

    u32 *irDataPhys = PA_FROM_VA_PTR(irData);

    char buf[20];
    u32 oldSpecialButtons = 0, specialButtons = 0;
    while(petithook_enabled && !terminationRequest)
    {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int pollres = socPoll(&pfd, 1, 10);
        if(pollres > 0 && (pfd.revents & POLLIN))
        {
            int n = soc_recvfrom(sock, buf, 20, 0, NULL, 0);
            if(n < 0)
                break;
            else if(n < 12)
                continue;

            memcpy(hidDataPhys, buf, 12);
            if(n >= 20)
            {
                memcpy(irDataPhys, buf + 12, 4);

                oldSpecialButtons = specialButtons;
                memcpy(&specialButtons, buf + 16, 4);

                if(!(oldSpecialButtons & 1) && (specialButtons & 1)) // HOME button pressed
                    srvPublishToSubscriber(0x204, 0);
                else if((oldSpecialButtons & 1) && !(specialButtons & 1)) // HOME button released
                    srvPublishToSubscriber(0x205, 0);

                if(!(oldSpecialButtons & 2) && (specialButtons & 2)) // POWER button pressed
                    srvPublishToSubscriber(0x202, 0);

                if(!(oldSpecialButtons & 4) && (specialButtons & 4)) // POWER button held long
                    srvPublishToSubscriber(0x203, 0);
            }
        }
    }

    struct linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;

    socSetsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));

    socClose(sock);
    miniSocExit();
}

void hidCodePatchFunc(void);
void irCodePatchFunc(void);

Result PetitMemHook_DoOrUndoPatches(void)
{
    #define TRY(expr) if(R_FAILED(res = (expr))) goto end;
    s64 startAddress, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize;
    u32 totalSize;
    Handle processHandle;
    static int memindex = 0;
    u32 memoffset = 0;
    char buf[65];

    Result res = OpenProcessByName("petitcom", &processHandle);

    if(R_SUCCEEDED(res))
    {
        
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    const u32 destAddress = 0x00100000;
    
        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002);
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);
        totalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);
        svcGetProcessInfo(&startAddress, processHandle, 0x10005);
        res = svcMapProcessMemoryEx(processHandle, destAddress, (u32)startAddress, totalSize);

    //value_ = ((u32*)(startAddress))[i+0x002FC075 - 0x00100000];

    u32 memoffset = 0;
    
    bool need_update = false;
    //0x10002 to 0x10007 are 
    
    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;

    //Trying to write file shit
    u64 total;
    IFile file;
    Result res;

    u32 filenum;
    char filename[64];
    bool isSdMode;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();
    svcFlushEntireDataCache();

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/smilehook/memdumps"), 0);
        if( (u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

        Result res1, res2, res3;
        IFile fileR;
            
            //I think that it's putting the string into the filename instead of the screen?
        sprintf(filename, "/smilehook/memdumps/memdump.bin");
        res1 = IFile_Open(&fileR, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_READ);
        IFile_Close(&fileR);
        //total = 100; //Try to write 4 MB of ram to dump file
        TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
        //TRY(IFile_Write(&file, &total, framebufferCache, 3 * 320 * 120, 0));
    
      
      //value_ = ((u32*)startAddress+memoffset+0x002FC075 - 0x00100000)[i];

            //TRY(IFile_Write(&file, &total, buffer, total, 0));
            TRY(IFile_Write(&file, &total, startAddress, totalSize, 0));
            TRY(IFile_Close(&file));

    end:
    IFile_Close(&file);
    
    }
    
    do
    {   
        Draw_Lock();
        Draw_FlushFramebuffer();
        Draw_Unlock();
    u32 buttons = HID_PAD;
       
        if (buttons & BUTTON_DOWN){
               memindex = memindex + 1;
                        }
           
             //if (buttons & BUTTON_A){((u32*)startAddress)[memindex+10] = 0x000000FF;
                //}
    Draw_Lock();
       
      
           // value_ = ((u32*)startAddress)[a];

        do{
            //nothing
            Draw_FlushFramebuffer();
        }while (!waitInput() & BUTTON_DOWN);
        ((u32*)startAddress)[memindex+12] = 0x000000FF;
        
       
       
        //Draw_ClearFramebuffer();
        
       
    
    }while(!(waitInput() & BUTTON_B) && !terminationRequest);

 Draw_Unlock();//put unmapping process handle stuff here later

   
    
    return res;
}
