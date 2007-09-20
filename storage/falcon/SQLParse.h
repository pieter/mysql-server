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
 *	MODULE:			Parse.h
 *	DESCRIPTION:	SQL Parser class definition
 *
 * copyright (c) 1997 by James A. Starkey
 */

#ifndef __PARSE_H
#define __PARSE_H

#include "Syntax.h"
#include "Stack.h"
#include "LinkedList.h"
#include "JString.h"

#define WHITE_SPACE				" \t\n\r"
#define PUNCTUATION_CHARS		".+-*/%()*<>=!;,?{}[]:~^|"
#define MULTI_CHARS				"+=*/%!~<>~^|&="
#define MAX_TOKEN				1024

#define WHITE					1
#define PUNCT					2
#define TERM					(WHITE | PUNCT)
#define MULTI_CHAR				4

enum TokenType {
	 PUNCTUATION_CHAR,
	 NAME,
	 QUOTED_NAME,
	 NUMBER,
	 END,
	 QUOTED_STRING,
	 SINGLE_QUOTED_STRING,
	 DECIMAL_NUMBER,
	 IP_ADDRESS,
	 };

//class Database;
class Input;
class Syntax;
CLASS(Field);
class Table;
class SymbolManager;

class  SQLParse 
{
public:
	SQLParse ();
	~SQLParse();
	Syntax*		parse (JString sqlStr, SymbolManager *symbolManager);
	Syntax*		renameTable();
	Syntax*		parseForeignKey (Syntax *constraint);
	Syntax*		parseSchema(SyntaxType type);
	Syntax*		parseDomain (SyntaxType type);
	Syntax*		parseRepository  (SyntaxType nodeType);
	Syntax*		parseRepair();
	Syntax*		makeNode (SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3, Syntax *child4);
	Syntax*		parseAddress();
	
	void		require (const char *keyword);
	void		saveBackup (int position);
	void		saveEnd ();
	int			saveBegin();

protected:
    void	deleteNodes();
	Syntax	*error (const char* expected);
	bool	keyword (const char *s);
	void	makeToken (int from, int to);
	bool	 match (const char *s);
	void 	nextToken ();
	void 	getToken ();

	Syntax* makeNode (SyntaxType type);
	Syntax* makeNode (SyntaxType type, int count);
	Syntax* makeNode (SyntaxType type, Syntax* child);
	Syntax* makeNode (SyntaxType type, Syntax* child1, Syntax* child2);
	Syntax* makeNode (SyntaxType type, Syntax* child1, Syntax* child2, Syntax* child3);
	Syntax* makeNode (SyntaxType type, const char *value);
	Syntax* makeNode (SyntaxType type, LinkedList &list);
	Syntax* makeNode (LinkedList &list);

	Syntax* parseAdd();
	Syntax* parseAlterNameSpace();
	Syntax* parseAlterTable ();
	Syntax* parseAlterTrigger();
	Syntax* parseAlterUser();
	Syntax* parseAnd();
	Syntax* parseAssumeRole();
	Syntax* parseBoolean ();
	Syntax* parseConstant ();
	Syntax* parseCreateCoterie (SyntaxType nodeType);
	Syntax* parseCreateFilterSet (SyntaxType nodeType);
	Syntax* parseCreateIndex (bool unique, SyntaxType nodeType);
	Syntax* parseCreateTable (SyntaxType nodeType);
	Syntax* parseCreateRole(SyntaxType nodeType);
	Syntax* parseCreateTrigger (SyntaxType nodType);
	Syntax* parseCreateUser();
	Syntax* parseCreateView (SyntaxType nodeType);
	Syntax* parseCreateTableSpace(SyntaxType nodeType);
	Syntax* parseDataType ();
	Syntax* parseDecimalNumber ();
	Syntax* parseDelete ();
	void 	parseEndParen ();
	Syntax* parseExpr ();
	Syntax* parseExprList ();
	Syntax* parseField ();
	Syntax* parseFieldClause ();
	Syntax* parseForeignKey ();
	Syntax* parseGrant();
	Syntax* parseIdentifier ();
	Syntax* parseIn (Syntax* expr);
	Syntax* parseIndexOptions();
	Syntax* parseInsert(SyntaxType nodType);
	Syntax* parseJoin();
	void 	parseKey ();
	Syntax* parseKeyOptions (bool multiple);
	Syntax* parseMultiply();
	Syntax* parseName ();
	Syntax* parseNames ();
	Syntax* parseNot ();
	Syntax* parseNumber ();
	Syntax* parseOptNames ();
	Syntax* parseOr();
	Syntax* parseOrder ();
	Syntax* parseParameter ();
	void 	parseParen ();
	Syntax* parsePrivilege();
	Syntax* parseQuotedString ();
	Syntax* parseRevoke();
	Syntax* parseSelect ();
	Syntax* parseSelectClause ();
	Syntax* parseSelectExpr ();
	Syntax* parseSequence (SyntaxType nodeType);
	Syntax* parseSimpleBoolean ();
	Syntax* parseSimpleExpr ();
	Syntax* parseStatement ();
	Syntax* parseTable (bool alias);
	Syntax* parseTableConstraint();
	Syntax* parseUpdate ();
	void	registerNode (Syntax* node);
	char 	skipWhite ();

	char		token [MAX_TOKEN], buffer [512];
	const char	*string;
	enum TokenType	tokenType;
	int			pos, length, tokenStart;
	Input		*input;
	Syntax		*syntax;
	Syntax		*nodes;
	//Database	*database;
	JString		sqlString;
	bool		upcase;
	bool		trace;
	bool		ipAddress;
	SymbolManager	*symbolManager;
	
public:

	LinkedList	fields;
	LinkedList	tables;
	Syntax* parseIndexFields(void);
};

#endif
