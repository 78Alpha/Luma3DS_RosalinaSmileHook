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
#include <errno.h>
#include <3ds.h>
#include "utils.h" // for makeARMBranch
#include "petithook.h"
#include "menus.h"
#include "memory.h"
#include "draw.h"
#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
//#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sock_util.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



bool smileNetEnabled = false;
Handle smileNetThreadStartedEvent;
u8 smileNetThreadStack[0x4000];
static MyThread smileNetThread;
static char messagebuf[256];
static struct sock_ctx *cli;
    //always use this for old 3ds
static u32 *magicoffset = 0x0082301C; //Offset on OLD 3ds using SmileBasic with the first array dimmed always being a value of 555555 :)
    //Patched for potentially NEW 3ds.
    
        //Always use this for new 3ds. --manual change REQUIRED
//static u32 *magicoffset = 0x0082B01C;

                          //00823020 FAR crash was at this address o.o
MyThread *smileNetCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&smileNetThread, smileNetThreadMain, smileNetThreadStack, 0x4000, 0x20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &smileNetThread;
}
int smileNetStartResult;
//Callbacks



void write_buffr(char *buf, u32 *startaddr, int len){
 u32 *oldoffset;
oldoffset = startaddr; 
  
 for (int i=0; i<len; i++){
     oldoffset ++;
     u32 etis2 = buf[i]; *oldoffset = (u32)etis2;
 }
 
}

u32 get_value(int index){
    u32 value = ((u32*)magicoffset)[index];
    return value;
}

void write_sendbuffr(char *buf, int len, int startoffset){
    
    for (int i=0;i<len;i++){
        buf[i + startoffset] = (char) get_value(i + startoffset);
        
    }
    
}


void write_value(int index, u32 *startaddr, u32 value){
     u32 *oldoffset;
     oldoffset = startaddr;
     u32 etis2 = value; *oldoffset = (u32)etis2;
}

int sockc_accept(struct sock_ctx *client_ctx)
{
    cli = client_ctx;
    return 0; //body of function, something that uses the client_ctx I guess
}

int sockc_data(struct sock_ctx *client_ctx){
    ssize_t read_size = soc_recv(client_ctx->sockfd, messagebuf, 256, 0);
    
        //First we check if it's a raw message or not
        
        char messagetype = messagebuf[0];
        if (messagetype == 0){
            //we're dealing with a raw message
                //The server should always assume that this thread has responded about the write buffer being writable.
                //So we skip checking and poorly assume it is.
                write_buffr(messagebuf, magicoffset, 256);
                
        }
        
        if (messagetype ==255){
            //this is some kind of message.
                char messageformat = messagebuf[1]; //This gets the message format
                    if (messageformat ==0) {
                        //This checks if the message format is of type can_write request.
                            //get the write flag 
                            int get_index = 256;
                            char can_recieve = 0; //False
                            u32 value = 0;
                            value = get_value(get_index);
                            
                                //format the message response
                                
                               if (value == 255){ can_recieve = 0;} //255 means can't recieve
                               if (value == 0) { can_recieve = 1; } //0 means cleared and can recieve.
                               
                               
                               char sendarray[3];
                               sendarray[0] = 255; //Make it a message type packet
                               sendarray[1] = 1; //response to can_write with true/false
                               sendarray[2] = can_recieve; //Fill it in with the truth
                               soc_send(client_ctx->sockfd, sendarray, sizeof(sendarray), 0);
                    }
                    
                    if (messageformat == 2){
                        //This asks if the smileBasic client has data for the server ready to send.
                         int get_index = 269; //This is equivalent to VARS[269] in a smilenet program and SmileBasic should modify this when it's ready to send data.
                         u32 value = get_value(get_index);
                         char have_data = 0;
                         if (value==0){have_data = 0;}
                         if (value==1){have_data = 1;}
                         
                         char sendarray[3];
                         sendarray[0] = 255; //make it a message packet
                         sendarray[1] = 3; //Response to have data for server type message evaluated as (messageformat) on server side
                         sendarray[2] = have_data;
                         soc_send(client_ctx->sockfd, sendarray, sizeof(sendarray), 0);
                         
                        
                    }
                    
                    if (messageformat ==4){
                        //This is the PC client/Snet server asking Smilenet to hand over the send_buffer.
                            //So now we need to send the data
                                char send_buffer[256];
                                write_sendbuffr(send_buffer, 256,  270);
                                soc_send(client_ctx->sockfd, send_buffer, sizeof(send_buffer), 0);
                                    //Tell SmileBasic we're done sending data
                                        write_value(268, magicoffset, 1); //tells SB safe to re-write send_buffer now;
                        
                    }
                    
                    
            
        }
    
        //Try to write to the buffer this way
    //messagebuf[0] = 'a';
    //messagebuf[1] = 'b';
    //messagebuf[2] = 'c';
   // messagebuf[3] = 'd';
    
   //char send[] = "Hello SmileNet";
    //soc_send(client_ctx->sockfd, send, 16, 0);
    
    
   
    //Draw_DrawFormattedString(10, (10)*11, COLOR_GREEN, "%p", read_size );
    return 0; //I don't know
}

int sockc_close(struct sock_ctx *client_ctx){
    return 0; // I don't know if this will even be valid 
}

struct sock_ctx* sockc_alloc(struct sock_server *serv, u16 port){
static struct sock_ctx ctx;
static struct sock_ctx *stuff = &ctx;
    //static struct sock_ctx ctx;
    //static struct sock_ctx *stuff;
    //stuff = ctx;
    return stuff ;
}


void sockc_free(struct sock_server *serv, struct sock_ctx *ctx){
}

//void nothing(void){
 //do{
     
// Draw_DrawFormattedString(10, (12)*10, COLOR_GREEN, "Press B to continue");

 //} while(! (waitInput() & BUTTON_B));
    
//}



void smileNetThreadMain(void)
{
    Draw_DrawFormattedString(10, (5)*10, COLOR_GREEN, "Starting Everything");
    nothing();
    s64 startAddress, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize, destAddress;
    u32 totalSize;
    Handle processHandle;
    static int memindex = 0;
    u32 memoffset = 0;
    char buf[65];
    
    static u32 value = 0; 
    Draw_DrawFormattedString(10, (6)*10, COLOR_GREEN, "Opening PetitCom Process");
    nothing();
    Result res = OpenProcessByName("petitcom", &processHandle);
     if(R_SUCCEEDED(res))
        
        {
      
        u32 codeStartAddress, heapStartAddress;
        u32 codeDestAddress, heapDestAddress;
        u32 codeTotalSize, heapTotalSize;

        s64 textStartAddress, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize;

        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002);
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);

        svcGetProcessInfo(&textStartAddress, processHandle, 0x10005);

        codeTotalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);
        codeDestAddress = codeStartAddress = (u32)textStartAddress; //should be 0x00100000

        MemInfo mem;
        PageInfo out;

        heapDestAddress = heapStartAddress = 0x08000000;
        svcQueryProcessMemory(&mem, &out, processHandle, heapStartAddress);
        heapTotalSize = mem.size;

       // Result codeRes = svcMapProcessMemoryEx(processHandle, codeDestAddress, codeStartAddress, codeTotalSize);
        Result heapRes = svcMapProcessMemoryEx(processHandle, startAddress, heapStartAddress, heapTotalSize);

        //bool codeAvailable = R_SUCCEEDED(codeRes);
        bool heapAvailable = R_SUCCEEDED(heapRes);
           
           
        
        
                if (R_SUCCEEDED(heapAvailable))
                        {
                             Draw_DrawFormattedString(10, (10)*10, COLOR_GREEN, "Now accessing memory");
                             nothing();
                            value = ((u32*)startAddress)[1];
                            Draw_DrawFormattedString(10, (11)*10, COLOR_GREEN, "Success didn't crash");
                            nothing();
                            FS_Archive archive;
                            FS_ArchiveID archiveId;
                            s64 out;
         
                            void *a;
                            a = &startAddress;
                            //Trying to write file shit
                            u64 total;
                            Result res;
        
                            char filename[64];
                            Result res1, res2, res3;
        
          
                            u32 size = totalSize;
                            size += 0xFFF;
                            size &= 0xFFF;
                            size >>= 12;
                            u32 offs = 0;
                            //u8 buf[0x1000];
                            u32 written;
                            u32 left;
                            int broadcast = 1;
                            u32 targetaddress = startAddress + totalSize;
                            bool repeat = true;
                            value = ((u32*)startAddress)[0];
                            
                             Draw_DrawFormattedString(10, (13)*10, COLOR_GREEN, "Now looking for magic offset");
                             nothing();
                            //Search for 555555, 555555, 555555
                            u8 pattern[] = {0x23, 0x7A, 0x08, 0x00, 0x23, 0x7A, 0x08, 0x00}; 
                            u32 *magicoffset = (u32 *)memsearch((u8 *)startAddress, &pattern, totalSize, sizeof(pattern));
                            Draw_DrawFormattedString(10, (14)*10, COLOR_GREEN, "Offset found == (%p)", magicoffset);
                           nothing();

                            smileNetEnabled = true;
                            svcSignalEvent(smileNetThreadStartedEvent);
                            char buf[100];
                            char strings[100];
                            //u32 oldSpecialButtons = 0, specialButtons = 0;
                            //Draw_DrawFormattedString(10, (10)*11, COLOR_GREEN, "Attempting to listen for a soc" );
                            
                            Draw_DrawFormattedString(10, (15)*10, COLOR_GREEN, "Now starting server");
                            nothing();
                            //Socket Stuff here
                            struct sock_server tcpserver = {0} ;
                            server_init(&tcpserver);
                            tcpserver.accept_cb = sockc_accept;
                            tcpserver.data_cb = sockc_data;
                            tcpserver.close_cb = sockc_close;
                            tcpserver.alloc = sockc_alloc;
                            tcpserver.free = sockc_free;
                            tcpserver.clients_per_server = 1;
                            server_bind(&tcpserver, 4000);
                            server_run(&tcpserver);
                            
                           
                           // server_bind(ptr, port)
                           // <more binds?>, 
                            //server_run(ptr)
                            
                            
                                while(smileNetEnabled && !terminationRequest)
                                        {
                                            //Attempt to patch in the reference to cli that we got from accept 0_o
                                         ssize_t read_size = soc_recv(cli->sockfd, messagebuf, 20, 0);
                                         Draw_DrawFormattedString(10, (10)*11, COLOR_GREEN, "%p", read_size );
                                         if (read_size >0){
                            
                                        u32 *oldoffset;
                                        oldoffset = magicoffset;
                                
                                        for (int i=0;i<255;i++){
                                        oldoffset ++;
                                        u32 etis2 = messagebuf[i]; *oldoffset = (u32)etis2;
                                        }
                                         }
                                
                                        

  
                         }
                        
                        }
        }
}


                        

