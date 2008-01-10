/**
   @file
 
   This file defines the API for the following object services:
     - getting CREATE statements for objects
     - generating GRANT statments for objects
     - enumerating objects
     - finding dependencies for objects
     - executor for SQL statments
     - wrappers for controlling the DDL Blocker

  The methods defined below are used to provide server functionality to
  and permitting an isolation layer for the client (caller).
 */ 

#include "si_objects.h"
