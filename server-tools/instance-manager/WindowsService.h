/*
   Copyright (C) 2006 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#pragma once

class WindowsService
{
protected:
  bool                  inited;
  const char            *serviceName;
  const char            *displayName;
  const char            *username;
  const char            *password;
  SERVICE_STATUS_HANDLE statusHandle;
  DWORD                 statusCheckpoint;
  SERVICE_STATUS        status;
  DWORD                 dwAcceptedControls;
  bool                  debugging;

public:
  WindowsService(void);
  ~WindowsService(void);

  BOOL  Install();
  BOOL  Remove();
  BOOL  Init();
  BOOL  IsInstalled();
  void  SetAcceptedControls(DWORD acceptedControls);
  void  Debug(bool debugFlag) { debugging= debugFlag; }

public:
  static void WINAPI    ServiceMain(DWORD argc, LPTSTR *argv);
  static void WINAPI    ControlHandler(DWORD CtrlType);

protected:
  virtual void Run(DWORD argc, LPTSTR *argv)= 0;
  virtual void Stop()                 {}
  virtual void Shutdown()             {}
  virtual void Pause()                {}
  virtual void Continue()             {}
  virtual void Log(const char *msg)   {}

  BOOL ReportStatus(DWORD currentStatus, DWORD waitHint= 3000, DWORD dwError=0);
  void HandleControlCode(DWORD opcode);
  void RegisterAndRun(DWORD argc, LPTSTR *argv);
};
