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

#include "Config.hpp"
#include <ctype.h>
#include <string.h>
#include "MgmtErrorReporter.hpp"
#include <Properties.hpp>

//*****************************************************************************
//  Ctor / Dtor
//*****************************************************************************

Config::Config() {
  m_oldConfig = 0;
  m_configValues = 0;
}

Config::~Config() {
  if(m_configValues != 0){
    free(m_configValues);
  }

  if(m_oldConfig != 0)
    delete m_oldConfig;
}

/*****************************************************************************/

void 
Config::printAllNameValuePairs(NdbOut &out,
			       const Properties *prop,
			       const char* s) const {
  Properties::Iterator it(prop);
  const Properties * section = m_info.getInfo(s);
  for (const char* n = it.first(); n != NULL; n = it.next()) {
    Uint32 int_value;
    const char* str_value;
    Uint64 int_64;

    if(!section->contains(n))
      continue;
    if (m_info.getStatus(section, n) == ConfigInfo::CI_INTERNAL) 
      continue;
    if (m_info.getStatus(section, n) == ConfigInfo::CI_DEPRICATED)
      continue;
    if (m_info.getStatus(section, n) == ConfigInfo::CI_NOTIMPLEMENTED)
      continue;

    out << n << ": ";

    switch (m_info.getType(section, n)) {
    case ConfigInfo::CI_INT:
      MGM_REQUIRE(prop->get(n, &int_value)); 
      out << int_value;
      break;

    case ConfigInfo::CI_INT64:
      MGM_REQUIRE(prop->get(n, &int_64)); 
      out << int_64;
      break;
      
    case ConfigInfo::CI_BOOL:
      MGM_REQUIRE(prop->get(n, &int_value)); 
      if (int_value) {
	out << "Y";
      } else {
	out << "N";
      }
      break;
    case ConfigInfo::CI_STRING:
      MGM_REQUIRE(prop->get(n, &str_value)); 
      out << str_value;
      break;
    case ConfigInfo::CI_SECTION:
      out << "SECTION";
      break;
    }      
    out << endl;
  }
}

/*****************************************************************************/
   
void Config::printConfigFile(NdbOut &out) const {
#if 0
  Uint32 noOfNodes, noOfConnections, noOfComputers;
  MGM_REQUIRE(get("NoOfNodes", &noOfNodes));
  MGM_REQUIRE(get("NoOfConnections", &noOfConnections));
  MGM_REQUIRE(get("NoOfComputers", &noOfComputers));

  out << 
    "######################################################################" <<
    endl <<
    "#" << endl <<
    "#  NDB Cluster  System configuration" << endl <<
    "#" << endl <<
    "######################################################################" <<
    endl << 
    "# No of nodes (DB, API or MGM):  " << noOfNodes << endl <<
    "# No of connections:             " << noOfConnections << endl <<
    "######################################################################" <<
    endl;

  /**************************
   * Print COMPUTER configs *
   **************************/
  const char * name;
  Properties::Iterator it(this);
  for(name = it.first(); name != NULL; name = it.next()){
    if(strncasecmp("Computer_", name, 9) == 0){ 
      
      const Properties *prop;
      out << endl << "[COMPUTER]" << endl;
      MGM_REQUIRE(get(name, &prop));
      printAllNameValuePairs(out, prop, "COMPUTER");
      
      out << endl <<
	"###################################################################" <<
	endl;

    } else if(strncasecmp("Node_", name, 5) == 0){
      /**********************
       * Print NODE configs *
       **********************/
      const Properties *prop;
      const char *s;

      MGM_REQUIRE(get(name, &prop));
      MGM_REQUIRE(prop->get("Type", &s));
      out << endl << "[" << s << "]" << endl;
      printAllNameValuePairs(out, prop, s);

      out << endl <<
	"###################################################################" <<
	endl;
    } else if(strncasecmp("Connection_", name, 11) == 0){
      /****************************
       * Print CONNECTION configs *
       ****************************/
      const Properties *prop;
      const char *s;

      MGM_REQUIRE(get(name, &prop));
      MGM_REQUIRE(prop->get("Type", &s));
      out << endl << "[" << s << "]" << endl;
      printAllNameValuePairs(out, prop, s);
      
      out << endl <<
	"###################################################################" <<
	endl;
    } else if(strncasecmp("SYSTEM", name, strlen("SYSTEM")) == 0) {
      /************************
       * Print SYSTEM configs *
       ************************/
      const Properties *prop;

      MGM_REQUIRE(get(name, &prop));
      out << endl << "[SYSTEM]" << endl;
      printAllNameValuePairs(out, prop, "SYSTEM");
      
      out << endl <<
	"###################################################################" <<
	endl;
    }
  }
#endif
}

Uint32
Config::getGenerationNumber() const {
#if 0
  Uint32 ret;
  const Properties *prop = NULL;

  get("SYSTEM", &prop);

  if(prop != NULL)
    if(prop->get("ConfigGenerationNumber", &ret))
      return ret;
  
  return 0;
#else
  return 0;
#endif
}

int
Config::setGenerationNumber(Uint32 gen) {
#if 0
  Properties *prop = NULL;

  getCopy("SYSTEM", &prop);

  if(prop != NULL) {
    MGM_REQUIRE(prop->put("ConfigGenerationNumber", gen, true));
    MGM_REQUIRE(put("SYSTEM", prop, true));
    return 0;
  }
  return -1;
#else
  return -1;
#endif
}

bool
Config::change(const BaseString &section,
	       const BaseString &param,
	       const BaseString &value) {
#if 0
  const char *name;
  Properties::Iterator it(this);

  for(name = it.first(); name != NULL; name = it.next()) {
    Properties *prop = NULL;
    if(strcasecmp(section.c_str(), name) == 0) {
      getCopy(name, &prop);
      if(prop == NULL) /* doesn't exist */
	return false;
      if(value == "") {
	prop->remove(param.c_str());
	put(section.c_str(), prop, true);
      } else {
	PropertiesType t;
	if(!prop->getTypeOf(param.c_str(), &t)) /* doesn't exist */
	  return false;
	switch(t) {
	case PropertiesType_Uint32:
	  long val;
	  char *ep;
	  errno = 0;
	  val = strtol(value.c_str(), &ep, 0);
	  if(value.length() == 0 || *ep != '\0')  /* not a number */
	    return false;
	  if(errno == ERANGE)
	    return false;
	  prop->put(param.c_str(), (unsigned int)val, true);
	  put(section.c_str(), prop, true);
	  break;
	case PropertiesType_char:
	  prop->put(param.c_str(), value.c_str(), true);
	  put(section.c_str(), prop, true);
	  break;
	default:
	  return false;
	}
      }
      break;
    }
  }
  return true;
#else
  return false;
#endif
}
