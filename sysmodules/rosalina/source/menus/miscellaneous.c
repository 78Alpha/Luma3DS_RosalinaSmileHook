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
#include "menus/miscellaneous.h"
#include "input_redirection.h"
#include "mcu.h"
#include "memory.h"
#include "draw.h"
#include "hbloader.h"
#include "fmt.h"
#include "utils.h" // for makeARMBranch
#include "minisoc.h"
#include "ifile.h"
#include "petithook.h"

Menu miscellaneousMenu = {
    "Miscellaneous options menu",
    .nbItems = 5,
    {
        { "Switch the hb. title to the current app.", METHOD, .method = &MiscellaneousMenu_SwitchBoot3dsxTargetTitle },
        { "Change the menu combo", METHOD, .method = MiscellaneousMenu_ChangeMenuCombo },
        { "Start InputRedirection", METHOD, .method = &MiscellaneousMenu_InputRedirection },
        { "Save settings", METHOD, .method = &MiscellaneousMenu_SaveSettings },
        {"New SmileHook RamDumper", METHOD, .method = &SmileHook },
    }
};

void SmileHook(void)
{
     //#define TRY(expr) if(R_FAILED(res = (expr))) goto end;
    
   
    static MyThread *smileNetThread = NULL;
     Result res = svcCreateEvent(&smileNetThreadStartedEvent, RESET_STICKY);
                    if( R_SUCCEEDED(res) )
                    {
                        smileNetThread = smileNetCreateThread();
                        res = svcWaitSynchronization(smileNetThreadStartedEvent, 10 * 1000 * 1000 * 1000LL);
                        if(res == 0)
                            res = (Result)smileNetStartResult;
                    }
    
   

    
      //  do
       // {   
               // Draw_Lock();
               // Draw_FlushFramebuffer();
               // Draw_Unlock();
               // u32 buttons = HID_PAD;
       
               // if (buttons & BUTTON_DOWN){
               //memindex = memindex + 1;
                     //  }
           
             //if (buttons & BUTTON_A){((u32*)startAddress)[memindex+10] = 0x000000FF;
                //}
          //  Draw_Lock();
       
      
           // value_ = ((u32*)startAddress)[a];

               // do{
                //nothing
               // Draw_FlushFramebuffer();
               // }while (!waitInput() & BUTTON_DOWN);
                //((u32*)startAddress)[memindex+12] = 0x000000FF;
        
       
       
                //Draw_ClearFramebuffer();
        
       
    
        //}while(!(waitInput() & BUTTON_B) && !terminationRequest);

            //Draw_Unlock();//put unmapping process handle stuff here later

   
    
    
     //#undef TRY
      //return res;
}

void nothing(void){
 do{
     
 Draw_DrawFormattedString(10, (12)*10, COLOR_GREEN, "Press B to continue");

 } while(! (waitInput() & BUTTON_B));
    
}

void MiscellaneousMenu_SwitchBoot3dsxTargetTitle(void)
{
    Result res;
    u64 titleId = 0;
    char failureReason[64];

    if(HBLDR_3DSX_TID == HBLDR_DEFAULT_3DSX_TID)
    {
        u32 pidList[0x40];
        s32 processAmount;

        res = svcGetProcessList(&processAmount, pidList, 0x40);
        if(R_SUCCEEDED(res))
        {
            for(s32 i = 0; i < processAmount && (u32)(titleId >> 32) != 0x00040010 && (u32)(titleId >> 32) != 0x00040000; i++)
            {
                Handle processHandle;
                Result res = svcOpenProcess(&processHandle, pidList[i]);
                if(R_FAILED(res))
                    continue;

                svcGetProcessInfo((s64 *)&titleId, processHandle, 0x10001);
                svcCloseHandle(processHandle);
            }
        }

        if(R_SUCCEEDED(res) && ((u32)(titleId >> 32) == 0x00040010 || (u32)(titleId >> 32) == 0x00040000))
        {
            HBLDR_3DSX_TID = titleId;
            miscellaneousMenu.items[0].title = "Switch the hb. title to hblauncher_loader";
        }
        else if(R_FAILED(res))
            sprintf(failureReason, "%08x", (u32)res);
        else
        {
            res = -1;
            strcpy(failureReason, "no suitable process found");
        }
    }
    else
    {
        res = 0;
        HBLDR_3DSX_TID = HBLDR_DEFAULT_3DSX_TID;
        miscellaneousMenu.items[0].title = "Switch the hb. title to the current app.";
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (%s).", failureReason);

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

static void MiscellaneousMenu_ConvertComboToString(char *out, u32 combo)
{
    static const char *keys[] = { "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y" };
    for(s32 i = 11; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keys[i]);
            out += strlen(keys[i]);
            *out++ = '+';
        }
    }

    out[-1] = 0;
}

void MiscellaneousMenu_ChangeMenuCombo(void)
{
    char comboStrOrig[64], comboStr[64];
    u32 posY;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    MiscellaneousMenu_ConvertComboToString(comboStrOrig, menuCombo);

    Draw_Lock();
    Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

    posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "The current menu combo is:  %s", comboStrOrig);
    posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Please enter the new combo:");

    Draw_FlushFramebuffer();
    Draw_Unlock();

    menuCombo = waitCombo();
    MiscellaneousMenu_ConvertComboToString(comboStr, menuCombo);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "The current menu combo is:  %s", comboStrOrig);
        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Please enter the new combo: %s", comboStr) + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Successfully changed the menu combo.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void MiscellaneousMenu_SaveSettings(void)
{
    Result res;

    IFile file;
    u64 total;

    struct PACKED
    {
        char magic[4];
        u16 formatVersionMajor, formatVersionMinor;

        u32 config, multiConfig, bootConfig;
        u64 hbldr3dsxTitleId;
        u32 rosalinaMenuCombo;
    } configData;

    u32 formatVersion;
    u32 config, multiConfig, bootConfig;
    s64 out;
    bool isSdMode;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 2))) svcBreak(USERBREAK_ASSERT);
    formatVersion = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 3))) svcBreak(USERBREAK_ASSERT);
    config = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 4))) svcBreak(USERBREAK_ASSERT);
    multiConfig = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 5))) svcBreak(USERBREAK_ASSERT);
    bootConfig = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    memcpy(configData.magic, "CONF", 4);
    configData.formatVersionMajor = (u16)(formatVersion >> 16);
    configData.formatVersionMinor = (u16)formatVersion;
    configData.config = config;
    configData.multiConfig = multiConfig;
    configData.bootConfig = bootConfig;
    configData.hbldr3dsxTitleId = HBLDR_3DSX_TID;
    configData.rosalinaMenuCombo = menuCombo;

    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/config.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE);

    if(R_SUCCEEDED(res))
        res = IFile_Write(&file, &total, &configData, sizeof(configData), 0);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08x).", res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void MiscellaneousMenu_InputRedirection(void)
{
    
}

//Function provided by Sono Chan ^_^ Thank you
Result syncdma(Handle desth, u32 dest, Handle srch, void* src, u32 size)
{
	Handle dmahand = 0;
	
	u8 dmaconf[0x18];
	memset(dmaconf, 0, sizeof(dmaconf));
	dmaconf[0] = -1; //don't care

	Result res = svcFlushProcessDataCache(desth, (void*)dest, size);
	if (res <0 ) return res;
	
	res = svcFlushProcessDataCache(srch, src, size);
	if (res <0) return res;
	
	res = svcStartInterProcessDma(&dmahand, desth, (void*)dest, srch, src, size, dmaconf);
	if (res < 0) return res;

	u32 stat;
	while(1)
	{
		svcGetDmaState(&stat, dmahand);
		if(stat & 0xFFF00000)
			break;
		else
			svcSleepThread(1000);
	}

	svcStopDma(dmahand);
	svcCloseHandle(dmahand);
	
	svcInvalidateProcessDataCache(desth, (void*)dest, size);

	return 0;
}
