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

// NNode.cpp: implementation of the NNode class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "NNode.h"
#include "NAlias.h"
#include "LinkedList.h"
#include "Value.h"
#include "Statement.h"
#include "CompiledStatement.h"
#include "Stream.h"
#include "NBitmap.h"
#include "Collation.h"
#include "Table.h"
#include "Context.h"
#include "Log.h"
#include "PrettyPrint.h"
#include "Connection.h"
#include "IndexKey.h"
#include "BinaryBlob.h"
#include "Bitmap.h"

#define SIGNATURE	0xabcdef01

static const char *nodeNames [] = {
	"Nothing",
	"Insert",
	"Parameter",
	"nField",
	"Select",
	"Add",
	"Subtract",
	"Multiply",
	"Divide",
	"Mod",
	"Negate",
	"Concate",
	"Count",
	"Min",
	"Max",
	"Sum",
	"Avg",
	"Case",
	"CaseSearch",
	"And",
	"Or",
	"Not",
	"Eql",
	"Neq",
	"Gtr",
	"Geq",
	"Lss",
	"Leq",
	"Like",
	"Starting",
	"Containing",
	"Matching",
	"List",
	"Between",
	"Assign",
	"NNull",
	"IsNull",
	"IsActiveRole",
	"NotNull",
	"BitmapNode",
	"BitmapAnd",
	"BitmapOr",
	"Constant",
	"Update",
	"Delete",
	"RecordNumber",
	"BitSet",
	"Alias",
	"NextValue",
	"InSelect",
	"InList",
	"InSelectBitmap",
	"InListBitmap",
	"Exists",
	"SelectExpr",
	"Replace",
	"ConnectionVariable",
	"LogBoolean",
	"Cast",
	"BlobReference",
	"Upper",
	"Lower",
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NNode::NNode(CompiledStatement *statement, NType nType)
{
	init(statement);
	type = nType;
}

NNode::NNode(CompiledStatement *statement, NType nType, int numberChildren)
{
	init(statement);
	type = nType;

	if (numberChildren)
		allocChildren(numberChildren);

	switch (type)
		{
		case Add:
		case Subtract:
		case Multiply:
		case Divide:
		case Mod:
		case Negate:
		case Concat:
		case Case:
		case CaseSearch:
		case NNull:
		case NextValue:
		case Cast:
		case BlobRef:
		case Upper:
		case Lower:
			if (statement)
				valueSlot = statement->getValueSlot();
			break;
		
		default:
			break;
		}
}

NNode::~NNode()
{
	if (statement)
		for (NNode **ptr = &statement->nodeList; *ptr; ptr = &(*ptr)->nextNode)
			if (*ptr == this)
				{
				*ptr = nextNode;
				break;
				}


	delete [] children;
}

Value* NNode::eval(Statement * statement)
{
	switch (type)
		{
		case CaseSearch:
			{
			Value *val = children [0]->eval(statement);
			int n;
			
			for (n = 1; n + 1 < count; n += 2)
				{
				Value *test = children [n]->eval(statement);
				
				if (val->compare(test) == 0)
					return children [n+1]->eval(statement);
				}
				
			if (n < count)
				return children [n]->eval(statement);
				
			Value *value = statement->getValue(valueSlot);
			value->clear();
			
			return value;
			}

		case Case:
			{
			Value *value = statement->getValue(valueSlot);
			value->clear();
			int args = count & ~1;
			
			for (int n = 0; n < args; n += 2)
				if (children [n]->evalBoolean(statement) == TRUE_BOOLEAN)
					return children [n + 1]->eval(statement);
					
			if (count & 1)
				return children [count - 1]->eval(statement);
				
			/*** fall thru to NNull
			Value *value = statement->getValue(valueSlot);
			value->clear();
			return value;
			***/
			}

		case NNull:
			{
			Value *value = statement->getValue(valueSlot);
			value->clear();
			
			return value;
			}

		case Add:
			{
			Value *value = statement->getValue(valueSlot);
			Value *v1 = children [0]->eval(statement);
			Value *v2 = children [1]->eval(statement);
			value->setValue(v1, false);
			value->add(v2);
			
			return value;
			}

		case Subtract:
			{
			Value *value = statement->getValue(valueSlot);
			Value *v1 = children [0]->eval(statement);
			Value *v2 = children [1]->eval(statement);
			value->setValue(v1, false);
			value->subtract(v2);
			
			return value;
			}

		case Multiply:
			{
			Value *value = statement->getValue(valueSlot);
			Value *v1 = children [0]->eval(statement);
			Value *v2 = children [1]->eval(statement);
			value->setValue(v1, false);
			value->multiply(v2);
			
			return value;
			}

		case Divide:
			{
			Value *value = statement->getValue(valueSlot);
			Value *v1 = children [0]->eval(statement);
			Value *v2 = children [1]->eval(statement);
			value->setValue(v1, false);
			value->divide(v2);
			
			return value;
			}

		case Negate:
			{
			Value *value = statement->getValue(valueSlot);
			Value *v1 = children [0]->eval(statement);
			value->setValue((int) 0);
			value->subtract(v1);
			
			return value;
			}

		case Concat:
			{
			Value *value = statement->getValue(valueSlot);
			Value *v1 = children [0]->eval(statement);
			Value *v2 = children [1]->eval(statement);
			char *temp1, *temp2;
			JString string = v1->getString(&temp1);
			string += v2->getString(&temp2);
			delete [] temp1;
			delete [] temp2;
			value->setString(string, true);
			
			return value;
			}

		case BlobRef:
			{
			Value *v1 = children [0]->eval(statement);
			UCHAR temp [256];
			int length;
			
			if (v1->getType() == ClobPtr) 
				{
				Clob *clob = v1->getClob();
				length = clob->getReference(sizeof(temp), temp);
				clob->release();
				}
			else
				{
				Blob *blob = v1->getBlob();
				length = blob->getReference(sizeof(temp), temp);
				blob->release();
				}

			BinaryBlob *newBlob = new BinaryBlob();
			newBlob->putSegment(length, temp);
			Value *value = statement->getValue(valueSlot);
			value->setValue(newBlob);
			newBlob->release();

			return value;
			}

		case Upper:
			{
			Value *v1 = children [0]->eval(statement);
			Value *value = statement->getValue(valueSlot);
			char *temp;
			value->setString(JString::upcase(v1->getString(&temp)), true);
			delete [] temp;

			return value;
			}
			break;
			
		default:
			ASSERT(false);
		}

	return NULL;
}

NNode** NNode::allocChildren(int number)
{
	count = number;
	children = new NNode* [count];
	memset(children, 0, sizeof(NNode*) * number);

	return children;
}

void NNode::setChild(int index, NNode * node)
{
	children [index] = node;
}

void NNode::decomposeConjuncts(LinkedList & conjuncts)
{
	if (type == And)
		{
		for (int n = 0; n < count; ++n)
			children [n]->decomposeConjuncts(conjuncts);
		}
	else
		conjuncts.append(this);
}

bool NNode::computable(CompiledStatement * statement)
{
	for (int n = 0; n < count; ++n)
		if (!children [n]->computable(statement))
			return false;

	return true;
}

void NNode::matchIndex(CompiledStatement * statement, Context * context, Index * index, NNode * * min, NNode * * max)
{
	switch (type)
		{
		case Eql:
		case Gtr:
		case Geq:
		case Lss:
		case Leq:
		case Between:
			break;

		case And:
			children [0]->matchIndex(statement, context, index, min, max);
			children [1]->matchIndex(statement, context, index, min, max);
			
			return;

		default:
			return;
		}

	int slot = -1;
	int field = 0;

	for (int n = 0; n < count; ++n)
		if (!children [n]->computable(statement))
			{
			if (slot >= 0)
				return;
				
			slot = children [n]->matchField(context, index);
			
			if (slot < 0)
				return;
				
			field = n;
			}

	if (slot < 0)
		return;

	NNode *node = (field == 0) ? children [1] : children [0];

	switch (type)
		{
		case Eql:
			max [slot] = min [slot] = node;
			break;

		case Gtr:
		case Geq:
			if (field == 0)
				min [slot] = node;
			else
				max [slot] = node;
				
			break;

		case Leq:
		case Lss:
			if (field == 0)
				max [slot] = node;
			else
				min [slot] = node;
				
			break;

		case Between:
			if (field == 0)
				{
				min [slot] = children [1];
				max [slot] = children [2];
				}
			else if (field == 1)
				max [slot] = children [0];
			else if (field == 2)
				min [slot] = children [0];
				
			break;
		
		default:
			break;
		}
}

int NNode::matchField(Context * context, Index * index)
{
	return -1;
}

Field* NNode::getField()
{
	return NULL;
}

bool NNode::isMember(Table * table)
{
	for (int n = 0; n < count; ++n)
		if (children [n]->isMember(table))
			return true;

	return false;
}

bool NNode::references(Table * table)
{
	for (int n = 0; n < count; ++n)
		if (children [n]->references(table))
			return true;

	return false;
}


const char* NNode::getName()
{
	return "";
}

bool NNode::like(const char * string, const char * pattern)
{
	char c;
	const char *s = string;

	for (const char *p = pattern; (c = *p++); ++s)
		if (c == '%')
			{
			if (!*p)
				return true;
				
			for (; *s; ++s)
				if (like(s, pattern+1))
					return true;
					
			return false;
			}
		else if (!*s)
			return false;
		else if (c != '_' && c != *s)
			return false;

	return (!c && !*s);
}


void NNode::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent(level++);
	pp->putLine(nodeNames [type]);

	for (int n = 0; n < count; ++n)
		printNode(level, children [n], pp);
}


void NNode::printNode(int level, NNode *child, PrettyPrint *pp)
{
	if (child)
		child->prettyPrint(level, pp);
	else
		{
		pp->indent(level);
		pp->put("*** null ***\n");
		}
}

void NNode::init(CompiledStatement *stmt)
{
	statement = stmt;
	children = NULL;
	collation = NULL;
	count = 0;

	if (statement)
		{
		nextNode = statement->nodeList;
		statement->nodeList = this;
		}
	else
		nextNode = NULL;
}

NNode** NNode::copyVector(int numberNodes, NNode * * nodes)
{
	if (!nodes)
		return NULL;

	NNode** newNodes = new NNode* [numberNodes];

	for (int n = 0; n < numberNodes; ++n)
		newNodes [n] = nodes [n];

	return newNodes;
}

bool NNode::containing(const char * string, const char * pattern)
{
	char c;
	char first = UPPER(*pattern);

    for (const char *str = string; (c = *str++);)
		if (UPPER(c) == first)
			{
			char sc, pc;
			
			for (const char *s = str, *p = pattern + 1;
				 (pc = *p++) && (sc = *s++) && (UPPER(sc) == UPPER(pc));)
				;
				
			if (!pc)
				return true;
			}

	return false;
}

void NNode::gen(Stream * stream)
{
	const char *op;
	bool paren = false;

	switch (type)
		{
		case Eql:	op = "="; break;
		case Neq:	op = "<>"; break;
		case Gtr:	op = ">"; break;
		case Geq:	op = ">="; break;
		case Leq:	op = "<="; break;
		case Lss:	op = "<"; break;
		case Like:	op = " like "; break;
		case Containing:	op = " containing "; break;
		case Starting:		op = " starting "; break;

		case And:
			op = " and "; 
			break;

		case Or:
			op = " or "; 
			paren = true;
			break;

		case List:
			op = ",";
			break;

		case Concat:
			op = " || ";
			break;

		case CaseSearch:
			{
			stream->putSegment("case ");
			children [0]->gen(stream);
			int n;
			
			for (n = 1; n + 1 < count; n += 2)
				{
				stream->putCharacter(' ');
				children [n]->gen(stream);
				stream->putSegment(" then ");
				children [n + 1]->gen(stream);
				}
				
			if (n < count)
				{
				stream->putSegment(" else ");
				children [n]->gen(stream);
				}
				
			stream->putSegment(" end");
			
			return;
			}

		case NNull:
			children [0]->gen(stream);
			stream->putSegment("is null");
			
			return;

		default:
			ASSERT(false);
		}

	if (paren)
		stream->putCharacter('(');

	for (int n = 0; n < count; ++n)
		{
		if (n != 0)
			stream->putSegment(op);
		children [n]->gen(stream);
		}

	if (paren)
		stream->putCharacter(')');
}

FieldType NNode::getType()
{
	FieldType returnType;

	switch (type)
		{
		case Concat:
			{
			returnType.type = Varchar;
			returnType.scale = 0;
			returnType.precision = 0;
			returnType.length = 0;
			
			for (int n = 0; n < count; ++n)
				{
				FieldType childType = children [n]->getType();
				returnType.length += childType.length;
				returnType.precision += childType.precision;
				}
			}
			break;

		default:
			returnType.type = Varchar;
			returnType.scale = 0;
			returnType.precision = 0;
			returnType.length = 0;
			NOT_YET_IMPLEMENTED;
		}

	return returnType;
}

NNode* NNode::copy(CompiledStatement * statement, Context * context)
{
	NNode *node = new NNode(statement, type, count);

	for (int n = 0; n < count; ++n)
		node->children [n] = children [n]->copy(statement, context);

	return node;
}

void NNode::reset(Statement *statement)
{

}

bool NNode::isStatistical()
{
	switch (type)
		{
		case Add:
		case Subtract:
		case Multiply:
		case Divide:
		case Mod:
		case Negate:
			break;

		default:
			return false;
		}

	for (int n = 0; n < count; ++n)
		if (!children[n]->isStatistical())
			return false;

	return true;
}

void NNode::increment(Statement * statement)
{
	switch (type)
		{
		case Add:
		case Subtract:
		case Multiply:
		case Divide:
		case Mod:
		case Negate:
			{
			for (int n = 0; n < count; ++n)
				children[n]->increment(statement);
			}
			break;
		
		default:
			break;
		}

}

NNode* NNode::makeInversion(CompiledStatement *statement, Context *context, Index *index)
{
	if (type == Starting)
		{
		int n = children [0]->matchField(context, index);
		
		if (n != 0)
			return NULL;
			
		if (!children [1]->computable(statement))
			return NULL;
			
		NBitmap *bitmap = new NBitmap(statement, index, 1, children + 1, children + 1);
		bitmap->partial = true;
		
		return bitmap;
		}

	return NULL;
}

bool NNode::equiv(NNode *node)
{
	if (type != node->type || count != node->count)
		return false;

	for (int n = 0; n < count; ++n)
		if (!children [n]->equiv(node->children [n]))
			return false;

	return true;
}

bool NNode::isMember(NNode *node)
{
	if (node->type == ValueAlias)
		node = ((NAlias*) node)->expr;

	for (int n = 0; n < count; ++n)
		if (children [n]->equiv(node))
			return true;

	return false;
}

Bitmap* NNode::evalInversion(Statement *statement)
{
	switch (type)
		{
		case BitmapAnd:
			{
			Bitmap *bitmap = children [0]->evalInversion(statement);
			
			for (int n = 1; n < count; ++n)
				bitmap->andBitmap(children [n]->evalInversion(statement));
				
			return bitmap;
			}

		case BitmapOr:
			{
			Bitmap *bitmap = children [0]->evalInversion(statement);
			
			for (int n = 1; n < count; ++n)
				bitmap->orBitmap(children [n]->evalInversion(statement));
				
			return bitmap;
			}

		default:
			ASSERT(false);
		}

	return NULL;
}

int NNode::evalBoolean(Statement *statement)
{
	switch (type)
		{
		case Eql:
		case Neq:
		case Gtr:
		case Geq:
		case Leq:
		case Lss:
			{
			Value *v1;
			Value *v2;
			v1 = children [0]->eval(statement);

			if (v1->isNull())
				return NULL_BOOLEAN;

			v2 = children [1]->eval(statement);

			if (v2->isNull())
				return NULL_BOOLEAN;

			int comparison = (collation) ? collation->compare (v1, v2) : v1->compare(v2);

			switch (type)
				{
				case Eql:	return (comparison == 0);
				case Neq:	return (comparison != 0);
				case Gtr:	return (comparison > 0);
				case Geq:	return (comparison >= 0);
				case Leq:	return (comparison <= 0);
				case Lss:	return (comparison < 0);
				default:
					break;
				}
			}
			break;

		case And:
			{
			int ret = TRUE_BOOLEAN;
			
			for (int n = 0; n < count; ++n)
				{
				int result = children [n]->evalBoolean(statement);

				if (result == NULL_BOOLEAN)
					ret = result;
				else if (result == FALSE_BOOLEAN)
					return result;
				}
				
			return ret;
			}

		case Or:
			{
			int ret = FALSE_BOOLEAN;

			for (int n = 0; n < count; ++n)
				{
				int result = children [n]->evalBoolean(statement);
				
				if (result == NULL_BOOLEAN)
					ret = result;
				else if (result == TRUE_BOOLEAN)
					return result;
				}
				
			return ret;
			}

		case Not:
			{
			int ret = children [0]->evalBoolean(statement);

			if (ret == NULL_BOOLEAN)
				return ret;

			if (ret == FALSE_BOOLEAN)
				return TRUE_BOOLEAN;

			return FALSE_BOOLEAN;
			}

		case LogBoolean:
			{
			int ret = children [1]->evalBoolean(statement);
			
			if (statement->connection->traceFlags & traceBooleans)
				{
				Value *value = children [0]->eval(statement);
				char *temp;
				const char *string = value->getString(&temp);
				Log::debug("boolean %s: %s\n", string,
							 (ret == TRUE_BOOLEAN) ? "true" :
							 (ret == FALSE_BOOLEAN) ? "false" : "null");
				delete [] temp;
				}
				
			return ret;
			}

		case Like:
		case Containing:
		case Starting:
			{
			Value *v1 = children [0]->eval(statement);

			if (v1->isNull())
				return NULL_BOOLEAN;

			Value *v2 = children [1]->eval(statement);
			
			if (v2->isNull())
				return NULL_BOOLEAN;
				
			char *str1, *str2;
			const char *string1 = v1->getString(&str1);
			const char *string2 = v2->getString(&str2);
			bool ret = false;
			
			switch (type)
				{
				case Like:
					if (collation)
						ret = collation->like(string1, string2);
					else
						ret = like(string1, string2);
					break;

				case Containing:
					ret = containing(string1, string2);
					break;

				case Starting:
					if (collation)
						ret = collation->starting(string1, string2);
					else
						{
						ret = true;

						for (const char *p = string1, *q = string2; *q;)
							if (*p++ != *q++)
								{
								ret = false;
								break;
								}
						}
					break;
				
				default:
					ASSERT(false);
				}

			delete [] str1;
			delete [] str2;

			return (ret) ? TRUE_BOOLEAN : FALSE_BOOLEAN;
			}

		case IsNull:
			{
			Value *v1 = children [0]->eval(statement);
			
			return (v1->isNull()) ? TRUE_BOOLEAN : FALSE_BOOLEAN;
			}

		case IsActiveRole:
			{
			Value *v1 = children [0]->eval(statement);
			char *str1;
			const char *string1 = v1->getString(&str1);
			
			if (!string1 [0])
				return TRUE_BOOLEAN;
				
			int ret = statement->connection->hasActiveRole(string1);
			delete [] str1;
				
			return (ret) ? TRUE_BOOLEAN : FALSE_BOOLEAN;
			}

		case NotNull:
			{
			Value *v1 = children [0]->eval(statement);
			
			return (v1->isNull()) ? FALSE_BOOLEAN : TRUE_BOOLEAN;
			}

		default:
			ASSERT(false);
		}

	return FALSE_BOOLEAN;
}

void NNode::evalStatement(Statement *statement)
{
	ASSERT(false);
}

Collation* NNode::getCollation()
{
	for (int n = 0; n < count; ++n)
		{
		Collation *collation = children [n]->getCollation();
		if (collation)
			return collation;
		}

	return NULL;
}

void NNode::fini()
{
	switch (type)
		{
		case Eql:
		case Neq:
		case Gtr:
		case Geq:
		case Lss:
		case Leq:
		case Between:
		case Starting:
		case Like:
			collation = getCollation();
			break;

		default:
			break;
		}
}

bool NNode::isRedundantIndex(LinkedList *indexes)
{
	return false;
}

bool NNode::isInvertible(CompiledStatement *statement, Context *context)
{
	switch (type)
		{
		case Or:
			{
			for (int n = 0; n < count; ++n)
				if (!statement->isInvertible(context, children [n]))
					return false;
					
			return true;
			}

		case And:
			{
			for (int n = 0; n < count; ++n)
				if (statement->isInvertible(context, children [n]))
					return true;
					
			return false;
			}

		case Eql:
			return statement->isDirectReference(context, this) != NULL;

		case Starting:
			{
			FOR_INDEXES(index, context->table)
				NNode *min [MAX_KEY_SEGMENTS];
				memset(min, 0, sizeof(min));
				NNode *max [MAX_KEY_SEGMENTS];
				memset(max, 0, sizeof(max));
				children [0]->matchIndex(statement, context, index, min, max);
				
				if(min [0] || max [0])
					return true;
			END_FOR;
			
			return false;
			}

		default:
			break;
		}

	return false;
}

void NNode::open(Statement *statement)
{
	for (int n = 0; n < count; ++n)
		children [n]->open (statement);
}

void NNode::close(Statement *statement)
{
	for (int n = 0; n < count; ++n)
		children [n]->close (statement);
}

bool NNode::isUniqueIndex()
{
	return false;
}
