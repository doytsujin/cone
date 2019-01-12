/** Handling for references
 * @file
 *
 * This source file is part of the Cone Programming Language C compiler
 * See Copyright Notice in conec.h
*/

#include "../ir.h"

// Create a new reference type whose info will be filled in afterwards
RefNode *newRefNode() {
    RefNode *refnode;
    newNode(refnode, RefNode, RefTag);
    refnode->tuptype = NULL;
    return refnode;
}

// Is type a nullable reference?
int refIsNullable(INode *typenode) {
    RefNode *ref = (RefNode*)typenode;
    return ref->tag == RefTag && (ref->flags & FlagRefNull);
}

// Define fat pointer type tuple for slice: {*T, usize}
void refSliceFatPtr(RefNode *reftype) {
    reftype->flags |= FlagArrSlice;
    PtrNode *refptr = newPtrNode();
    refptr->pvtype = reftype->pvtype;

    // Name-resolved usize type
    NameUseNode *size = newNameUseNode(nametblFind("usize", 5));
    size->tag = TypeNameUseTag;
    size->dclnode = (INamedNode*)usizeType;
    size->vtype = usizeType->vtype;

    TTupleNode *fatptr = newTTupleNode(2);
    nodesAdd(&fatptr->types, (INode*)refptr);
    nodesAdd(&fatptr->types, (INode*)size);
    reftype->tuptype = fatptr;
}

// Serialize a pointer type
void refPrint(RefNode *node) {
    inodeFprint("&(");
    inodePrintNode(node->alloc);
    inodeFprint(" ");
    inodePrintNode((INode*)node->perm);
    inodeFprint(" ");
    if (node->flags & FlagArrSlice)
        inodeFprint("[]");
    inodePrintNode(node->pvtype);
    inodeFprint(")");
}

// Semantically analyze a reference node
void refPass(PassState *pstate, RefNode *node) {
    inodeWalk(pstate, &node->alloc);
    inodeWalk(pstate, (INode**)&node->perm);
    inodeWalk(pstate, &node->pvtype);
    if (node->flags & FlagArrSlice)
        inodeWalk(pstate, (INode**)&node->tuptype);

    if (pstate->pass == TypeCheck) {
        if (node->flags & FlagArrSlice) {
            INode *perm = iexpGetTypeDcl(node->perm);
            if (perm != (INode*)immPerm && perm != (INode*)uniPerm && perm != (INode*)constPerm)
                errorMsgNode(node->perm, ErrorBadSlice, "Unsafe permission for slice");
        }
    }
}

// Compare two reference signatures to see if they are equivalent
int refEqual(RefNode *node1, RefNode *node2) {
    return itypeIsSame(node1->pvtype,node2->pvtype) 
        && permIsSame(node1->perm, node2->perm)
        && node1->alloc == node2->alloc;
}

// Will from reference coerce to a to reference (we know they are not the same)
int refMatches(RefNode *to, RefNode *from) {
    if (0 == permMatches(to->perm, from->perm)
        || (to->alloc != from->alloc && to->alloc != voidType))
        return 0;
    if ((to->flags & FlagArrSlice) != (from->flags & FlagArrSlice))
        return 0;
    return itypeMatches(to->pvtype, from->pvtype) == 1 ? 1 : 2;
}

// If self needs to auto-ref or auto-deref, make sure it legally can
int refAutoRefCheck(INode *selfnode, INode *totype) {
    INode *selftype = iexpGetTypeDcl(selfnode);
    totype = itypeGetTypeDcl(totype);

    // Auto-deref, if we have a ref but we need a value
    if (selftype->tag == RefTag && totype->tag != RefTag) {
        int match = itypeMatches(totype, ((RefNode*)selftype)->pvtype);
        if (match == 1 || match == 2)
            return 1;
    }
    // Auto-ref, if we have a value but need a ref
    else if (selftype->tag != RefTag && totype->tag == RefTag && ((RefNode*)totype)->alloc == voidType) {
        int match = itypeMatches(((RefNode*)totype)->pvtype, selftype);
        if (selfnode->tag != VarNameUseTag || match == 0 || match > 2)
            return 0;
        return permMatches(((RefNode*)totype)->perm, ((VarDclNode*)((NameUseNode*)selfnode)->dclnode)->perm);
    }
    return 0;
}

// Auto-ref or auto-deref self node (we already know it is legal)
void refAutoRef(INode **selfnodep, INode *totype) {
    INode *selftype = iexpGetTypeDcl(*selfnodep);
    totype = itypeGetTypeDcl(totype);

    int match = itypeMatches(totype, selftype);
    if (match == 1 || match == 2)
        return;

    // Auto-deref, if we have a ref but we need a value
    if (selftype->tag == RefTag && totype->tag != RefTag) {
        derefAuto(selfnodep);
        return;
    }

    // Auto-ref, if we have a value (as variable), but we need a ref
    if (selftype->tag != RefTag && totype->tag == RefTag) {
        addrAuto(selfnodep, totype);
        return;
    }
}