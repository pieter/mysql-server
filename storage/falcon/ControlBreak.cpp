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

// ControlBreak.cpp: implementation of the ControlBreak class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ControlBreak.h"
#include "Template.h"
#include "TemplateContext.h"
#include "TemplateAggregate.h"
#include "TemplateParse.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ControlBreak::ControlBreak(Template *tmpl, TemplateContext *templateContext)
{
	pTemplate = tmpl;
	context = templateContext;
	aggregates = NULL;
}

ControlBreak::~ControlBreak()
{
	for (TemplateAggregate *aggregate; aggregate = aggregates;)
		{
		aggregates = aggregate->next;
		delete aggregate;
		}
}

void ControlBreak::reset()
{
	for (TemplateAggregate *aggregate = aggregates; aggregate; aggregate = aggregate->next)
		aggregate->reset();
}

void ControlBreak::increment()
{
	for (TemplateAggregate *aggregate = aggregates; aggregate; aggregate = aggregate->next)
		aggregate->increment();
}

void ControlBreak::findAggregates()
{
	TemplateParse *parse = context->parse;
	TemplateParse scope;
	scope.setText (length, start);
	context->parse = &scope;

	while (scope.nextTag (context->scripting))
		if (scope.tag [0] == '%')
			{
			const char *expr = scope.args [0];
			const char *p = Template::findTail ("TOTAL(", expr);
			if (p)
				{
				TemplateAggregate *aggregate = new TemplateAggregate (this, p);
				aggregate->next = aggregates;
				aggregates = aggregate;
				}
			}
		else if (scope.substitution)
			for (int n = 0; n < scope.numberArgs; ++n)
				{
				const char *value = scope.values [n];
				if (value)
					{
					const char *p = Template::findTail ("%TOTAL(", value);
					if (p)
						{
						TemplateAggregate *aggregate = new TemplateAggregate (this, p);
						aggregate->next = aggregates;
						aggregates = aggregate;
						}
					}
				}

	context->parse = parse;
}

bool ControlBreak::getValue(const char *expr, Value *value)
{
	for (TemplateAggregate *aggregate = aggregates; aggregate; aggregate = aggregate->next)
		if (aggregate->string == expr)
			{
			aggregate->getValue (value);
			return true;
			}

	return false;			
}

void ControlBreak::expand()
{
	TemplateParse *parse = context->parse;
	TemplateParse scope;
	context->parse = &scope;
	context->controlBreak = this;
	scope.setText (length, start);
	pTemplate->expand (context);
	context->controlBreak = NULL;
	context->parse = parse;
}
