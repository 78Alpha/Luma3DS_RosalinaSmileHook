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
#include "input_redirection.h"
#include "menus.h"
#include "memory.h"

bool inputRedirectionEnabled = false;
Handle inputRedirectionThreadStartedEvent;

static MyThread inputRedirectionThread;
static u8 ALIGN(8) inputRedirectionThreadStack[0x4000];

MyThread *inputRedirectionCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&inputRedirectionThread, inputRedirectionThreadMain, inputRedirectionThreadStack, 0x4000, 0x20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &inputRedirectionThread;
}

//                       local hid,  local tsrd  localcprd,  localtswr,  localcpwr,  remote hid, remote ts,  remote circle
static u32 hidData[] = { 0x00000FFF, 0x02000000, 0x007FF7FF, 0x00000000, 0x00000000, 0x00000FFF, 0x02000000, 0x007FF7FF };
//                      remote ir
static u32 irData[] = { 0x80800081 }; // Default: C-Stick at the center, no buttons.

int inputRedirectionStartResult;

void inputRedirectionThreadMain(void)
{
    Result res = 0;
   
}

void hidCodePatchFunc(void);
void irCodePatchFunc(void);

Result InputRedirection_DoOrUndoPatches(void)
{
    Result res = 0;
    return res;
}
