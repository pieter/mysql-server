/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// ControlBreak.h: interface for the ControlBreak class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONTROLBREAK_H__7BF379C1_6818_11D5_8977_B04496000000__INCLUDED_)
#define AFX_CONTROLBREAK_H__7BF379C1_6818_11D5_8977_B04496000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum BreakType {
	OnTop,
	OnBottom,
	};

class TemplateContext;
class Template;
class TemplateAggregate;
class Value;

class ControlBreak  
{
public:
	void expand();
	bool getValue (const char *expr, Value *value);
	void findAggregates();
	void increment();
	void reset();
	ControlBreak(Template *tmpl, TemplateContext *templateContext);
	virtual ~ControlBreak();

	Template		*pTemplate;
	TemplateContext	*context;
	JString			valueString;
	JString			value;
	const char		*start;
	int				length;
	BreakType		type;
	ControlBreak	*next;
	TemplateAggregate	*aggregates;
};

#endif // !defined(AFX_CONTROLBREAK_H__7BF379C1_6818_11D5_8977_B04496000000__INCLUDED_)
