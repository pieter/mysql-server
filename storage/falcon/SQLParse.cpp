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

/*
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			SQLParse.c
 *	DESCRIPTION:	SQL Parser class code
 *
 * copyright (c) 1997 by James A. Starkey
 */

#include "Engine.h"
#include "string.h"
#include "stdio.h"
#include "SQLParse.h"
#include "LinkedList.h"
//#include "Database.h"
//#include "Table.h"
//#include "Field.h"
#include "SQLError.h"
#include "SymbolManager.h"
#include "Log.h"

//#define DEBUG
#define DIGIT(c)			(c >= '0' && c <= '9')

static char		characters [256];

typedef struct priv {
    const char	*string;
	SyntaxType	type;
	} PRIV;

static const PRIV privileges [] = {
	"SELECT", nod_priv_select,
	"INSERT", nod_priv_insert,
	"UPDATE", nod_priv_update,
	"DELETE", nod_priv_delete,
	"EXECUTE", nod_priv_execute,
	"ALTER", nod_priv_alter,
	"GRANT", nod_priv_grant,
	"ALL", nod_priv_all,
	NULL, nod_priv_all
	};


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


SQLParse::SQLParse ()
{
/**************************************
 *
 *		S Q L P a r s e
 *
 **************************************
 *
 * Functional description
 *		Construct object.
 *
 **************************************/

	const char *p;

	for (p = PUNCTUATION_CHARS; *p; ++p)
		characters [(int) *p] |= PUNCT;

	for (p = WHITE_SPACE; *p; ++p)
		characters [(int) *p] |= WHITE;

	for (p = MULTI_CHARS; *p; ++p)
		characters [(int) *p] |= MULTI_CHAR;

	length = pos = 0;
	nodes = NULL;
	syntax = NULL;
	upcase = true;
	trace = false;
	//java = false;
}

SQLParse::~SQLParse ()
{
/**************************************
 *
 *		~ S Q L P a r s e
 *
 **************************************
 *
 * Functional description
 *		Destroy object.
 *
 **************************************/

	deleteNodes();
}

void SQLParse::deleteNodes ()
{
/**************************************
 *
 *		d e l e t e N o d e s
 *
 **************************************
 *
 * Functional description
 *		Get rid of nodes.
 *
 **************************************/

	syntax = NULL;

	for (Syntax *node; (node = nodes);)
		{
		nodes = node->next;
		delete node;
		}
}

Syntax *SQLParse::error (const char* expected)
{
/**************************************
 *
 *		e r r o r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	char	line [512], pointer [512], *l = line, *p = pointer;
	char	*endLine = line + sizeof (line) - 1;
	const char	*end = string + tokenStart;
	int lineNumber = 1;
	const char *s;

	for (s = string; s < end && l < endLine && p < pointer;)
		{
		char c = *s++;
		if (c == '\n')
			{
			l = line;
			p = pointer;
			++lineNumber;
			}
		else
			{
			*l++ = c;
			*p++ = (c == '\t') ? c : ' ';
			}
		}

	while (*s && *s != '\n' && l < endLine)
		*l++ = *s++;

	*l = 0;
	*p = 0;

	JString msg;
	msg.Format ("syntax error on line %d\n%s\n%s^ expected %s got %s\n", 
				lineNumber, line, pointer, expected, token);
	throw SQLEXCEPTION (SYNTAX_ERROR, (const char*) msg);

	return NULL;
}

bool SQLParse::keyword (const char* s)
{
/**************************************
 *
 *		k e y w o r d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return ((tokenType == NAME ||tokenType == PUNCTUATION_CHAR) &&	
		!strcmp (s, token));
}

void SQLParse::makeToken (int from, int to)
{
/**************************************
 *
 *		m a k e T o k e n
 *
 **************************************
 *
 * Functional description
 *		Copy token from line to token.
 *
 **************************************/

strncpy (token, string + from, to - from);
token [to - from] = 0;
}

bool SQLParse::match (const char *s)
{
/**************************************
 *
 *		m a t c h
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!(tokenType == NAME || tokenType == PUNCTUATION_CHAR) ||
	strcmp (s, token) != 0)
	return false;

nextToken();

return true;
}
			
void SQLParse::nextToken ()
{
/**************************************
 *
 *		n e x t _ t o k e n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	getToken();

	if (trace)
		Log::debug ("token (%d) %s\n", tokenType, token);
}

void SQLParse::getToken ()
{
/**************************************
 *
 *		g e t T o k e n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

char 	c = skipWhite();
tokenStart = pos - 1;

if (c == 0)
	{
	strcpy (token, "end-of-command");
	tokenType = END;
	tokenStart = pos;
	return;
	}

char next = string [pos];

if (ipAddress && c >= '0' && c <= '9')
	{
	tokenType = IP_ADDRESS;
	for (; pos < length; ++pos)
		{
		c = string [pos];
		if (!(c >= '0' && c <= '9') && c != '.')
			break;
		}
	}
else if ((c >= '0' && c <= '9') || 
	(c == '-' && (DIGIT (next) || next == '.')) ||
	(c == '.' && DIGIT (next)))
	{
	tokenType = (c == '.') ? DECIMAL_NUMBER : NUMBER;
	for (; pos < length; ++pos)
		{
		c = string [pos];
		if (c == '.')
			{
			if (tokenType != NUMBER)
				break;
			tokenType = DECIMAL_NUMBER;
			}
		else if (!(c >= '0' && c <= '9'))
			break;
		}
	}
else if (characters [(int) c] & PUNCT)
	{
	tokenType = PUNCTUATION_CHAR;
	if (c == '|' && string [pos] == '|')
		++pos;
	}	
else if (c == '"' || c == '\'')
	{
	int quote = c;
	char *t = token;
	for (; pos < length; ++pos)
		{
		c = string [pos];
		if (c == quote)
			if (pos+1 < length && string [pos+1] == quote)
				++pos;
			else
				break;
		*t++ = c;
		}
	*t = 0;
	if (pos == length)
		error ("unterminated quoted string");
	++pos;
	tokenType = (c == '\'') ? QUOTED_STRING : QUOTED_NAME;
	return;
	}
else
	{
	for (; pos < length; ++pos)
		{
		c = string [pos];
		if (characters [(int) c] & TERM)
			break;
		}
	makeToken (tokenStart, pos);
	if (upcase)
		for (char *p = token; *p; ++p)
			if (*p >= 'a' && *p <= 'z')
				*p += 'A' - 'a';
	tokenType = NAME;
	return;
	}

makeToken (tokenStart, pos);
}

Syntax *SQLParse::makeNode(SyntaxType type, int count)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type, count);
registerNode (syntax);

return syntax;
}

Syntax *SQLParse::makeNode(SyntaxType type)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type);
registerNode (syntax);

return syntax;
}

Syntax *SQLParse::makeNode(SyntaxType type, Syntax *child)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type, child);
registerNode (syntax);

return syntax;
}

Syntax *SQLParse::makeNode(SyntaxType type, Syntax *child1, Syntax *child2)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type, child1, child2);
registerNode (syntax);

return syntax;
}

Syntax *SQLParse::makeNode(SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type, child1, child2, child3);
registerNode (syntax);

return syntax;
}
Syntax *SQLParse::makeNode(SyntaxType type, const char *val)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type, val);
registerNode (syntax);

return syntax;
}

Syntax *SQLParse::makeNode(SyntaxType type, LinkedList &list)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (type, list);
registerNode (syntax);

return syntax;
}

Syntax *SQLParse::makeNode(LinkedList &list)
{
/**************************************
 *
 *		m a k e N o d e
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

Syntax *syntax = new Syntax (list);
registerNode (syntax);

return syntax;
}

Syntax* SQLParse::parse (JString sqlStr, SymbolManager *symbols)
{
/**************************************
 *
 *		p a r s e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/


	string = sqlStr;
	sqlString = sqlStr;
	length = (int) strlen (string);
	//database = db;
	symbolManager = symbols;
	input = NULL;
	pos = 0;
	ipAddress = false;
	nextToken();
	//Log::debug ("Parse: %s\n", string);

	if (trace)
		Log::debug ("Parse: %s\n", string);

	try
		{
		syntax = parseStatement ();
		match (";");
		if (tokenType != END)
			{
			syntax = NULL;
			error ("end of statement");
			}
		}
	catch (...)
		{
		deleteNodes();
		throw;
		//return sqlStr;
		}

	return syntax;
}

Syntax* SQLParse::parseAlterNameSpace()
{
/**************************************
 *
 *		p a r s e A l t e r N a m s p a c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (match ("SET"))
	{
	LinkedList names;
	for (;;)
		{
		names.append (parseName());
		if (!match (","))
			break;
		}
   return makeNode(nod_set_namespace, makeNode(names));
   }

if (match ("PUSH"))
   return makeNode(nod_push_namespace, parseName());

if (match ("POP"))
   return makeNode(nod_pop_namespace);

return error ("set, push, or pop");
}

Syntax *SQLParse::parseAlterTable ()
{
/**************************************
 *
 *		p a r s e A l t e r T a b l e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *name = parseIdentifier ();
LinkedList fields;

for (;;)
	{
	if (match ("ADD"))
		{
		if (match ("("))
			{
			for (;;)
				{
				Syntax *field = parseField();
				fields.append (makeNode(nod_add_field, field));
				if (!match (","))
					break;
				}
			parseEndParen();
			}
		else if (match ("UNIQUE"))
			fields.append (makeNode(nod_unique, parseNames()));
		else if (match ("PRIMARY"))
			{
			parseKey();
			fields.append (makeNode(nod_primary_key, parseIndexFields()));
			}
		else if (match ("FOREIGN"))
			{
			fields.append (parseForeignKey ());
			}
		else if (match ("CONSTRAINT"))
			fields.append (parseTableConstraint());
		else
			{
			Syntax *field = parseField();
			fields.append (makeNode(nod_add_field, field));
			}
		}
	else if (match ("DROP"))
		{
		if (match ("PRIMARY"))
			{
			parseKey();
			fields.append (makeNode(nod_drop_primary_key));
			}
		else if (match ("FOREIGN"))
			{
			parseKey();
			Syntax *names = parseNames();
			Syntax *table = NULL;
			if (match ("REFERENCES"))
				table = parseIdentifier();
			fields.append (makeNode(nod_drop_foreign_key, names, table));
			}
		else if (match ("("))
			{
			for (;;)
				{
				Syntax *field = parseName();
				fields.append (makeNode(nod_drop_field, field));
				if (!match (","))
					break;
				}
			parseEndParen();
			}
		else
			{
			Syntax *field = parseName();
			fields.append (makeNode(nod_drop_field, field));
			}
		}
	else if (match ("ALTER"))
		{
		match ("COLUMN");
		Syntax *field = makeNode(nod_alter_field, 3);
		field->setChild (0, parseName());
		LinkedList clauses;

		for (;;)
			{
			Syntax *clause = parseFieldClause();
			if (!clause)
				break;
			clauses.append (clause);
			}

		field->setChild (2, makeNode(clauses));
		fields.append (field);
		}
	else
		error ("ADD | DROP");
	if (!match (","))
		break;
	}

return makeNode(nod_alter_table, name, makeNode(fields));
}

Syntax *SQLParse::parseAnd()
{
/**************************************
 *
 *		p a r s e A n d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *expr = parseNot();

while (match ("AND"))
	expr = makeNode(nod_and, expr, parseNot());

return expr;
}

Syntax *SQLParse::parseBoolean ()
{
/**************************************
 *
 *		p a r s e B o o l e a n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return parseOr ();
}

Syntax *SQLParse::parseConstant ()
{
/**************************************
 *
 *		p a r s e C o n s t a n t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

switch (tokenType)
	{
	case QUOTED_STRING:		return parseQuotedString();
	case NUMBER:			return parseNumber();
	case DECIMAL_NUMBER:	return parseDecimalNumber();

	default:
		error ("constant");
	}
return NULL;
}

Syntax *SQLParse::parseCreateIndex (bool unique, SyntaxType nodeType)
{
/**************************************
 *
 *		p a r s e C r e a t e I n d e x
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	Syntax *syntax = makeNode(nodeType, 4);

	if (unique)
		syntax->setChild (0, makeNode(nod_unique));

	syntax->setChild (1, parseName());

	if (!match ("ON"))
		error ("ON table");
			
	syntax->setChild (2, parseIdentifier());
	syntax->setChild (3, parseIndexFields());

	parseKeyOptions(true);

	return syntax;
}

Syntax *SQLParse::parseCreateTable (SyntaxType nodeType)
{
/**************************************
 *
 *		p a r s e C r e a t e T a b l e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	LinkedList fields;
	Syntax *name = parseIdentifier ();
	parseParen();

	for (;;)
		{
		if (match ("UNIQUE"))
			fields.append (makeNode(nod_unique, parseNames()));
		else if (match ("PRIMARY"))
			{
			parseKey();
			fields.append (makeNode(nod_primary_key, parseIndexFields()));
			}
		else if (match ("FOREIGN"))
			{
			fields.append (parseForeignKey ());
			}
		else if (match ("CONSTRAINT"))
			fields.append (parseTableConstraint());
		else
			fields.append (parseField());
			
		if (!match (","))
			break;
		}

	parseEndParen();
	Syntax *tableSpace = NULL;

	if (match("TABLESPACE"))
		tableSpace = parseName();

	return makeNode(nodeType, name, makeNode(fields), tableSpace);
}

Syntax *SQLParse::parseCreateView (SyntaxType nodeType)
{
/**************************************
 *
 *		p a r s e C r e a t e V i e w
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *name = parseIdentifier ();
Syntax *alias = parseOptNames();

if (!match ("AS"))
	return error ("AS SELECT");

if (!match ("SELECT"))
	return error ("SELECT");

Syntax *select = parseSelect();

return makeNode(nodeType, name, alias, select);
}

Syntax *SQLParse::parseDataType ()
{
/**************************************
 *
 *		p a r s e D a t a T y p e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (match ("INTEGER") || match ("INT"))
		return makeNode(nod_integer);
	else if (match ("SMALLINT"))
		return makeNode(nod_smallint);
	else if (match ("TINYINT"))
		return makeNode(nod_tinyint);
	else if (match ("BIGINT"))
		return makeNode(nod_bigint);
	else if (match ("FLOAT"))
		return makeNode(nod_float);
	else if (match ("DOUBLE"))
		{
		match ("PRECISION");
		return makeNode(nod_double);
		}
	else if (match ("DATE"))
		return makeNode(nod_date);
	else if (match ("DATETIME"))
		return makeNode(nod_date);
	else if (match ("TIMESTAMP"))
		return makeNode(nod_timestamp);
	else if (match ("DATEONLY"))
		return makeNode(nod_date);
	else if (match ("TIMEONLY"))
		return makeNode(nod_time);
	else if (match ("TIME"))
		return makeNode(nod_time);
	else if (match ("SMALLDATETIME"))
		return makeNode(nod_date);
	else if (match ("BLOB"))
		return makeNode(nod_blob);
	else if (match ("IMAGE"))
		return makeNode(nod_blob);
	else if (match ("TEXT"))
		return makeNode(nod_text);
	else if (match ("CLOB"))
		return makeNode(nod_text);
	else if (match ("CHAR"))
		{
		parseParen();
		Syntax *number = parseNumber();
		parseEndParen();
		
		return makeNode(nod_char, number);
		}
	else if (match ("VARCHAR"))
		{
		parseParen();
		Syntax *number = parseNumber();
		parseEndParen();
		
		return makeNode(nod_varchar, number);
		}
	else if (match ("NUMERIC") ||
			match ("NUMBER"))
		{
		parseParen();
		Syntax *size = parseNumber();
		Syntax *scale = NULL;
		
		if (match (","))
			scale = parseNumber();
			
		parseEndParen();
		
		return makeNode(nod_numeric, size, scale);
		}
	/****
	else if (Field::findDomain (token))
		{
		Syntax *domain = makeNode(nod_domain, token);
		nextToken();
		return domain;
		}
	****/

	return error ("data type");
}

Syntax *SQLParse::parseDecimalNumber ()
{
/**************************************
 *
 *		p a r s e D e c i m a l N u m b e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (tokenType != DECIMAL_NUMBER)
		error ("number");

	Syntax *syntax = makeNode(nod_decimal_number, token);
	nextToken();

	return syntax;
}

Syntax *SQLParse::parseDelete ()
{
/**************************************
 *
 *		p a r s e D e l e t e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!match ("FROM"))
	return error ("FROM");

Syntax *table = parseTable (true);
Syntax *node = NULL;

if (match ("WHERE"))
	{
	if (match ("CURRENT"))
		{
		if (!match ("OF"))
			error ("OF");
		node = makeNode(nod_cursor, parseName());
		}
	else
		node = parseBoolean();
	}

return makeNode(nod_delete, table, node);
}

void SQLParse::parseEndParen ()
{
/**************************************
 *
 *		p a r s e E n d P a r e n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!match (")"))
	error ("closing parenthesis");
}

Syntax *SQLParse::parseExpr ()
{
/**************************************
 *
 *		p a r s e E x p r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

//return parseSimpleExpr();
return parseAdd();
}

Syntax *SQLParse::parseExprList ()
{
/**************************************
 *
 *		p a r s e E x p r L i s t
 *
 **************************************
 *
 * Functional description
 *		SQLParse a comma list of expressions.  Look for trailing
 *		parenthesis.
 *
 **************************************/
LinkedList list;

if (match (")"))
	return makeNode(list);

for (;;)
	{
	list.append (parseExpr());
	if (!match (","))
		break;
	}

parseEndParen();

return makeNode(list);
}
		
Syntax *SQLParse::parseField ()
{
/**************************************
 *
 *		p a r s e F i e l d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	Syntax *field = makeNode(nod_field, 3);
	field->setChild (0, parseName());
	field->setChild (1, parseDataType());
	LinkedList clauses;

	for (;;)
		{
		Syntax *clause = parseFieldClause();
		if (!clause)
			break;
		clauses.append (clause);
		}

	field->setChild (2, makeNode(clauses));

	return field;
}
		
Syntax *SQLParse::parseFieldClause ()
{
/**************************************
 *
 *		p a r s e F i e l d C l a u s e
 *
 **************************************
 *
 * Functional description
 *		SQLParse a field clause.  Return NULL if nothing recognized.
 *
 **************************************/

	if (match ("NULL"))
		return makeNode(nod_null);

	if (match ("NOT"))
		if (match ("NULL"))
			return makeNode(nod_not_null);
		else if (match ("SEARCHABLE"))
			return makeNode(nod_not_searchable);
		else
			error ("NULL | SEARCHABLE");

	if (match ("DEFAULT"))
		return makeNode(nod_default_value, parseExpr());

	if (match ("SEARCHABLE"))
		return makeNode(nod_searchable);

	if (match ("COLLATION"))
		return makeNode(nod_collation, parseName());

	if (match ("UNIQUE"))
		return makeNode(nod_unique);

	if (match ("PRIMARY"))
		{
		parseKey();
		
		return makeNode(nod_primary_key);
		}

	if (match ("REFERENCES"))
		return parseForeignKey (NULL);

	if (match ("CONSTRAINT"))
		{
		Syntax *constraintName = parseName();
		
		if (match ("DEFAULT"))
			return makeNode(nod_default_value, parseExpr());
			
		if (match ("REFERENCES"))
			return parseForeignKey (constraintName);
			
		error ("field constraint");
		}

	if (match ("REPOSITORY"))
		return makeNode(nod_repository, parseName());

	/***
	if (match ("IDENTITY"))
		{
		if (!match ("("))
			error ("parenthesis");
		return makeNode(nod_identity, parseExprList());
		}
	***/

	return NULL;
}

Syntax *SQLParse::parseForeignKey ()
{
/**************************************
 *
 *		p a r s e F o r e i g n K e y
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

/***
Syntax *key = makeNode(nod_foreign_key, 3);
parseKey();
key->setChild (0, parseNames());

if (!match ("REFERENCES"))
	error ("REFERENCES");

return parseForeignKey (constraintName);

key->setChild (1, parseIdentifier());
key->setChild (2, parseOptNames());
return key;
***/

parseKey();
Syntax *names = parseNames();

if (!match ("REFERENCES"))
	error ("REFERENCES");

return parseForeignKey (names);
}

Syntax *SQLParse::parseGrant ()
{
/**************************************
 *
 *		p a r s e G r a n t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
LinkedList privs;

Syntax *privilege = parsePrivilege();

// If this isn't a known privilege, must be a roll name

if (!privilege)
	{
	Syntax *ident = parseIdentifier();
	if (!match ("TO"))
		error ("TO");
	Syntax *name = parseName();
	//Syntax *name = parseIdentifier();
	Syntax *option = NULL;
	Syntax *defaultRole = NULL;

	for (;;)
		if (match ("WITH"))
			{
			if (match ("GRANT"))
				{
				match ("OPTION");
				option = makeNode(nod_grant_option);
				}
			}
		else if (match ("DEFAULT"))
			defaultRole = makeNode(nod_default_role);
		else
			break;

	Syntax *node = makeNode(nod_grant_role, 4);
	node->setChild (0, ident);
	node->setChild (1, name);
	node->setChild (2, option);
	node->setChild (3, defaultRole);
	return node;
	}

privs.append (privilege);

while (match (","))
	{
	Syntax *privilege = parsePrivilege();
	if (!privilege)
		error ("privilege");
	privs.append (privilege);
	}

if (!match ("ON"))
	error ("ON");

SyntaxType type = nod_table;

if (match ("PROCEDURE"))
	type = nod_procedure;
else if (match ("VIEW"))
	type = nod_view;
else if (match ("ROLE"))
	type = nod_role;
else if (match ("USER"))
	type = nod_user;
else if (match ("COTERIE"))
	type = nod_coterie;
else if (match ("TABLE"))
	type = nod_table;
	
Syntax *object = makeNode(type, parseIdentifier());

if (!match ("TO"))
	error ("TO");

Syntax *user;

if (match ("ROLE"))
	user = makeNode(nod_role, parseIdentifier());
else
	{
	match ("USER");
	user = makeNode(nod_user, parseName());
	}

Syntax *node = makeNode(nod_grant, 5);
node->setChild (0, makeNode(privs));
node->setChild (1, object);
node->setChild (2, user);

if (match ("WITH"))
	{
	if (match ("GRANT"))
		{
		match ("OPTION");
		node->setChild (3, makeNode(nod_grant_option));
		}
	}

return node;
}

Syntax *SQLParse::parseIdentifier ()
{
/**************************************
 *
 *		p a r s e I d e n t i f i e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
LinkedList list;

if (tokenType != NAME && tokenType != QUOTED_NAME)
	error ("identifier");

for (;;)
	{
	if (match ("."))
		list.append (NULL);
	else
		{
		list.append (parseName());
		if (!match ("."))
			break;
		}
	}
return makeNode(nod_identifier, list);
}

Syntax *SQLParse::parseInsert (SyntaxType nodType)
{
/**************************************
 *
 *		p a r s e I n s e r t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

match ("INTO");
Syntax *syntax = makeNode(nodType, 4);
syntax->setChild (0, parseIdentifier ());

if (keyword ("("))
	syntax->setChild (1, parseNames());

if (match ("VALUES"))
	{
	if (!match ("("))
		error ("parenthesis");
	syntax->setChild (2, parseExprList());
	}
else if (match ("SELECT"))
	syntax->setChild (3, parseSelect());

return syntax;
}

void SQLParse::parseKey ()
{
/**************************************
 *
 *		p a r s e K e y
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!match ("KEY"))
	error ("KEY");
}

Syntax *SQLParse::parseKeyOptions (bool multiple)
{
/**************************************
 *
 *		p a r s e K e y O p t i o n s
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (match ("WITH"))
	for (;;)
		{
		if (match ("FILLFACTOR"))
			{
			if (!match ("="))
				error ("=");
			parseNumber();
			}
		else if (match ("ALLOW_DUP_ROW"))
			;
		else if (match ("IGNORE_DUP_ROW"))
			;
		else
			error ("key option");
		if (!multiple || !match (","))
			break;
		}

return NULL;
}

Syntax *SQLParse::parseIndexOptions ()
{
/**************************************
 *
 *		p a r s e I n d e x O p t i o n s
 *
 **************************************
 *
 * Functional description
 *		SQLParse index options.
 *
 **************************************/

if (match ("CLUSTERED"))
    ;
else 
	match ("NONCLUSTERED");

return NULL;
}


Syntax *SQLParse::parseName ()
{
/**************************************
 *
 *		p a r s e N a m e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	const char *name = NULL;

	if (tokenType == NAME)
		name = symbolManager->getSymbol (token);
	else if (tokenType == QUOTED_NAME)
		name = symbolManager->getString (token);
	else
		error ("name");

	Syntax *syntax = makeNode(nod_name, name);
	nextToken();

	return syntax;
}

Syntax *SQLParse::parseNames ()
{
/**************************************
 *
 *		p a r s e N a m e s
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (!match ("("))
		error ("name list");

	LinkedList list;

	for (;;)
		{
		list.append (parseName());
		if (!match (","))
			break;
		}

	parseEndParen();

	return makeNode(list);
}

Syntax *SQLParse::parseNot ()
{
/**************************************
 *
 *		p a r s e N o t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (match ("NOT"))
	return makeNode(nod_not, parseNot());

return parseSimpleBoolean();
}

Syntax *SQLParse::parseNumber ()
{
/**************************************
 *
 *		p a r s e N u m b e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (tokenType != NUMBER)
		error ("number");

	Syntax *syntax = makeNode(nod_number, token);
	nextToken();
	
	return syntax;
}

Syntax *SQLParse::parseOptNames ()
{
/**************************************
 *
 *		p a r s e O p t N a m e s
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!keyword ("("))
	return NULL;

return parseNames ();
}

Syntax *SQLParse::parseOr()
{
/**************************************
 *
 *		p a r s e O r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *expr = parseAnd();

while (match ("OR"))
	expr = makeNode(nod_or, expr, parseAnd());

return expr;
}

Syntax *SQLParse::parseOrder ()
{
/**************************************
 *
 *		p a r s e O r d e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *syntax = makeNode(nod_order, 2);

if (tokenType == NUMBER)
	syntax->setChild (0, parseNumber());
else
	syntax->setChild (0, parseExpr());

if (match ("ASC"))
	;
else if (match ("DESC"))
	syntax->setChild (1, makeNode(nod_descending));

return syntax;
}

Syntax *SQLParse::parseParameter ()
{
/**************************************
 *
 *		p a r s e P a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/


if (tokenType != NAME && tokenType != QUOTED_STRING)
	error ("named parameter following '?'");

Syntax *syntax = makeNode(nod_parameter, token);
nextToken();

return syntax;
}

void SQLParse::parseParen ()
{
/**************************************
 *
 *		p a r s e P a r e n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!match ("("))
	error ("parenthesis");
}

Syntax *SQLParse::parseQuotedString ()
{
/**************************************
 *
 *		p a r s e Q u o t e d S t r i n g
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (tokenType != QUOTED_STRING)
		error ("quoted string");

	Syntax *syntax = makeNode(nod_quoted_string, token);
	nextToken();

	return syntax;
}

Syntax *SQLParse::parseSelect ()
{
/**************************************
 *
 *		p a r s e S e l e c t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	Syntax *syntax = makeNode(nod_select, 2);
	LinkedList list;

	for (;;)
		{
		list.append (parseSelectClause());
		
		if (!match ("UNION"))
			break;
			
		match ("ALL");
		
		if (!match ("SELECT"))
			error ("SELECT");
		}

	syntax->setChild (0, makeNode(list));

	if (match ("ORDER"))
		{
		match ("BY");
		LinkedList sortOrder;
		
		for (;;)
			{
			sortOrder.append (parseOrder());
			
			if (!match (","))
				break;
			}
			
		syntax->setChild (1, makeNode(sortOrder));
		}

	return syntax;
}

Syntax *SQLParse::parseSelectClause ()
{
/**************************************
 *
 *		p a r s e S e l e c t C l a u s e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	Syntax *syntax = makeNode(nod_select_clause, 6);

	if (match ("ALL"))
		;
	else if (match ("DISTINCT"))
		syntax->setChild (0, makeNode(nod_distinct));

	if (!match ("*"))
		{
		LinkedList list;
		
		for (;;)
			{
			Syntax *expr = parseSelectExpr();
			
			if (match ("AS"))
				expr = new Syntax (nod_alias, expr, parseName());
				
			list.append (expr);
			
			if (!match (","))
				break;
			}
			
		syntax->setChild (1, makeNode(list));
		}

	if (!match ("FROM"))
		error ("FROM clause");

	syntax->setChild (2, parseJoin());

	if (match ("WHERE"))
		syntax->setChild (3, parseBoolean());

	if (match ("GROUP"))
		{
		match ("BY");
		LinkedList list;
		
		for (;;)
			{
			list.append (parseExpr());
			
			if (!match (","))
				break;
			}
			
		syntax->setChild (4, makeNode(list));
		
		if (match ("HAVING"))
			syntax->setChild (5, parseBoolean());
		}

	return syntax;
}


Syntax *SQLParse::parseSelectExpr ()
{
/**************************************
 *
 *		p a r s e S e l e c t E x p r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
/***
SyntaxType type;

if (match ("COUNT"))
	type = nod_count;
else if (match ("SUM"))
	type = nod_sum;
else if (match ("AVG"))
	type = nod_avg;
else if (match ("MAX"))
	type = nod_max;
else if (match ("MIN"))
	type = nod_min;
else
	return parseExpr();

parseParen();
Syntax *distinct = NULL;
Syntax *expr = NULL;

if (type == nod_count && match ("*"))
	;
else if (match ("ALL"))
	expr = parseExpr();
else
	{
	match ("DISTINCT");
	distinct = makeNode(nod_distinct);
	expr = parseExpr();
	}

parseEndParen();

return makeNode(type, distinct, expr);
***/

return parseExpr();
}

Syntax *SQLParse::parseSequence (SyntaxType nodeType)
{
/**************************************
 *
 *		p a r s e S e q u e n c e
 *
 **************************************
 *
 * Functional description
 *		SQLParse Oracle "create sequence" command.
 *
 **************************************/
Syntax *name = parseIdentifier();
LinkedList clauses;

for (;;)
    {
	/***
	if (match ("NOMAXVALUE"))
		;
	else if (match ("NOMINVALUE"))
		;
	else if (match ("NOMINVALUE"))
		;
	else if (match ("NOCYCLE"))
		;
	else if (match ("NOORDER"))
		;
	else if (match ("NOCACHE"))
		;
	else if (match ("INCREMENT"))
		{
		if (!match ("BY"))
			error ("BY");
		clauses.append (makeNode(nod_increment, parseNumber()));
		}
	else 
	***/
	if (match ("START"))
		{
		if (!match ("WITH"))
			error ("WITH");
		clauses.append (makeNode(nod_start, parseNumber()));
		}
	/***
	else if (match ("MINVALUE"))
		clauses.append (makeNode(nod_minvalue, parseNumber()));
	else if (match ("MAXVALUE"))
		clauses.append (makeNode(nod_maxvalue, parseNumber()));
	else if (match ("CACHE"))
		clauses.append (makeNode(nod_cache, parseNumber()));
	else if (match ("CYCLE"))
		clauses.append (makeNode(nod_cycle));
	else if (match ("ORDER"))
		clauses.append (makeNode(nod_order));
	***/
	else
		break;
	}

return makeNode(nodeType, name, makeNode(clauses));
}

Syntax *SQLParse::parseSimpleBoolean ()
{
/**************************************
 *
 *		p a r s e S i m p l e B o o l e a n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (match ("LOG_BOOL"))
		{
		if (!match ("("))
			error ("value");
		Syntax *value = parseExpr();
		parseEndParen();
		Syntax *boolean = parseSimpleBoolean();
		Syntax *syntax = makeNode(nod_log_bool, value, boolean);
		return syntax;
		}

	if (match ("("))
		{
		Syntax *expr = parseBoolean();
		parseEndParen();
		return expr;
		}

	if (match ("EXISTS"))
		{
		if (!match ("("))
			error ("parenthesised select express");
		if (!match ("SELECT"))
			error ("select expression");
		Syntax *syntax = makeNode(nod_exists, parseSelect());
		parseEndParen();
		return syntax;
		}

	Syntax *expr1 = parseExpr();
	bool negate = match ("NOT");
	SyntaxType type = nod_not; // initialized to avoid warnings

	if (match (">"))
		{
		if (match ("="))
			type = nod_geq;
		else
			type = nod_gtr;
		}
	else if (match ("<"))
		{
		if (match ("="))
			type = nod_leq;
		else if (match (">"))
			type = nod_neq;
		else
			type = nod_lss;
		}
	else if (match ("="))
		type = nod_eql;
	else if (match ("BETWEEN"))
		type = nod_between;
	else if (match ("LIKE"))
		type = nod_like;
	else if (match ("STARTING"))
		{
		match ("WITH");
		type = nod_starting;
		}
	else if (match ("CONTAINING"))
		type = nod_containing;
	else if (match ("MATCHING"))
		type = nod_matching;
	else if (match ("IS"))
		{
		if (match ("ACTIVE_ROLE"))
			return makeNode(nod_is_active_role, expr1);
		type = nod_is_null;
		if (match ("NOT"))
			type = nod_not_null;
		if (!match ("NULL"))
			error ("NULL or ACTIVE_ROLE");
		return makeNode(type, expr1);
		}
	else if (match ("IN"))
		{
		Syntax *syntax = parseIn (expr1);
		if (negate)
			syntax = makeNode(nod_not, syntax);
		return syntax;
		}
	else
		error ("comparison operator");

	Syntax *expr2 = parseExpr();
	Syntax *syntax = NULL;

	if (type == nod_between)
		{
		match ("AND");
		syntax = makeNode(type, expr1, expr2, parseExpr());
		}
	else
		syntax = makeNode(type, expr1, expr2);

	if (negate)
		syntax = makeNode(nod_not, syntax);

	return syntax;
}

Syntax *SQLParse::parseSimpleExpr ()
{
/**************************************
 *
 *		p a r s e S i m p l e E x p r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
Syntax	*name;

if (match ("-"))
	{
	return makeNode(nod_negate, parseSimpleExpr());
	}

if (match ("NULL"))
	return makeNode(nod_null);

if (match ("?"))
	return makeNode(nod_parameter);

if (match ("CASE"))
	{
	LinkedList list;
	if (keyword ("WHEN"))
		{
		while (match ("WHEN"))
			{
			list.append (parseBoolean());
			require ("THEN");
			list.append (parseExpr());
			}
		if (match ("ELSE"))
			list.append (parseExpr());
		require ("END");
		return makeNode(nod_case, list);
		}
	list.append (parseExpr());
	while (match ("WHEN"))
		{
		list.append (parseExpr());
		require ("THEN");
		list.append (parseExpr());
		}
	if (match ("ELSE"))
		list.append (parseExpr());
	require ("END");
	return makeNode(nod_case_search, list);
	}

if (match ("SELECT"))
	return makeNode(nod_select_expr, parseSelect());

if (match ("CAST"))
	{
	if (!match ("("))
		error ("parenthesis");
	Syntax *value = parseExpr();
	if (!match ("AS"))
		error ("AS");
	Syntax *dataType = parseDataType();
	parseEndParen();
	return makeNode(nod_cast, value, dataType);
	}

if (match ("("))
	{
	Syntax *expression = parseExpr();
	parseEndParen();
	return expression;
	}

if (match ("NEXT_VALUE"))
	return makeNode(nod_next_value, parseIdentifier());

SyntaxType type = nod_not; // initialized to avoid warning

if (match ("COUNT"))
	type = nod_count;
else if (match ("SUM"))
	type = nod_sum;
else if (match ("AVG"))
	type = nod_avg;
else if (match ("MAX"))
	type = nod_max;
else if (match ("MIN"))
	type = nod_min;
else
	switch (tokenType)
		{
		case QUOTED_STRING:		return parseQuotedString();
		case NUMBER:			return parseNumber();
		case DECIMAL_NUMBER:	return parseDecimalNumber();

		case NAME:
			name = parseIdentifier();
			if (match ("("))
				{
				Syntax *function = makeNode(nod_function, name, parseExprList());
				return function;
				}
			return name;

		default:
			error ("expression");
		}


parseParen();
Syntax *distinct = NULL;
Syntax *expr = NULL;

if (type == nod_count && match ("*"))
	;
else if (match ("ALL"))
	expr = parseExpr();
else
	{
	match ("DISTINCT");
	distinct = makeNode(nod_distinct);
	expr = parseExpr();
	}

parseEndParen();

return makeNode(type, distinct, expr);
}

Syntax *SQLParse::parseStatement ()
{
/**************************************
 *
 *		p a r s e S t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/


if (match ("CREATE"))
	{
	if (match ("DATABASE"))
		return makeNode(nod_create_database,
						   parseName(),
						   parseQuotedString());
	else if (match ("TABLE"))
		return parseCreateTable (nod_create_table);
	else if (match ("INDEX"))
		return parseCreateIndex (false, nod_create_index);
	else if (match ("VIEW"))
		return parseCreateView (nod_create_view);
	else if (match ("USER"))
		return parseCreateUser ();
	else if (match ("ROLE"))
		return parseCreateRole (nod_create_role);
	else if (match ("TRIGGER"))
		return parseCreateTrigger (nod_create_trigger);
	else if (match ("FILTERSET"))
		return parseCreateFilterSet (nod_create_filterset);
	else if (match ("COTERIE"))
		return parseCreateCoterie (nod_create_coterie);
	else if (match ("DOMAIN"))
		return parseDomain(nod_create_domain);
	else if (match ("SEQUENCE"))
		return parseSequence (nod_create_sequence);
	else if (match ("REPOSITORY"))
		return parseRepository (nod_create_repository);
	else if (match ("SCHEMA"))
		return parseSchema (nod_upgrade_schema);
	else if (match ("TABLESPACE"))
		return parseCreateTableSpace (nod_create_tablespace);
	else if (match ("UNIQUE"))
		{
		parseIndexOptions();
		
		if (!match ("INDEX"))
			error ("INDEX");
			
		return parseCreateIndex (true, nod_create_index);
		}
	else if (match ("CLUSTERED"))
		{
		if (!match ("INDEX"))
			error ("INDEX");
		return parseCreateIndex (false, nod_create_index);
		}
	/***
	else if (match ("SCHEMA"))
		{
		if (match ("AUTHORIZATION"))
			return makeNode(nod_schema_authorization,
									  parseName());
		error ("authorization");
		}
	***/
	error ("database | table | view | index | unique index | sequence | trigger | filterset");
	}

if (match ("UPGRADE"))
	{
	if (match ("TABLE"))
		return parseCreateTable (nod_upgrade_table);
		
	if (match ("ROLE"))
		return parseCreateRole (nod_upgrade_role);
	else if (match ("INDEX"))
		return parseCreateIndex (false, nod_upgrade_index);
	else if (match ("UNIQUE"))
		{
		parseIndexOptions();
		
		if (!match ("INDEX"))
			error ("INDEX");
			
		return parseCreateIndex (true, nod_upgrade_index);
		}
	else if (match ("SEQUENCE"))
		return parseSequence (nod_upgrade_sequence);
	else if (match ("TRIGGER"))
		return parseCreateTrigger (nod_upgrade_trigger);
	else if (match ("FILTERSET"))
		return parseCreateFilterSet (nod_upgrade_filterset);
	else if (match ("COTERIE"))
		return parseCreateCoterie (nod_upgrade_coterie);
	else if (match ("VIEW"))
		return parseCreateView (nod_upgrade_view);
	else if (match ("REPOSITORY"))
		return parseRepository (nod_upgrade_repository);
	else if (match ("DOMAIN"))
		return parseDomain(nod_upgrade_domain);
	else if (match ("SCHEMA"))
		return parseSchema (nod_upgrade_schema);
	else if (match ("TABLESPACE"))
		return parseCreateTableSpace (nod_upgrade_tablespace);
	/***
	else if (match ("CLUSTERED"))
		{
		if (!match ("INDEX"))
			error ("INDEX");
		return parseCreateIndex (false);
		}
	else if (match ("SCHEMA"))
		{
		if (match ("AUTHORIZATION"))
			return makeNode(nod_schema_authorization,
									  parseName());
		error ("authorization");
		}
	***/
	error ("database | table | view | index | unique index | sequence | trigger | filterset | coterie");
	}

if (match ("ALTER"))
	{
	if (match ("TABLE"))
		return parseAlterTable();
	if (match ("NAMESPACE"))
		return parseAlterNameSpace();
	if (match ("USER"))
		return parseAlterUser();
	if (match ("TRIGGER"))
		return parseAlterTrigger();
	error ("database | table | index | trigger");
	}

if (match ("DROP"))
	{
	/***
	if (match ("DATABASE"))
		return makeNode(nod_drop_database, parseName());
	***/
	if (match ("INDEX"))
		return makeNode(nod_drop_index, parseIdentifier());
		
	if (match ("TABLE"))
		return makeNode(nod_drop_table, parseIdentifier());
		
	if (match ("VIEW"))
		return makeNode(nod_drop_view, parseIdentifier());
		
	if (match ("USER"))
		return makeNode(nod_drop_user, parseName());
		
	if (match ("COTERIE"))
		return makeNode(nod_drop_coterie, parseName());
		
	if (match ("ROLE"))
		return makeNode(nod_drop_role, parseIdentifier());
		
	if (match ("SEQUENCE"))
		return makeNode(nod_drop_sequence, parseIdentifier());
		
	if (match ("TRIGGER"))
		return makeNode(nod_drop_trigger, parseIdentifier());
		
	if (match ("FILTERSET"))
		return makeNode(nod_drop_filterset, parseIdentifier());
		
	if (match ("REPOSITORY"))
		return makeNode(nod_drop_repository, parseIdentifier());
		
	if (match ("TABLESPACE"))
		return makeNode(nod_drop_tablespace, parseName());
		
	error ("DATABASE | TABLE | VIEW | INDEX | CONSTRAINT | USER | ROLE");
	}

if (match ("RENAME"))
	{
	if (match ("TABLE"))
		return renameTable();
	error ("TABLE");
	}

if (match ("INSERT"))
	return parseInsert(nod_insert);

if (match ("REPLACE"))
	return parseInsert(nod_replace);

if (match ("SELECT"))
	return parseSelect();

if (match ("UPDATE"))
	return parseUpdate();

if (match ("DELETE"))
	return parseDelete();

if (match ("REPAIR"))
	return parseRepair();

if (match ("GRANT"))
	return parseGrant();
if (match ("REVOKE"))
	return parseRevoke();

/***
if (match ("OPEN"))
	return makeNode(nod_open, parseQuotedString());
if (match ("SHOW"))
	return makeNode(nod_show);
if (match ("LOAD"))
	return makeNode(nod_load, parseQuotedString());
if (match ("SAVE"))
	return makeNode(nod_save, parseQuotedString());

if (match ("CLOSE"))
	return makeNode(nod_close);
***/

if (match ("REINDEX"))
	{
	Syntax *name = parseName();
	match ("ON");
	return makeNode(nod_reindex, name, parseIdentifier ());
	}

if (match ("COMMIT"))
	{
	match ("WORK");
	return makeNode(nod_commit);
	}

if (match ("ROLLBACK"))
	{
	match ("WORK");
	return makeNode(nod_rollback);
	}

/***
if (match ("ASSUME"))
	return parseAssumeRole();

if (match ("REVERT"))
	return makeNode(nod_revert);
***/

if (match ("ENABLE"))
	{
	if (match ("FILTERSET"))
		return makeNode(nod_enable_filterset, parseIdentifier());
	if (match ("TRIGGER"))
		{
		if (!match ("CLASS"))
			error ("CLASS");
		return makeNode(nod_enable_trigger_class, parseName());
		}
	error ("FILTERSET");
	}

if (match ("DISABLE"))
	{
	if (match ("FILTERSET"))
		return makeNode(nod_disable_filterset, parseIdentifier());
	if (match ("TRIGGER"))
		{
		if (!match ("CLASS"))
			error ("CLASS");
		return makeNode(nod_disable_trigger_class, parseName());
		}
	error ("FILTERSET");
	}

if (match ("SYNCHRONIZE"))
	{
	match ("REPOSITORY");
	Syntax *name = parseIdentifier();
	match ("WITH");
	match ("FILE");
	return makeNode(nod_sync_repository, name, parseQuotedString());
	}

/***
if (match ("PRINT"))
	return makeNode(nod_print, parseQuotedString());

if (match ("GO"))
	return makeNode(nod_go);
***/

return error ("statement");
}

Syntax *SQLParse::parseTable (bool alias)
{
/**************************************
 *
 *		p a r s e T a b l e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *name = parseIdentifier();
Syntax *synonym = NULL;

if (alias && tokenType == NAME &&
	!keyword ("WHERE") &&
	!keyword ("GROUP") &&
	!keyword ("ORDER") &&
	!keyword ("LEFT") &&
	!keyword ("RIGHT") &&
	!keyword ("FULL") &&
	!keyword ("JOIN") &&
	!keyword ("ON") &&
	!keyword ("UNION"))
	synonym = parseName();

return makeNode(nod_table, name, synonym);
}

Syntax *SQLParse::parseTableConstraint ()
{
/**************************************
 *
 *		p a r s e T a b l e C o n s t r a i n t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
Syntax *name = parseName();
Syntax *constraint = NULL;

if (match ("UNIQUE"))
	{
	parseIndexOptions();
	constraint = makeNode(nod_unique, parseNames());
	}
else if (match ("PRIMARY"))
	{
	parseKey();
	parseIndexOptions();
	constraint = makeNode(nod_primary_key, parseIndexFields());
	parseKeyOptions (false);
	}
else if (match ("FOREIGN"))
	constraint = parseForeignKey();
else if (match ("CHECK"))
	{
	if (match ("NOT"))
		{
		match ("FOR");
		match ("REPLICATION");
		}
	parseParen();
	constraint = makeNode(nod_check, parseBoolean());
	parseEndParen();
	}
else
	error ("constraint type");

return makeNode(nod_constraint, name, constraint);
}

Syntax *SQLParse::parseUpdate ()
{
/**************************************
 *
 *		p a r s e U p d a t e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Syntax *table = parseTable (false);

if (!match ("SET"))
	return error ("SET clause");

LinkedList items;

for (;;)
	{
	Syntax *field = parseIdentifier();
	if (!match ("="))
		return error ("=");
	Syntax *value = parseExpr();
	Syntax *assign = makeNode(nod_assign, field, value);
	items.append (assign);
	if (!match (","))
		break;
	}

Syntax *booleon = NULL;

if (match ("WHERE"))
	{
	if (match ("CURRENT"))
		{
		if (!match ("OF"))
			error ("OF");
		booleon = makeNode(nod_cursor, parseName());
		}
	else
		booleon = parseBoolean();
	}

return makeNode(nod_update, table, makeNode(items), booleon);
}

void SQLParse::registerNode (Syntax *node)
{
/**************************************
 *
 *		r e g i s t e r N o d e
 *
 **************************************
 *
 * Functional description
 *		Register syntax nodes
 *
 **************************************/

	node->next = nodes;
	nodes = node;
}

char SQLParse::skipWhite ()
{
/**************************************
 *
 *		s k i p W h i t e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int		skipping = false;

for (;;)
    {
	while (pos < length)
		{
		char c = string [pos++];
		if (skipping)
			{
			if (c == '*' && string [pos] == '/')
				{
				skipping = false;
				++pos;
				}
			}
		else if (!(characters [(int) c] & WHITE))
			{
			if (c == '/' && string [pos] == '*')
				{
				skipping = true;
				++pos;
				}
			else if (c == '/' && string [pos] == '/')
				for (++pos; (pos < length) && string [pos] != '\n'; ++pos)
					;				
			else
				return c;
			}
		}
	if (!input)
		return 0;
	return 0;
	}

return 0;
}

int SQLParse::saveBegin()
{
	return tokenStart;
}

void SQLParse::saveEnd()
{
}

void SQLParse::saveBackup(int position)
{
	pos = position;

	if (trace)
		Log::debug ("backing up...\n");

	getToken();
}


Syntax* SQLParse::parseAdd()
{
	Syntax *expr = parseMultiply();

	for (;;)
		if (match ("+"))
			expr = makeNode(nod_add, expr, parseMultiply());
		else if (match ("-"))
			expr = makeNode(nod_subtract, expr, parseMultiply());
		else if (match ("||"))
			expr = makeNode(nod_concat, expr, parseMultiply());
		else
			break;

	return expr;
}

Syntax* SQLParse::parseMultiply()
{
	Syntax *expr = parseSimpleExpr();

	for (;;)
		if (match ("*"))
			expr = makeNode(nod_multiply, expr, parseSimpleExpr());
		else if (match ("/"))
			expr = makeNode(nod_divide, expr, parseSimpleExpr());
		else if (match ("%"))
			expr = makeNode(nod_mod, expr, parseSimpleExpr());
		else
			break;

	return expr;
}

void SQLParse::require(const char * keyword)
{
	if (!match (keyword))
		error (keyword);
}

Syntax* SQLParse::parsePrivilege()
{
	for (const PRIV *privilege = privileges; privilege->string; ++privilege)
		if (match (privilege->string))
			return makeNode(privilege->type);

	return NULL;
}

Syntax* SQLParse::parseRevoke()
{
	LinkedList privs, objects, users;
	Syntax *privilege = parsePrivilege();

	// If this isn't a known privilege, must be a role name

	if (!privilege)
		{
		Syntax *ident = parseIdentifier();
		if (!match ("FROM"))
			error ("FROM");
		Syntax *name = parseName();

		Syntax *node = makeNode(nod_revoke_role, 4);
		node->setChild (0, ident);
		node->setChild (1, name);
		return node;
		}

	privs.append (privilege);

	while (match (","))
		{
		Syntax *privilege = parsePrivilege();
		if (!privilege)
			error ("privilege");
		privs.append (privilege);
		}

	if (match ("ON"))
		for (;;)
			{
			SyntaxType type = nod_table;
			if (match ("PROCEDURE"))
				type = nod_procedure;
			else if (match ("VIEW"))
				type = nod_view;
			Syntax *object = makeNode(type, parseIdentifier());
			objects.append (object);
			//objects.append (parseIdentifier());
			if (!match (","))
				break;
			}

	if (!match ("FROM"))
		error ("FROM");

	for (;;)
		{
		//users.append (parseIdentifier());
		Syntax *user;
		if (match ("ROLE"))
			user = makeNode(nod_role, parseIdentifier());
		else
			user = makeNode(nod_user, parseIdentifier());
		users.append (user);
		if (!match (","))
			break;
		}

	return makeNode(nod_revoke, 
					  makeNode(privs),
					  makeNode(objects), 
					  makeNode(users));
}

Syntax* SQLParse::parseCreateUser()
{
	Syntax *user = parseName();
	Syntax *encrypted = NULL;

	if (match ("ENCRYPTED"))
		encrypted = makeNode(nod_encrypted);

	if (!match ("PASSWORD"))
		error ("PASSWORD");

	// The following crock is for backward compatibility
	//Syntax *password = (tokenType == QUOTED_STRING) ? parseQuotedString() : parseName();

	Syntax *password = parseQuotedString();
	Syntax *coterie = NULL;

	if (match ("COTERIE"))
		coterie = parseName();

	return makeNode(nod_create_user, user, password, coterie, encrypted);
}

Syntax* SQLParse::parseCreateRole (SyntaxType nodeType)
{
	Syntax *role = parseIdentifier();

	return makeNode(nodeType, role);
}

Syntax* SQLParse::parseAssumeRole()
{
	LinkedList	roles;

	for (;;)
		{
		roles.append (parseIdentifier());
		if (!match (","))
			break;
		}

	return makeNode(nod_assume, roles);
}

Syntax* SQLParse::parseAlterUser()
{
	Syntax *user = parseName();
	Syntax *password = NULL;
	Syntax *coterie = NULL;
	Syntax *encrypted = NULL;

	if (match ("ENCRYPTED"))
		encrypted = makeNode(nod_encrypted);

	if (match ("PASSWORD"))
		password = parseQuotedString();

	if (match ("COTERIE"))
		coterie = parseName();

	return makeNode(nod_alter_user, user, password, coterie, encrypted);
}

Syntax* SQLParse::parseIn(Syntax *expr)
{
	if (!match ("("))
		error ("parenthesised value list");
		
	if (match ("SELECT"))
		{
		Syntax *syntax = makeNode(nod_in_select, expr, parseSelect());
		parseEndParen();
		
		return syntax;
		}

	LinkedList values;

	for (;;)
		{
		values.append (parseExpr());
		
		if (!match (","))
			break;
		}

	parseEndParen();

	return makeNode(nod_in_list, expr, makeNode(values));
}

Syntax* SQLParse::parseJoin()
{
	if (match ("("))
		{
		Syntax *node = (match("SELECT")) ? parseSelect() : parseJoin();
		parseEndParen();
		
		return node;
		};

	Syntax *node = parseTable (true);

	// Handle legacy case

	if (match (","))
		{
		LinkedList list;
		list.append (node);
		
		for (;;)
			{
			list.append (parseTable (true));
			
			if (!match (","))
				break;
			}
			
		return makeNode(nod_join, list);
		}

	Syntax *boolean = NULL;

	if (match ("ON"))
		boolean = parseBoolean();

	// Handle the hip, new, overblown, over-general, syntactically obsese versions

	SyntaxType type = nod_inner;

	if (match ("INNER"))
		type = nod_inner;
	else if (match ("LEFT"))
		{
		type = nod_left_outer;
		match ("OUTER");
		}
	else if (match ("RIGHT"))
		{
		type = nod_right_outer;
		match ("OUTER");
		}
	else if (match ("FULL"))
		{
		type = nod_full_outer;
		match ("OUTER");
		}

	if (!match ("JOIN"))
		{
		if (type == nod_inner)
			{
			if (boolean)
				return makeNode(nod_join_term, node, boolean);
			return makeNode(nod_join, node);
			}

		error ("JOIN");
		}

	Syntax *sub = parseJoin();

	return makeNode(type, node, sub, boolean);
}

Syntax* SQLParse::parseCreateTrigger(SyntaxType nodeType)
{
	Syntax *syntax = makeNode(nodeType, 4);
	syntax->setChild (0, parseName());
	match ("FOR");
	syntax->setChild (1, parseIdentifier());

	// Get options

	LinkedList options;
	bool before = true;
	bool verbPending = false;

	for (;;)
		{
		if (match ("INSERT"))
			{
			options.append (makeNode((before) ? nod_pre_insert : nod_post_insert));
			verbPending = false;
			}
		else if (match ("UPDATE"))
			{
			options.append (makeNode((before) ? nod_pre_update : nod_post_update));
			verbPending = false;
			}
		else if (match ("DELETE"))
			{
			options.append (makeNode((before) ? nod_pre_delete : nod_post_delete));
			verbPending = false;
			}
		else if (match ("COMMIT"))
			{
			options.append (makeNode((before) ? nod_pre_commit : nod_post_commit));
			verbPending = false;
			}
		else if (verbPending)
			error ("INSERT | UPDATE | DELETE");
		else if (match ("BEFORE"))
			{
			before = true;
			verbPending = true;
			}
		else if (match ("AFTER"))
			{
			before = false;
			verbPending = true;
			}
		else if (match ("POSITION"))
			options.append (makeNode(nod_position, parseNumber()));
		else if (match ("ACTIVE"))
			options.append (makeNode(nod_active));
		else if (match ("INACTIVE"))
			options.append (makeNode(nod_inactive));
		else if (match ("CLASS"))
			options.append (makeNode(nod_trigger_class, parseName()));
		else if (match (","))
			;
		else
			break;
		}

	syntax->setChild (2, makeNode(options));

	if (!match ("USING"))
		error ("USING 'trigger method'");

	syntax->setChild (3, parseQuotedString());

	return syntax;
}

Syntax* SQLParse::parseAlterTrigger()
{
	Syntax *name = parseName();
	match ("FOR");
	Syntax *tableName = parseIdentifier();

	// Get options

	LinkedList options;
	//bool before = true;
	//bool verbPending = false;

	for (;;)
		{
		/***
		if (match ("INSERT"))
			{
			options.append (makeNode((before) ? nod_pre_insert : nod_post_insert));
			verbPending = false;
			}
		else if (match ("UPDATE"))
			{
			options.append (makeNode((before) ? nod_pre_update : nod_post_update));
			verbPending = false;
			}
		else if (match ("DELETE"))
			{
			options.append (makeNode((before) ? nod_pre_delete : nod_post_delete));
			verbPending = false;
			}
		else if (match ("COMMIT"))
			{
			options.append (makeNode((before) ? nod_pre_commit : nod_post_commit));
			verbPending = false;
			}
		else if (verbPending)
			error ("INSERT | UPDATE | DELETE");
		else if (match ("BEFORE"))
			{
			before = true;
			verbPending = true;
			}
		else if (match ("AFTER"))
			{
			before = false;
			verbPending = true;
			}
		***/
		if (match ("POSITION"))
			options.append (makeNode(nod_position, parseNumber()));
		else if (match ("ACTIVE"))
			options.append (makeNode(nod_active));
		else if (match ("INACTIVE"))
			options.append (makeNode(nod_inactive));
		else if (match (","))
			;
		else
			break;
		}


	return makeNode(nod_alter_trigger, name, tableName, makeNode(options));
}

Syntax* SQLParse::parseCreateFilterSet(SyntaxType nodeType)
{
	Syntax *name = parseIdentifier();

	if (!match ("("))
		error ("parenthesised filter list");

	LinkedList filters;

	for (;;)
		{
		Syntax *tableName = parseIdentifier();
		Syntax *alias = NULL;
		if (!match (":"))
			{
			alias = parseName();
			if (!match (":"))
				error ("colon");
			}
		Syntax *boolean = parseBoolean();
		filters.append (makeNode(nod_table_filter, tableName, boolean, alias));
		if (!match (","))
			break;
		}

	parseEndParen();

	return makeNode(nodeType, name, makeNode(filters));
}

Syntax* SQLParse::parseCreateCoterie(SyntaxType nodeType)
{
	ipAddress = true;
	Syntax *name = parseName();

	if (!match ("AS"))
		error ("AS");

	LinkedList addresses;

	for (;;)
		{
		Syntax *from = parseAddress();
		Syntax *to = NULL;
		if (match ("TO"))
			to = parseAddress();
		addresses.append (makeNode(nod_range, from, to));
		if (!match (","))
			break;
		}

	ipAddress = false;

	return makeNode(nodeType, name, makeNode(addresses));
}

Syntax* SQLParse::parseAddress()
{
	if (tokenType == IP_ADDRESS)
		{
		Syntax *node = makeNode(nod_ip_address, token);
		getToken();
		return node;
		}

	LinkedList segments;

	if (match ("*"))
		{
		segments.append (makeNode(nod_wildcard));
		if (!match ("."))
			error ("dot after node wildcard");
		}

	for (;;)
		{
		segments.append (parseName());
		if (!match ("."))
			break;
		}

	return makeNode(nod_node_name, segments);
}

Syntax* SQLParse::makeNode(SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3, Syntax *child4)
{
	Syntax *syntax = new Syntax (type, child1, child2, child3, child4);
	registerNode (syntax);

	return syntax;
}

Syntax* SQLParse::parseRepair()
{
	if (!match ("FROM"))
		return error ("FROM");

	Syntax *table = parseTable (true);
	Syntax *node = NULL;

	if (match ("WHERE"))
		node = parseBoolean();

	return makeNode(nod_repair, table, node);
}

Syntax* SQLParse::parseRepository(SyntaxType nodeType)
{
	Syntax *syntax = makeNode(nodeType, 5);
	syntax->setChild (0, parseIdentifier());

	for (;;)
		{
		if (match ("SEQUENCE"))
			syntax->setChild (1, parseName());
		else if (match ("FILENAME"))
			syntax->setChild (2, parseQuotedString());
		else if (match ("VOLUME"))
			syntax->setChild (3, parseNumber());
		else if (match ("ROLLOVER"))
			syntax->setChild (4, parseQuotedString());
		else if (match (","))
			;
		else
			break;
		}

	return syntax;
}

Syntax* SQLParse::parseDomain(SyntaxType type)
{
	Syntax *name = parseIdentifier();
	Syntax *dtype = parseDataType();

	return makeNode(type, name, dtype);
}


Syntax* SQLParse::parseSchema(SyntaxType type)
{
	Syntax *name = parseName();
	Syntax *systemId = NULL;
	Syntax *sequenceInterval = NULL;

	for (;;)
		if (match ("INTERVAL"))
			sequenceInterval = parseNumber();
		else if (match ("SYSTEM_ID"))
			systemId = parseNumber();
		else
			break;

	return makeNode(type, name, sequenceInterval, systemId);
}

Syntax* SQLParse::parseForeignKey(Syntax *constraint)
{
	Syntax *identifier = parseIdentifier();
	Syntax *names = parseOptNames();
	LinkedList options;

	if (match ("ON"))
		{
		if (match ("DELETE"))
			{
			if (match ("CASCADE"))
				options.append (makeNode(nod_cascade_delete));
			else
				error ("CASCADE");
			}
		else
			error ("DELETE");
		}

	return makeNode(nod_foreign_key, constraint, identifier, names, makeNode(options));
}

Syntax* SQLParse::renameTable()
{
	LinkedList list;

	for (;;)
		{
		Syntax *from = parseIdentifier();
		require("TO");
		Syntax *to = parseIdentifier();
		list.append(makeNode(nod_list, from, to));

		if (!match(","))
			break;
		}

	return makeNode(nod_rename_table, list);
}

Syntax* SQLParse::parseIndexFields(void)
{
	if (!match ("("))
		error ("name list");

	LinkedList list;

	for (;;)
		{
		Syntax *name = parseName();
		Syntax *partial = NULL;
		
		if (match("("))
			{
			partial = parseNumber();
			parseEndParen();
			}
			
		list.append (makeNode(nod_index_segment, name, partial));
		
		if (!match (","))
			break;
		}

	parseEndParen();

	return makeNode(list);
}

Syntax* SQLParse::parseCreateTableSpace(SyntaxType nodeType)
{
	Syntax *name = parseName();

	if (!match("FILENAME"))
		error("FILENAME");

	Syntax *fileName = parseQuotedString();
	Syntax *initialAllocation = NULL;
	
	if (match("ALLOCATION"))
		initialAllocation = parseNumber();
		
	return makeNode(nodeType, name, fileName, initialAllocation);
}
