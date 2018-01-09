/** Parse expressions
 * @file
 *
 * The parser translates the lexer's tokens into AST nodes
 *
 * This source file is part of the Cone Programming Language C compiler
 * See Copyright Notice in conec.h
*/

#include "parser.h"
#include "../ast/ast.h"
#include "../shared/symbol.h"
#include "../shared/memory.h"
#include "../shared/error.h"
#include "lexer.h"

#include <stdio.h>
#include <assert.h>

// Parse a term: literal, identifier, etc.
AstNode *parseTerm() {
	switch (lex->toktype) {
	case trueToken:
	{
		ULitAstNode *node = newULitNode(1, (AstNode*)boolType);
		lexNextToken();
		return (AstNode *)node;
	}
	case falseToken:
	{
		ULitAstNode *node = newULitNode(0, (AstNode*)boolType);
		lexNextToken();
		return (AstNode *)node;
	}
	case IntLitToken:
		{
			ULitAstNode *node = newULitNode(lex->val.uintlit, lex->langtype);
			lexNextToken();
			return (AstNode *)node;
		}
	case FloatLitToken:
		{
			FLitAstNode *node = newFLitNode(lex->val.floatlit, lex->langtype);
			lexNextToken();
			return (AstNode *)node;
		}
	case IdentToken:
		{
			NameUseAstNode *node = newNameUseNode(lex->val.ident);
			lexNextToken();
			return (AstNode*)node;
		}
	case LParenToken:
		{
			AstNode *node;
			lexNextToken();
			node = parseExpr();
			parseRParen();
			return node;
		}
	default:
		errorMsgLex(ErrorBadTerm, "Invalid term value: expected variable, literal, etc.");
		return NULL;
	}
}

// Parse the postfix operators: '.', '::', '()'
AstNode *parsePostfix() {
	AstNode *node = parseTerm();
	while (1) {
		switch (lex->toktype) {

		// Function call with possible parameters
		case LParenToken:
		{
			FnCallAstNode *fncall = newFnCallAstNode(node, 8);
			lexNextToken();
			if (!lexIsToken(RParenToken))
				while (1) {
					nodesAdd(&fncall->parms, parseExpr());
					if (lexIsToken(CommaToken))
						lexNextToken();
					else
						break;
				}
			parseRParen();
			node = (AstNode *)fncall;
			break;
		}

		// Object call with possible parameters
		case DotToken:
		{
			lexNextToken();
			// Get field/method name
			if (!lexIsToken(IdentToken)) {
				errorMsgLex(ErrorNoMbr, "This should be a named field/method");
				lexNextToken();
				break;
			}
			AstNode *method = (AstNode*)newNameUseNode(lex->val.ident);
			lexNextToken();
			method->asttype = FieldNameUseNode;
			
			// If parameters provided, make this a function call
			// (where FieldNameUseNode signals it is an OO call)
			if (lexIsToken(LParenToken)) {
				lexNextToken();
				FnCallAstNode *fncall = newFnCallAstNode(method, 8);
				nodesAdd(&fncall->parms, node); // treat object as first parameter (self)
				if (!lexIsToken(RParenToken)) {
					while (1) {
						nodesAdd(&fncall->parms, parseExpr());
						if (lexIsToken(CommaToken))
							lexNextToken();
						else
							break;
					}
				}
				parseRParen();
				node = (AstNode *)fncall;
			}
			else {
				assert(0 && "Add logic to handle object member with no parameters!");
			}
		}

		default:
			return node;
		}
	}
}

// Parse a prefix operator, e.g.: -
AstNode *parsePrefix() {
	if (lexIsToken(DashToken)) {
		FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("neg", 3)), 1);
		lexNextToken();
		nodesAdd(&node->parms, parsePrefix());
		return (AstNode *)node;
	}
	else if (lexIsToken(TildeToken)) {
		FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("~", 1)), 1);
		lexNextToken();
		nodesAdd(&node->parms, parsePrefix());
		return (AstNode *)node;
	}
	else if (lexIsToken(AmperToken)) {
		AddrAstNode *node = newAddrAstNode();
		lexNextToken();

		PtrTypeAstNode *ptype = newPtrTypeNode();
		ptype->alloc = voidType;
		ptype->pvtype = voidType;
		if (lex->val.ident->node && lex->val.ident->node->asttype == PermNameDclNode) {
			ptype->perm = (PermAstNode*)((NameDclAstNode *)lex->val.ident->node)->value;
			lexNextToken();
		}
		else
			ptype->perm = constPerm;

		node->vtype = (AstNode *)ptype;
		node->exp = parsePrefix();
		return (AstNode *)node;
	}
	else if (lexIsToken(StarToken)) {
		DerefAstNode *node = newDerefAstNode();
		lexNextToken();
		node->exp = parsePrefix();
		return (AstNode *)node;
	}
	return parsePostfix();
}

// Parse binary multiply, divide, rem operator
AstNode *parseMult() {
	AstNode *lhnode = parsePrefix();
	while (1) {
		if (lexIsToken(StarToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("*", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parsePrefix());
			lhnode = (AstNode*)node;
		}
		else if (lexIsToken(SlashToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("/", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parsePrefix());
			lhnode = (AstNode*)node;
		}
		else if (lexIsToken(PercentToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("%", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parsePrefix());
			lhnode = (AstNode*)node;
		}
		else
			return lhnode;
	}
}

// Parse binary add, subtract operator
AstNode *parseAdd() {
	AstNode *lhnode = parseMult();
	while (1) {
		if (lexIsToken(PlusToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("+", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parseMult());
			lhnode = (AstNode*)node;
		}
		else if (lexIsToken(DashToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("-", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parseMult());
			lhnode = (AstNode*)node;
		}
		else
			return lhnode;
	}
}

// Parse bitwise And
AstNode *parseAnd() {
	AstNode *lhnode = parseAdd();
	while (1) {
		if (lexIsToken(AmperToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("&", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parseAdd());
			lhnode = (AstNode*)node;
		}
		else
			return lhnode;
	}
}

// Parse bitwise Xor
AstNode *parseXor() {
	AstNode *lhnode = parseAnd();
	while (1) {
		if (lexIsToken(CaretToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("^", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parseAnd());
			lhnode = (AstNode*)node;
		}
		else
			return lhnode;
	}
}

// Parse bitwise or
AstNode *parseOr() {
	AstNode *lhnode = parseXor();
	while (1) {
		if (lexIsToken(BarToken)) {
			FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind("|", 1)), 2);
			lexNextToken();
			nodesAdd(&node->parms, lhnode);
			nodesAdd(&node->parms, parseXor());
			lhnode = (AstNode*)node;
		}
		else
			return lhnode;
	}
}

// Parse comparison operator
AstNode *parseCmp() {
	AstNode *lhnode = parseOr();
	char *cmpop;
	int cmpsz = 2;

	switch (lex->toktype) {
	case EqToken:  cmpop = "=="; break;
	case NeToken:  cmpop = "!="; break;
	case LtToken:  cmpop = "<"; cmpsz = 1; break;
	case LeToken:  cmpop = "<="; break;
	case GtToken:  cmpop = ">"; cmpsz = 1; break;
	case GeToken:  cmpop = ">="; break;
	default: return lhnode;
	}

	FnCallAstNode *node = newFnCallAstNode((AstNode*)newFieldUseNode(symFind(cmpop, cmpsz)), 2);
	lexNextToken();
	nodesAdd(&node->parms, lhnode);
	nodesAdd(&node->parms, parseOr());
	return (AstNode*)node;
}

// Parse 'not' logical operator
AstNode *parseNotLogic() {
	if (lexIsToken(NotToken)) {
		LogicAstNode *node = newLogicAstNode(NotLogicNode);
		lexNextToken();
		node->lexp = parseNotLogic();
		return (AstNode*)node;
	}
	return parseCmp();
}

// Parse 'and' logical operator
AstNode *parseAndLogic() {
	AstNode *lhnode = parseNotLogic();
	while (lexIsToken(AndToken)) {
		LogicAstNode *node = newLogicAstNode(AndLogicNode);
		lexNextToken();
		node->lexp = lhnode;
		node->rexp = parseNotLogic();
		lhnode = (AstNode*)node;
	}
	return lhnode;
}

// Parse 'or' logical operator
AstNode *parseOrLogic() {
	AstNode *lhnode = parseAndLogic();
	while (lexIsToken(OrToken)) {
		LogicAstNode *node = newLogicAstNode(OrLogicNode);
		lexNextToken();
		node->lexp = lhnode;
		node->rexp = parseAndLogic();
		lhnode = (AstNode*)node;
	}
	return lhnode;
}

// Parse an assignment expression
AstNode *parseAssign() {
	AstNode *lval = parseOrLogic();
	if (lexIsToken(AssgnToken)) {
		lexNextToken();
		AstNode *rval = parseExpr();
		return (AstNode*) newAssignAstNode(NormalAssign, lval, rval);
	}
	else
		return lval;
}

AstNode *parseExpBlock() {
	switch (lex->toktype) {
	case IfToken:
		return parseIf();
	case LCurlyToken:
		return parseBlock();
	default:
		return parseAssign();
	}
}

// Parse an expression
AstNode *parseExpr() {
	return parseExpBlock();
}