/** Handling for while nodes
 * @file
 *
 * This source file is part of the Cone Programming Language C compiler
 * See Copyright Notice in conec.h
*/

#ifndef loop_h
#define loop_h

// Loop expression node
typedef struct LoopNode {
    INodeHdr;
    INode *blk;
} LoopNode;

LoopNode *newLoopNode();
void loopPrint(LoopNode *wnode);

// while block name resolution
void loopNameRes(NameResState *pstate, LoopNode *node);

// Type check the while block (conditional expression must be coercible to bool)
void loopTypeCheck(TypeCheckState *pstate, LoopNode *wnode);

// Perform data flow analysis on an while statement
void loopFlow(FlowState *fstate, LoopNode **nodep);

#endif