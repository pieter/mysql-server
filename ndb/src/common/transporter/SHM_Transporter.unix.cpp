/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>

#include "SHM_Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <TransporterCallback.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>

#include <sys/ipc.h>
#include <sys/shm.h>



bool
SHM_Transporter::connectServer(Uint32 timeOutMillis){
  if(!_shmSegCreated){
    shmId = shmget(shmKey, shmSize, IPC_CREAT | 960);
    if(shmId == -1){
      perror("shmget: ");
      reportThreadError(remoteNodeId, TE_SHM_UNABLE_TO_CREATE_SEGMENT);
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    _shmSegCreated = true;
  }

  if(!_attached){
    shmBuf = (char *)shmat(shmId, 0, 0);
    if(shmBuf == 0){
      perror("shmat: ");
      reportThreadError(remoteNodeId, TE_SHM_UNABLE_TO_ATTACH_SEGMENT);
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    _attached = true;
  }
  
  struct shmid_ds info;
  const int res = shmctl(shmId, IPC_STAT, &info);
  if(res == -1){
    perror("shmctl: ");
    reportThreadError(remoteNodeId, TE_SHM_IPC_STAT);
    NdbSleep_MilliSleep(timeOutMillis);
    return false;
  }
  
  if(info.shm_nattch == 2 && !setupBuffersDone) {
    setupBuffers();
    setupBuffersDone=true;
  }

  if(setupBuffersDone) {
    NdbSleep_MilliSleep(timeOutMillis);
    if(*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }
  

  if(info.shm_nattch > 2){
    reportThreadError(remoteNodeId, TE_SHM_DISCONNECT);
    NdbSleep_MilliSleep(timeOutMillis);
    return false;
  }
  
  NdbSleep_MilliSleep(timeOutMillis);
  return false;
}

bool
SHM_Transporter::connectClient(Uint32 timeOutMillis){
  if(!_shmSegCreated){

    shmId = shmget(shmKey, shmSize, 0);
    if(shmId == -1){
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    _shmSegCreated = true;
  }

  if(!_attached){
    shmBuf = (char *)shmat(shmId, 0, 0);
    if(shmBuf == 0){
      reportThreadError(remoteNodeId, TE_SHM_UNABLE_TO_ATTACH_SEGMENT);
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    _attached = true;
  }

  struct shmid_ds info;

  const int res = shmctl(shmId, IPC_STAT, &info);
  if(res == -1){
    reportThreadError(remoteNodeId, TE_SHM_IPC_STAT);
    NdbSleep_MilliSleep(timeOutMillis);
    return false;
  }
  

  if(info.shm_nattch == 2 && !setupBuffersDone) {
    setupBuffers();
    setupBuffersDone=true;
  }

  if(setupBuffersDone) {
    NdbSleep_MilliSleep(timeOutMillis);
    if(*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }

  if(info.shm_nattch > 2){
    reportThreadError(remoteNodeId, TE_SHM_DISCONNECT);
    NdbSleep_MilliSleep(timeOutMillis);
    return false;
  }

  NdbSleep_MilliSleep(timeOutMillis);
  return false;
}

bool
SHM_Transporter::checkConnected(){
  struct shmid_ds info;
  const int res = shmctl(shmId, IPC_STAT, &info);
  if(res == -1){
    reportError(callbackObj, remoteNodeId, TE_SHM_IPC_STAT);
    return false;
  }
 
  if(info.shm_nattch != 2){
    reportError(callbackObj, remoteNodeId, TE_SHM_DISCONNECT);
    return false;
  }
  return true;
}

void
SHM_Transporter::disconnectImpl(){
  if(_attached){
    const int res = shmdt(shmBuf);
    if(res == -1){
      perror("shmdelete: ");
      return;   
    }
    _attached = false;
    if(!isServer && _shmSegCreated)
      _shmSegCreated = false;
  }
  
  if(isServer && _shmSegCreated){
    const int res = shmctl(shmId, IPC_RMID, 0);
    if(res == -1){
      reportError(callbackObj, remoteNodeId, TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    _shmSegCreated = false;
  }
  setupBuffersDone=false;
}

