/*
 *  fspc parse tree
 *
 *  Copyright (C) 2013  Vincenzo Maffione
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <sstream>

#include "tree.hpp"
#include "driver.hpp"
#include "unresolved.hpp"
#include "helpers.hpp"
#include "utils.hpp"

using namespace std;
using namespace fsp;


//#define DEBUG
#ifdef DEBUG
#define IFD(x) x
#else
#define IFD(x)
#endif


/* Helper function used to update the 'unres' table.
   name: the name of the (possibly local) process to assign
         an alias
   lts:  the LTS corresponding the the (possibly local) process,
         which can also be a LTS containing a single unresolved
         node
   define: true if 'name' is on the left side of an FSP assignement
*/
static void update_unres(FspDriver& c, const string& name,
                         fsp::SmartPtr<fsp::Lts> lts,
                         bool define, const fsp::location& loc)
{
    unsigned int ui;

    if (define && c.unres.defined(name)) {
        /* A process name cannot be defined twice. */
        stringstream errstream;
        errstream << "Process " << name << " defined twice";
        semantic_error(c, errstream, loc);
    }

    if (lts->get_priv(0) == LtsNode::NoPriv) {
        /* If 'lts[0]' does not have its 'priv' set, we must be in one of
           the following two cases:
                - 'lts[0]' is an unresolved node, and so we have to assign
                  a new UnresolvedName alias (an 'idx') to it, for subsequent
                  name resolution (Lts::resolve). In this case 'define' is
                  false.
                - 'lts[0]' is not unresolved and so we have to assign a new
                   alias (an 'idx') to it, for subsequent name resolution
                  (Lts::resolve). In this case 'define' is true.
        */
        ui = c.unres.insert(name, define); /* Create a new entry for 'name' */
        lts->set_priv(0, ui);  /* Record the alias into the 'lts[0]' priv. */
    } else {
        /* If 'lts[0]' does have its 'priv' set, it means that 'lts[0]'
           is not unresolved, and there is already an alias assigned to it.

           Tell 'unres' that the 'lts[0]' priv field must be an alias also
           for 'name'. */
        ui = c.unres.append(name, lts->get_priv(0), define);
        /* Update all the 'priv' fields that have the 'idx' previously
           associated to 'name', if any. */
        if (ui != LtsNode::NoPriv) {
            lts->replace_priv(lts->get_priv(0), ui);
        }
    }
}

fsp::TreeNode::~TreeNode()
{
    for (unsigned int i=0; i<children.size(); i++)
        if (children[i])
            delete children[i];
}

string fsp::TreeNode::getClassName() const
{
    return "TreeNode";
}

void fsp::TreeNode::addChild(fsp::TreeNode *n, const fsp::location& loc)
{
    children.push_back(n);
    if (n) {
        n->loc = loc;
    }
}

void fsp::TreeNode::print(ofstream& os)
{
    vector<TreeNode *> frontier;
    TreeNode *current;
    unsigned int pop = 0;
    bool print_nulls = false;  /* Do we want to print null nodes? (i.e. optional symbols) */

    os << "digraph G {\n";
    frontier.push_back(this);

    while (pop != frontier.size()) {
        string label = "NULL";

        current = frontier[pop];
        if (current) {
            LowerCaseIdNode *ln;
            UpperCaseIdNode *un;
            IntegerNode *in;

            label = current->getClassName();
            ln = tree_downcast_safe<LowerCaseIdNode>(current);
            un = tree_downcast_safe<UpperCaseIdNode>(current);
            in = tree_downcast_safe<IntegerNode>(current);
            assert(!(in && ln) && !(ln && un) && !(un && in));
            if (ln && label == "LowerCaseId") {
                label = ln->content;
            } else if (un && label == "UpperCaseId") {
                label = un->content;
            } else if (in) {
                label = int2string(in->value);
            }
        }
        if (current || print_nulls)
            os << pop << " [label=\"" << label << "\", style=filled];\n";
        if (current) {
            for (unsigned int i=0; i<current->children.size(); i++) {
                if (current->children[i] || print_nulls) {
                    frontier.push_back(current->children[i]);
                    os << pop << " -> " << frontier.size()-1 << " [label=\"\"];\n";
                }
            }
        }
        pop++;
    }

    os << "}\n";
}

Result *fsp::TreeNode::translate(FspDriver& c)
{
    /* Never get to here. */
    assert(0);

    return NULL;
}

void fsp::TreeNode::getNodesByClasses(const vector<string>& classes,
                            vector<TreeNode *>& results)
{
    queue<TreeNode *> frontier;

    results.clear();
    frontier.push(this);

    while (!frontier.empty()) {
        TreeNode *cur = frontier.front();

        frontier.pop();

        for (unsigned int i = 0; i < classes.size(); i++) {
            if (cur->getClassName() == classes[i]) {
                results.push_back(cur);
                break;
            }
        }

        for (unsigned int i = 0; i < cur->children.size(); i++) {
            if (cur->children[i]) {
                frontier.push(cur->children[i]);
            }
        }
    }
}

/* ========================== Translation methods ======================== */

Result *fsp::LowerCaseIdNode::translate(FspDriver& c)
{
    StringResult *str = new StringResult;

    str->val = content;

    return str;
}

Result *fsp::UpperCaseIdNode::translate(FspDriver& c)
{
    StringResult *str = new StringResult;

    str->val = content;

    return str;
}

Result *fsp::VariableIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::ConstantIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::RangeIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::SetIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::ConstParameterIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::ParameterIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::ProcessIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::ProgressIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::MenuIdNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[0]->translate(c));

    return id;
}

Result *fsp::ExpressionNode::translate(FspDriver& c)
{
    if (children.size() == 1) {
        RDC(IntResult, expr, children[0]->translate(c));

        return expr;
    } else if (children.size() == 2) {
        /* OPERATOR EXPR */
        TDC(OperatorNode, on, children[0]);
        RDC(IntResult, expr, children[1]->translate(c));

        if (on->sign == "+") {
        } else if (on->sign == "-") {
            expr->val = -expr->val;
        } else if (on->sign == "!") {
            expr->val = !expr->val;
        } else {
            assert(0);
        }

        return expr;
    } else if (children.size() == 3) {
        TDCS(OpenParenNode, pn, children[0]);

        if (pn) {
            /* ( EXPR ) */
            RDC(IntResult, expr, children[1]->translate(c));

            return expr;
        } else {
            /* EXPR OPERATOR EXPR */
            RDC(IntResult, l, children[0]->translate(c));
            TDC(OperatorNode, o, children[1]);
            RDC(IntResult, r, children[2]->translate(c));
            IntResult *expr = new IntResult;

            if (o->sign == "||") {
                expr->val = l->val || r->val;
            } else if (o->sign == "&&") {
                expr->val = l->val && r->val;
            } else if (o->sign == "|") {
                expr->val = l->val | r->val;
            } else if (o->sign == "^") {
                expr->val = l->val ^ r->val;
            } else if (o->sign == "&") {
                expr->val = l->val & r->val;
            } else if (o->sign == "==") {
                expr->val = (l->val == r->val);
            } else if (o->sign == "!=") {
                expr->val = (l->val != r->val);
            } else if (o->sign == "<") {
                expr->val = (l->val < r->val);
            } else if (o->sign == ">") {
                expr->val = (l->val > r->val);
            } else if (o->sign == "<=") {
                expr->val = (l->val <= r->val);
            } else if (o->sign == ">=") {
                expr->val = (l->val >= r->val);
            } else if (o->sign == "<<") {
                expr->val = l->val << r->val;
            } else if (o->sign == ">>") {
                expr->val = l->val >> r->val;
            } else if (o->sign == "+") {
                expr->val = l->val + r->val;
            } else if (o->sign == "-") {
                expr->val = l->val - r->val;
            } else if (o->sign == "*") {
                expr->val = l->val * r->val;
            } else if (o->sign == "/") {
                expr->val = l->val / r->val;
            } else if (o->sign == "%") {
                expr->val = l->val % r->val;
            } else {
                assert(0);
            }
            delete l;
            delete r;

            return expr;
        }
    } else {
        assert(0);
    }

    return NULL;
}

Result *fsp::BaseExpressionNode::translate(FspDriver& c)
{
    TDCS(IntegerNode, in, children[0]);
    TDCS(VariableIdNode, vn, children[0]);
    TDCS(ConstParameterIdNode, cn, children[0]);

    if (in) {
        IntResult *result = new IntResult;

        /* We don't need to translate here, we have a literal integer. */
        result->val = in->value;

        return result;
    } else if (vn) {
        RDC(StringResult, id, children[0]->translate(c));
        string val;
        int v;

        if (!c.ctx.lookup(id->val, val)) {
            stringstream errstream;
            errstream << "variable " << id->val << " undeclared";
            semantic_error(c, errstream, loc);
        }
        delete id;

        if (string2int(val, v)) {
            stringstream errstream;
            errstream << "string '" << val << "' is not a number";
            semantic_error(c, errstream, loc);
        }

        return new IntResult(v);
    } else if (cn) {
        RDC(StringResult, id, children[0]->translate(c));
        Symbol *svp;
        IntS *cvp;

        if (!c.identifiers.lookup(id->val, svp)) {
            stringstream errstream;
            errstream << "const/parameter " << id->val << " undeclared";
            semantic_error(c, errstream, loc);
        }
        cvp = err_if_not<IntS>(c, svp, loc);
        delete id;

        return new IntResult(cvp->value);
    } else {
        assert(0);
    }

    return NULL;
}

Result *fsp::RangeExprNode::translate(FspDriver& c)
{
    RDC(IntResult, l, children[0]->translate(c));
    RDC(IntResult, r, children[2]->translate(c));
    /* Build a range from two expressions. */
    RangeResult *range = new RangeResult(l->val, r->val);

    delete l;
    delete r;

    return range;
}

Result *fsp::RangeNode::translate(FspDriver& c)
{
    TDCS(RangeIdNode, ri, children[0]);
    TDCS(RangeExprNode, re, children[0]);

    if (ri) {
        /* Lookup the range identifier. */
        RDC(StringResult, id, children[0]->translate(c));
        Symbol *svp;
        RangeS *rvp;
        RangeResult *range = new RangeResult;

        if (!c.identifiers.lookup(id->val, svp)) {
            stringstream errstream;
            errstream << "range " << id->val << " undeclared";
            semantic_error(c, errstream, loc);
        }
        rvp = err_if_not<RangeS>(c, svp, loc);
        range->val = *rvp;
        delete id;

        return range;
    } else if (re) {
        /* Return the range expression. */
        RDC(RangeResult, range, children[0]->translate(c));

        return range;
    } else {
        assert(0);
    }

    return NULL;
}

Result *fsp::ConstantDefNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[1]->translate(c));
    RDC(IntResult, expr, children[3]->translate(c));
    IntS *cvp = new IntS;

    cvp->value = expr->val;
    if (!c.identifiers.insert(id->val, cvp)) {
        stringstream errstream;
        errstream << "const " << id->val << " declared twice";
        delete cvp;
        semantic_error(c, errstream, loc);
    }

    delete id;
    delete expr;

    return NULL;
}

Result *fsp::RangeDefNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[1]->translate(c));
    RDC(IntResult, l, children[3]->translate(c));
    RDC(IntResult, r, children[5]->translate(c));
    RangeS *rvp = new RangeS;

    rvp->low = l->val;
    rvp->high = r->val;
    if (!c.identifiers.insert(id->val, rvp)) {
        stringstream errstream;
        errstream << "range " << id->val << " declared twice";
        delete rvp;
        semantic_error(c, errstream, loc);
    }

    delete id;
    delete l;
    delete r;

    return NULL;
}

Result *fsp::SetDefNode::translate(FspDriver& c)
{
    RDC(StringResult, id, children[1]->translate(c));
    RDC(SetResult, se, children[3]->translate(c));
    SetS *svp = new SetS;

    *svp = se->val;
    if (!c.identifiers.insert(id->val, svp)) {
        stringstream errstream;
        errstream << "set " << id->val << " declared twice";
        delete svp;
        semantic_error(c, errstream, loc);
    }

    delete id;
    delete se;

    return NULL;
}

void fsp::ProgressDefNode::combination(FspDriver& c, Result *r,
                                      string index, bool first)
{
    RDC(StringResult, id, children[1]->translate(c));
    ProgressS *pv = new ProgressS;
    string name = id->val + index;

    if (children.size() == 5) {
        /* Progress definition in unconditional form (normal form). */
        RDC(SetResult, se, children[4]->translate(c));

        pv->conditional = false;
        se->val.toActionSetValue(c.actions, pv->set);
        delete se;
    } else if (children.size() == 8) {
        /* Progress definition in conditional form. */
        RDC(SetResult, cse, children[5]->translate(c));
        RDC(SetResult, se, children[7]->translate(c));

        pv->conditional = true;
        cse->val.toActionSetValue(c.actions, pv->condition);
        se->val.toActionSetValue(c.actions, pv->set);
        delete se;
        delete cse;
    } else {
        assert(0);
    }

    if (!c.progresses.insert(name, pv)) {
        stringstream errstream;
        errstream << "progress " << name << " declared twice";
        semantic_error(c, errstream, loc);
    }

    delete id;
}

static bool next_set_indexes(const vector<TreeNode *>& elements,
                             vector<unsigned int>& indexes,
                             vector<unsigned int>& limits)
{
    unsigned int j = indexes.size() - 1;

    assert(elements.size() == indexes.size());

    if (!elements.size()) {
        return false;
    }

    for (;;) {
        TDCS(StringTreeNode, strn, elements[j]);
        TDCS(SetNode, setn, elements[j]);
        TDCS(ActionRangeNode, an, elements[j]);

        if (strn) {
            /* This element contains just one action, and so there is
               noting to iterate over. In other words, we always wraparound.
               Just pass to the next element. */
        } else if (setn) {
            indexes[j]++;
            if (indexes[j] == limits[j]) {
                /* Wraparaund: continue with the next element. */
                indexes[j] = 0;
            } else {
                /* No wraparound: stop here. */
                break;
            }
        } else if (an) {
            indexes[j]++;
            if (indexes[j] == limits[j]) {
                /* Wraparaund: continue with the next element. */
                indexes[j] = 0;
            } else {
                /* No wraparound: stop here. */
                break;
            }
        } else {
            assert(0);
        }
        /* Continue with the next element, unless we are at the very
           last one: In the last case tell the caller that all the
           element combinations have been scanned. At this point
           'indexes' contain all zeroes. */
        if (j == 0) {
            return false;
        }
        j--;
    }

    return true; /* There are more combinations. */
}

static void for_each_combination(FspDriver& c, Result *r,
                                 const vector<TreeNode *>& elements,
                                 TreeNode *n)
{
    vector<unsigned int> indexes(elements.size());
    vector<unsigned int> limits(elements.size());
    Context ctx = c.ctx;  /* Save the original context. */
    bool first = true;

    /* Initialize the 'indexes' vector, used to iterate over all the
       possible index combinations. */
    for (unsigned int j=0; j<elements.size(); j++) {
        indexes[j] = 0;
        limits[j] = 1;
    }

    do {
        string index_string;

        /* Scan the ranges from the left to the right, computing the
           '[x][y][z]...' string corresponding to 'indexes'. */
        for (unsigned int j=0; j<elements.size(); j++) {
            Result *re;

            /* Here we do the translation that was deferred in the lower
               layers. */
            re = elements[j]->translate(c);
            TDC(ActionRangeNode, an, elements[j]);
            SetResult *ar = result_downcast<SetResult>(re);

            (void)an;
            index_string += "." + ar->val[ indexes[j] ];
            if (ar->val.hasVariable()) {
                if (!c.ctx.insert(ar->val.variable,
                            ar->val[ indexes[j] ])) {
                    cout << "ERROR: ctx.insert()\n";
                }
            }
            limits[j] = ar->val.size();
            delete re;
        }

        n->combination(c, r, index_string, first);
        first = false;

        /* Restore the saved context. */
        c.ctx = ctx;

        /* Increment 'indexes' for the next 'index_string', and exits if
           there are no more combinations. */
    } while (next_set_indexes(elements, indexes, limits));
}

Result *fsp::ProgressDefNode::translate(FspDriver& c)
{
    RDC(TreeNodeVecResult, ir, children[2]->translate(c));

    for_each_combination(c, NULL, ir->val, this);

    delete ir;

    return NULL;
}

Result *fsp::MenuDefNode::translate(FspDriver &c)
{
    /* menu_id set */
    RDC(StringResult, id, children[1]->translate(c));
    RDC(SetResult, se, children[3]->translate(c));
    ActionSetS *asv = new ActionSetS;

    /* Turn the SetS contained into the SetNode into an
       ActionSetS. */
    se->val.toActionSetValue(c.actions, *asv);

    if (!c.menus.insert(id->val, asv)) {
        stringstream errstream;
        errstream << "menu " << id->val << " declared twice";
        semantic_error(c, errstream, loc);
    }

    return NULL;
}

/* This recursive method can be used to compute the set of action defined
   by an arbitrary complex label expression, e.g.
        'a[i:1..2].b.{h,j,k}.c[3][j:i..2*i][j*i+3]'
    The caller should pass a SetS object built using the
    default constructor (e.g. an empty SetS) to 'base', and 0 to 'idx'.
    The elements vector contains pointers to strings, sets or action ranges.
*/
SetS fsp::TreeNode::computeActionLabels(FspDriver& c, SetS base,
                                           const vector<TreeNode*>& elements,
                                           unsigned int idx)
{
    Result *r;

    assert(idx < elements.size());

    /* Here we do the translation that was deferred in the lower layers.
       This is necessary because of context expansion: When an action range
       defines a variable in the middle of a label expression, that variable
       can influence the translation of the expression elements which are on
       the right of the variable definition: In these cases, we need to
       retranslate those elements on the right many times, once for each
       possibile variable value.
    */
    r = elements[idx]->translate(c);
    if (idx == 0) {
        /* This is the first element of a label expression. We set 'base'
           to its initial value. */
        StringResult *str = result_downcast_safe<StringResult>(r);
        SetResult *se = result_downcast_safe<SetResult>(r);

        base = SetS();
        if (str) {
            /* Single action. */
            base += str->val;
         } else if (se) {
            /* A set of actions. */
            base += se->val;
        } else {
            assert(0);
        }
    } else {
        /* Here we are in the middle (or the end) of a label expression.
           We use the dotcat() or indexize() method to extend the current
           'base'. */
        TDCS(StringTreeNode, strn, elements[idx]);
        TDCS(SetNode, setn, elements[idx]);
        TDCS(ActionRangeNode, an, elements[idx]);

        if (strn) {
            StringResult *str = result_downcast<StringResult>(r);

            base.dotcat(str->val);
        } else if (setn) {
            SetResult *se = result_downcast<SetResult>(r);

            base.dotcat(se->val);
        } else if (an) {
            SetResult *ar = result_downcast<SetResult>(r);

            if (!ar->val.hasVariable() || idx+1 >= elements.size()) {
                /* When an action range doesn't define a variable, or when
                   such a declaration is useless since this is the end of
                   the expression, we just extend the current 'base'. */
                base.indexize(ar->val);
            } else {
                /* When an action range does define a variable, we must split
                   the computation in N parts, one for each action in the
                   action range, and then concatenate all the results. For
                   each part, we extend the current 'base' with only an
                   action, insert the variable into the context and do a
                   recursive call. */
                SetS ret;
                SetS next_base;
                bool ok;

                for (unsigned int j=0; j<ar->val.size(); j++) {
                    next_base = base;
                    next_base.indexize(ar->val[j]);
                    if (!c.ctx.insert(ar->val.variable, ar->val[j])) {
                        cout << "ERROR: ctx.insert()\n";
                    }
                    ret += computeActionLabels(c, next_base,
                                               elements, idx+1);
                    ok = c.ctx.remove(ar->val.variable);
                    assert(ok);
                }
                delete r;

                return ret;
            }
        } else {
            assert(0);
        }
    }
    delete r;

    if (idx+1 >= elements.size()) {
        /* If there are no more elements to scan, return what we have
           collected so far: The current base, which has been extended
           by the code above. */
        return base;
    }

    /* It there are more elements to scan, extend the current base with
       the rest of the elements and return the result. */
    return computeActionLabels(c, base, elements, idx+1);
}

Result *fsp::SetElementsNode::translate(FspDriver& c)
{
    SetResult *se = new SetResult;

    /* Here we have a list of ActionLabels. We compute the set of actions
       corresponding to each element by using the computeActionLabels()
       protected method, and concatenate all the results. */

    for (unsigned int i = 0; i < children.size(); i += 2) {
        RDC(TreeNodeVecResult, al, children[i]->translate(c));

        se->val += computeActionLabels(c, SetS(), al->val, 0);
        delete al;
    }

    return se;
}

Result *fsp::SetExprNode::translate(FspDriver& c)
{
    /* { SetElementsNode } */
    RDC(SetResult, se, children[1]->translate(c));

    return se;
}

Result *fsp::SetNode::translate(FspDriver& c)
{
    TDCS(SetIdNode, sin, children[0]);
    TDCS(SetExprNode, sen, children[0]);

    if (sin) {
        /* Lookup the set identifier. */
        RDC(StringResult, id, children[0]->translate(c));
        Symbol *svp;
        SetS *setvp;
        SetResult *se = new SetResult;

        if (!c.identifiers.lookup(id->val, svp)) {
            stringstream errstream;
            errstream << "set " << id->val << " undeclared";
            semantic_error(c, errstream, loc);
        }
        setvp = err_if_not<SetS>(c, svp, loc);
        delete id;
        se->val = *setvp;

        return se;
    } else if (sen) {
        /* Return the set expression. */
        RDC(SetResult, se, children[0]->translate(c));

        return se;
    } else {
        assert(0);
    }

    return NULL;
}

Result *fsp::ActionRangeNode::translate(FspDriver& c)
{
    SetResult *result = new SetResult;

    if (children.size() == 1) {
        /* Build a set of actions from an integer, a range or a set. */
        TDCS(ExpressionNode, en, children[0]);
        TDCS(RangeNode, rn, children[0]);
        TDCS(SetNode, sn, children[0]);

        if (en) {
            RDC(IntResult, expr, children[0]->translate(c));

            result->val += int2string(expr->val);
            delete expr;
        } else if (rn) {
            RDC(RangeResult, range, children[0]->translate(c));

            range->val.set(result->val);
            delete range;
        } else if (sn) {
            RDC(SetResult, se, children[0]->translate(c));

            result->val = se->val;
            delete se;
        } else {
            assert(0);
        }
    } else if (children.size() == 3) {
        /* Do the same with variable declarations. */
        RDC(StringResult, id, children[0]->translate(c));
        TDCS(RangeNode, rn, children[2]);
        TDCS(SetNode, sn, children[2]);

        if (rn) {
            RDC(RangeResult, range, children[2]->translate(c));

            range->val.set(result->val);
            delete range;
        } else if (sn) {
            RDC(SetResult, se, children[2]->translate(c));

            result->val = se->val;
            delete se;
        } else {
            assert(0);
        }
        result->val.variable = id->val;
        delete id;
    } else {
        assert(0);
    }

    return result;
}

Result *fsp::ActionLabelsNode::translate(FspDriver& c)
{
    /* Given an arbitrary complex label expression, e.g.
            'a[i:1..2].b.{h,j,k}.c[3][j:i..2*i][j*i+3]'
       this function collects all the children that make up the expression,
       i.e. a list of TreeNode* pointing to instances of LowerCaseIdNode,
       SetNode or ActionRangeNode.
       It's not necessary to call translate the children, since they
       will be translated in the upper layers.
    */
    TreeNodeVecResult *result = new TreeNodeVecResult;

    /* The leftmost part of the label expression: A single action
       or a set of actions. */
    do {
        TDCS(StringTreeNode, strn, children[0]);
        TDCS(SetNode, setn, children[0]);

        if (strn) {
            /* Single action. */
            result->val.push_back(strn);
        } else if (setn) {
            /* A set of actions. */
            result->val.push_back(setn);
        } else {
            assert(0);
        }
    } while (0);

    /* The rest of the expression. */
    for (unsigned int i=1; i<children.size();) {
        TDCS(PeriodNode, pn, children[i]);
        TDCS(OpenSquareNode, sqn, children[i]);

        if (pn) {
            /* Recognize the "." operator; Can follow a string or a set. */
            TDCS(StringTreeNode, strn, children[i+1]);
            TDCS(SetNode, setn, children[i+1]);

            if (strn) {
                result->val.push_back(strn);
            } else if (setn) {
                result->val.push_back(setn);
            } else {
                assert(0);
            }
            i += 2;
        } else if (sqn) {
            /* Recognize the "[]" operator; Must follow an action range. */
            TDC(ActionRangeNode, an, children[i+1]);

            result->val.push_back(an);
            i += 3;
        } else {
            assert(0);
            break;
        }
    }

    return result;
}

fsp::SmartPtr<fsp::Lts> fsp::TreeNode::computePrefixActions(FspDriver& c,
                                           const vector<TreeNode *>& als,
                                           unsigned int idx,
                                           vector<Context>& ctxcache)
{
    assert(idx < als.size());
    TDC(ActionLabelsNode, an, als[idx]);
    RDC(TreeNodeVecResult, vec, an->translate(c));
    const vector<TreeNode *>& elements = vec->val;
    vector<unsigned int> indexes(elements.size());
    vector<unsigned int> limits(elements.size());
    fsp::SmartPtr<fsp::Lts> lts = new Lts(LtsNode::Normal, &c.actions);
    Context ctx = c.ctx;

    /* Initialize the 'indexes' vector. */
    for (unsigned int j=0; j<elements.size(); j++) {
        indexes[j] = 0;
        limits[j] = 1;
    }

    do {
        string label;

        /* Scan the expression from the left to the right, computing the
           action label corresponding to 'indexes'. */
        for (unsigned int j=0; j<elements.size(); j++) {
            /* Here we do the translation that was deferred in the lower
               layers. This is necessary because of context expansion: When
               an action range defines a variable in the middle of a label
               expression, that variable can influence the translation of the
               expression elements which are on the right of the variable
               definition: In these cases, we need to retranslate those
               elements on the right many times, once for each
               possibile variable value.
            */
            Result *r;

            r = elements[j]->translate(c);
            if (j == 0) {
                /* This is the first element of a label expression. */
                StringResult *str = result_downcast_safe<StringResult>(r);
                SetResult *se = result_downcast_safe<SetResult>(r);

                if (str) {
                    /* Single action. */
                    label = str->val;
                } else if (se) {
                    /* A set of actions. */
                    label = se->val[ indexes[j] ];
                    limits[j] = se->val.size();
                } else {
                    assert(0);
                }
            } else {
                /* Here we are in the middle (or the end) of a label
                   expression. */
                TDCS(StringTreeNode, strn, elements[j]);
                TDCS(SetNode, setn, elements[j]);
                TDCS(ActionRangeNode, an, elements[j]);

                if (strn) {
                    StringResult *str = result_downcast_safe<StringResult>(r);

                    label += "." + str->val;
                } else if (setn) {
                    SetResult *se = result_downcast_safe<SetResult>(r);

                    label += "." + se->val[ indexes[j] ];
                    limits[j] = se->val.size();
                } else if (an) {
                    SetResult *ar = result_downcast_safe<SetResult>(r);

                    label += "." + ar->val[ indexes[j] ];
                    if (ar->val.hasVariable()) {
                        if (!c.ctx.insert(ar->val.variable,
                                    ar->val[ indexes[j] ])) {
                            cout << "ERROR: ctx.insert()\n";
                        }
                    }
                    limits[j] = ar->val.size();
                } else {
                    assert(0);
                }
            }
            delete r;
        }

        fsp::SmartPtr<fsp::Lts> next;
        if (idx+1 >= als.size()) {
            /* This was the last ActionLabels in the chain: We create an
               incomplete node which represent an Lts which is the result
               of a LocalProcessNode we will translate later (in the
               ActionPrefixNode::translate method). However, we have to
               save now the context that will be used in the deferred
               translation. The incomplete node stores an index which refers
               to a context in the 'ctxcache' array of saved contexts. */
            if (!ctxcache.size() || c.ctx != ctxcache.back()) {
                /* Optimization: Avoid to duplicate the last inserted
                   context. */
                ctxcache.push_back(c.ctx);
            }
            next = new Lts(LtsNode::Incomplete, &c.actions);
            /* Store the index in the 'priv' field. */
            next->set_priv(0, ctxcache.size() - 1);
        } else {
            /* This was not the last ActionLabels in the chain. Get
               the result of the remainder of the chain. */
            next = computePrefixActions(c, als, idx + 1, ctxcache);
        }

        /* Attach 'next' to 'lts' using 'label'. */
        lts->zerocat(*next, label);

        /* Restore the saved context. */
        c.ctx = ctx;

        /* Increment indexes for the next 'label', and exits if there
           are no more combinations. */
    } while (next_set_indexes(elements, indexes, limits));

    return lts;
}

Result *fsp::PrefixActionsNode::translate(FspDriver& c)
{
    TreeNodeVecResult *result = new TreeNodeVecResult;

    /* Here we have a chain of ActionLabels, e.g.
            't[1..2] -> g.y7 -> f[j:1..2][9] -> a[j+3].a.y'

       From such a chain we want to build an incomplete LTS, e.g. and LTS
       that lacks of some connections that will be completed by the upper
       ActionPrefix node.
    */
    for (unsigned int i=0; i<children.size(); i+=2) {
        TDC(ActionLabelsNode, an, children[i]);

        result->val.push_back(an);
    }

    return result;
}

Result *fsp::IndicesNode::translate(FspDriver& c)
{
    StringResult *result = new StringResult;

    /* [ EXPR ] [ EXPR ] ... [ EXPR ] */ 
    for (unsigned int i=0; i<children.size(); i+=3) {
        RDC(IntResult, expr, children[i+1]->translate(c));

        result->val += "." + int2string(expr->val);
        delete expr;
    }

    return result;
}

Result *fsp::BaseLocalProcessNode::translate(FspDriver& c)
{
    TDCS(EndNode, en, children[0]);
    TDCS(StopNode, sn, children[0]);
    TDCS(ErrorNode, ern, children[0]);
    LtsResult *result = new LtsResult;

    if (en) {
        result->val = new Lts(LtsNode::End, &c.actions);
    } else if (sn) {
        result->val = new Lts(LtsNode::Normal, &c.actions);
    } else if (ern) {
        result->val = new Lts(LtsNode::Error, &c.actions);
    } else {
        /* process_id indices_OPT */
        RDC(StringResult, id, children[0]->translate(c));
        TDCS(IndicesNode, ixn, children[1]);
        string name = id->val;

        /* Create an LTS containing a single unresolved
           node. */
        result->val = new Lts(LtsNode::Unresolved, &c.actions);
        if (ixn) {
            RDC(StringResult, idx, children[1]->translate(c));

            name += idx->val;
            delete idx;
        }
        /* Tell the unresolved names table that there
           is a new unresolved name (define is false
           because this is not a process definition,
           the process name is only referenced). */
        update_unres(c, name, result->val, false, loc);

        delete id;
    }

    return result;
}

Result *fsp::ChoiceNode::translate(FspDriver& c)
{
    LtsResult *result;

    assert(children.size());

    /* action_prefix | action_prefix | ... | action_prefix */
    do {
        RDC(LtsResult, ap, children[0]->translate(c));

        result = ap;
    } while (0);

    for (unsigned int i=2; i<children.size(); i+=2) {
        RDC(LtsResult, ap, children[i]->translate(c));

        result->val->zeromerge(*ap->val);
        delete ap;
    }

    return result;
}

Result *fsp::ArgumentListNode::translate(FspDriver& c)
{
    IntVecResult *result = new IntVecResult;

    /* EXPR , EXPR , ... , EXPR */
    for (unsigned int i = 0; i < children.size(); i += 2) {
        RDC(IntResult, expr, children[i]->translate(c));

        result->val.push_back(expr->val);
        delete expr;
    }

    return result;
}

Result *fsp::ArgumentsNode::translate(FspDriver& c)
{
    /* ( argument_list ) */
    RDC(IntVecResult, argl, children[1]->translate(c));

    return argl;
}

void fsp::process_ref_translate(FspDriver& c, const location& loc,
                               const string& name, const vector<int> *args,
                               fsp::SmartPtr<fsp::Lts> *res)
{
    Symbol *svp;
    ParametricProcess *pp;
    vector<int> arguments;
    string extension;

    assert(res);

    /* Lookup 'process_id' in the 'parametric_process' table. */
    if (!c.parametric_processes.lookup(name, svp)) {
	stringstream errstream;
	errstream << "Process " << name << " undeclared";
	semantic_error(c, errstream, loc);
    }
    pp = is<ParametricProcess>(svp);

    /* Find the arguments for the process parameters. */
    arguments = args ? *args : pp->defaults;

    if (arguments.size() != pp->defaults.size()) {
	stringstream errstream;
	errstream << "Parameters mismatch";
	semantic_error(c, errstream, loc);  // XXX tr.locations[1]
    }

    lts_name_extension(arguments, extension);

    /* We first lookup the global processes table in order to see whether
       we already have the requested LTS or we need to compute it. */
    IFD(cout << "Looking up " << name + extension << "\n");
    if (!c.processes.lookup(name + extension, svp)) {
        TreeNode *pdn;
        bool ok;

	/* If there is a cache miss, we have to compute the requested LTS
	   using the translate method and save it in the global processes
           table. */
        pdn = dynamic_cast<TreeNode *>(pp->translator);
        assert(pdn);
        /* Save and reset the compiler context. It must be called before
           inserting the parameters into 'c.parameters' (see the following
           for loop). */
        ok = c.nesting_save();
        if (!ok) {
            stringstream errstream;
            errstream << "Max reference depth exceeded while translating "
                        "process " << name + extension;
            general_error(c, errstream, loc);
        }
        /* Insert the arguments into the identifiers table, taking care
           of overridden names. */
        for (unsigned int i=0; i<pp->names.size(); i++) {
            Symbol *svp;
            IntS *cvp = new IntS();

            if (c.identifiers.lookup(pp->names[i], svp)) {
                /* If there is already an identifier with the same name as
                   the i-th parameter, override temporarly the identifier.
                */
                c.overridden_names.push_back(pp->names[i]);
                c.overridden_values.push_back(svp->clone());
                c.identifiers.remove(pp->names[i]);
            }

            cvp->value = arguments[i];
            if (!c.identifiers.insert(pp->names[i], cvp)) {
                assert(0);
                delete cvp;
            }
            /* Insert the parameter into 'c.parameters', which is part of
               the translator context. */
            c.parameters.insert(pp->names[i], arguments[i]);
        }
        /* Do the translation. The new LTS is stored in the 'processes'
           table by the translate function. */
        pdn->translate(c);
        /* Restore the previously saved compiler context. */
        c.nesting_restore();
    }

    /* Use the LTS stored in the 'processes' table. */
    if (c.processes.lookup(name + extension, svp)) {
	*res = is<fsp::Lts>(svp->clone());
    } else {
        assert(0);
    }
}

Result *fsp::ProcessRefSeqNode::translate(FspDriver& c)
{
    /* process_id arguments_OPT */
    RDC(StringResult, id, children[0]->translate(c));
    TDCS(ArgumentsNode, an, children[1]);
    LtsResult *lts = new LtsResult;
    IntVecResult *args = NULL;

    if (an) {
        RDC(IntVecResult, temp, children[1]->translate(c));

        args = temp;
    }

    process_ref_translate(c, loc, id->val, args ? &args->val : NULL,
                          &lts->val);

    delete id;
    if (args) {
        delete args;
    }

    return lts;
}

Result *fsp::SeqProcessListNode::translate(FspDriver& c)
{
    LtsResult *result;

    /* process_ref_seq ; process_ref_seq ; ... process_ref_seq */
    do {
        RDC(LtsResult, pr, children[0]->translate(c));

        result = pr;
    } while (0);

    for (unsigned int i=2; i<children.size(); i+=2) {
        RDC(LtsResult, pr, children[i]->translate(c));

        result->val->endcat(*pr->val);
        delete pr;
    }

    return result;
}

Result *fsp::SeqCompNode::translate(FspDriver& c)
{
    /* seq_process_list ; base_local_process */
    RDC(LtsResult, plist, children[0]->translate(c));
    RDC(LtsResult, localp, children[2]->translate(c));

    plist->val->endcat(*localp->val);
    delete localp;

    return plist;
}

Result *fsp::LocalProcessNode::translate(FspDriver& c)
{
    LtsResult *result;

    if (children.size() == 1) {
        TDCS(BaseLocalProcessNode, b, children[0]);
        TDCS(SeqCompNode, sc, children[0]);
        RDC(LtsResult, lts, children[0]->translate(c));

        assert(b || sc);
        result = lts;
    } else if (children.size() == 3) {
        /* ( choice ) */
        RDC(LtsResult, lts, children[1]->translate(c));

        result = lts;
    } else if (children.size() == 5) {
        /* IF expression THEN local_process else_OPT. */
        RDC(IntResult, expr, children[1]->translate(c));
        TDCS(ProcessElseNode, pen, children[4]);

        if (expr->val) {
            RDC(LtsResult, localp, children[3]->translate(c));

            result = localp;
        } else if (pen) {
            RDC(LtsResult, elsep, children[4]->translate(c));

            result = elsep;
        } else {
            result = new LtsResult;
            result->val = new Lts(LtsNode::Normal, &c.actions);
        }
        delete expr;
    } else {
        assert(0);
    }

    return result;
}

Result *fsp::ProcessElseNode::translate(FspDriver& c)
{
    /* ELSE local_process */
    RDC(LtsResult, localp, children[1]->translate(c));

    return localp;
}

Result *fsp::ActionPrefixNode::translate(FspDriver& c)
{
    vector<Context> ctxcache;
    Context saved_ctx = c.ctx;
    LtsResult *result = new LtsResult;

    /* guard_OPT prefix_actions local_process */
    TDCS(GuardNode, gn, children[0]);
    RDC(TreeNodeVecResult, pa, children[1]->translate(c));
    TDC(LocalProcessNode, lp, children[3]);
    IntResult *guard = NULL;

    if (gn) {
        RDC(IntResult, temp, children[0]->translate(c));

        guard = temp;
    }

    /* Don't translate 'lp', since it will be translated into the loop,
       with proper context. */

    if (!guard || guard->val) {
        vector<Lts> processes; /* XXX can this be vector<fsp::SmartPtr<fsp::Lts>> ?? */

        /* Compute an incomplete Lts, and the context related to
           each incomplete node (ctxcache). */
        result->val = computePrefixActions(c, pa->val, 0, ctxcache);
        /* Translate 'lp' under all the contexts in ctxcache. */
        for (unsigned int i=0; i<ctxcache.size(); i++) {
            LtsResult *lts;

            c.ctx = ctxcache[i];
            lts = result_downcast<LtsResult>(lp->translate(c));
            processes.push_back(*lts->val);
            delete lts;
        }

        /* Connect the incomplete Lts to the computed translations. */
        result->val->incompcat(processes);
    }
    if (guard) {
        delete guard;
    }
    delete pa;

    c.ctx = saved_ctx;

    return result;
}

Result *fsp::ProcessBodyNode::translate(FspDriver& c)
{
    RDC(LtsResult, localp, children[0]->translate(c)); /* local_process */

    if (children.size() == 1) {
    } else if (children.size() == 3) {
        /* local_process , local_process_defs */
        RDC(LtsResult, ldefs, children[2]->translate(c));

        localp->val->append(*ldefs->val, 0);
        delete ldefs;
    } else {
        assert(0);
    }

    return localp;
}

Result *fsp::AlphaExtNode::translate(FspDriver& c)
{
    /* + set */
    RDC(SetResult, se, children[1]->translate(c));

    return se;
}

void fsp::RelabelDefNode::combination(FspDriver& c, Result *r,
                                     string index, bool first)
{
    RDC(RelabelingResult, relab, children[2]->translate(c));
    RelabelingResult *result = result_downcast<RelabelingResult>(r);

    if (first) {
        result->val = relab->val;
    } else {
        result->val.merge(relab->val);
    }
    delete relab;
}

Result *fsp::RelabelDefNode::translate(FspDriver& c)
{
    assert(children.size() == 3);

    TDCS(ActionLabelsNode, left, children[0]);
    TDCS(ForallNode, fan, children[0]);
    RelabelingResult *relab = new RelabelingResult;

    if (left) {
        /* action_labels / action_labels */
        RDC(TreeNodeVecResult, l, children[0]->translate(c));
        RDC(TreeNodeVecResult, r, children[2]->translate(c));

        relab->val.add(computeActionLabels(c, SetS(), l->val, 0),
                       computeActionLabels(c, SetS(), r->val, 0));
        delete l;
        delete r;
    } else if (fan) {
        /* FORALL index_ranges braces_relabel_defs */
        RDC(TreeNodeVecResult, ir, children[1]->translate(c));

        /* Translate 'index_ranges' only, and rely on deferred translation
           for 'brace_relabel_defs'. */
        for_each_combination(c, relab, ir->val, this);
        delete ir;
    } else {
        assert(0);
    }

    return relab;
}

Result *fsp::RelabelDefsNode::translate(FspDriver& c)
{
    RelabelingResult *result;

    /* relabel_def , relabel_def , ... , relabel_def */
    do {
        RDC(RelabelingResult, rl, children[0]->translate(c));

        result = rl;
    } while (0);

    for (unsigned int i = 2; i < children.size(); i += 2) {
        RDC(RelabelingResult, rl, children[i]->translate(c));

        result->val.merge(rl->val);
    }

    return result;
}

Result *fsp::BracesRelabelDefsNode::translate(FspDriver& c)
{
    /* { relabel_defs }*/
    RDC(RelabelingResult, rl, children[1]->translate(c));

    return rl;
}

Result *fsp::RelabelingNode::translate(FspDriver& c)
{
    /* / braces_relabel_defs */
    RDC(RelabelingResult, rl, children[1]->translate(c));

    return rl;
}

Result *fsp::HidingInterfNode::translate(FspDriver& c)
{
    TDCS(HidingNode, hn, children[0]);
    TDCS(InterfNode, in, children[0]);
    RDC(SetResult, se, children[1]->translate(c));
    HidingResult *result = new HidingResult;

    if (hn) {
        result->val.interface = false;
    } else if (in) {
        result->val.interface = true;
    } else {
        assert(0);
    }

    result->val.setv = se->val;
    delete se;

    return result;
}

Result *fsp::IndexRangesNode::translate(FspDriver& c)
{
    /* [ action_range ][ action_range] ... [action_range] */
    TreeNodeVecResult *result = new TreeNodeVecResult;

    /* Translation is deferred: Just collect the children. */
    for (unsigned int i=0; i<children.size(); i+=3) {
        TDC(ActionRangeNode, arn, children[i+1]);

        result->val.push_back(arn);
    }

    return result;
}

void fsp::LocalProcessDefNode::combination(FspDriver& c, Result *r,
                                          string index, bool first)
{
    RDC(StringResult, id, children[0]->translate(c));
    /* Translate the LocalProcess using the current context. */
    RDC(LtsResult, lts, children[3]->translate(c));
    LtsResult *result = result_downcast<LtsResult>(r);

    /* Register the local process name (which is the concatenation of
       'process_id' and the 'index_string', e.g. 'P' + '[3][1]') into
       c.unres. The 'define' parameter is true since this is a (local)
       process definition. */
    update_unres(c, id->val + index, lts->val, true, loc);

    if (first) {
        result->val = lts->val;
    } else {
        result->val->append(*lts->val, 0);
    }

    delete id;
    delete lts;
}

Result *fsp::LocalProcessDefNode::translate(FspDriver& c)
{
    RDC(TreeNodeVecResult, ir, children[1]->translate(c));
    LtsResult *lts = new LtsResult;

    /* Only translate 'index_ranges', while 'process_id' and 'local_process'
       will be translated in the loop below. */
    for_each_combination(c, lts, ir->val, this);
    delete ir;

    return lts;
}

Result *fsp::LocalProcessDefsNode::translate(FspDriver& c)
{
    /* local_process_def , local_process_def, ... , local_process_def */
    LtsResult *result;

    do {
        RDC(LtsResult, lpd, children[0]->translate(c));

        result = lpd;
    } while (0);

    for (unsigned int i=2; i<children.size(); i+=2) {
        RDC(LtsResult, lpd, children[i]->translate(c));

        result->val->append(*lpd->val, 0);
        delete lpd;
    }

    return result;
}

void fsp::TreeNode::post_process_definition(FspDriver& c,
                                           fsp::SmartPtr<fsp::Lts> res,
                                           const string& name)
{
    string extension;

    res->name = name;
    res->cleanup();

    /* Compute the LTS name extension with the parameter values
       used with this translation. */
    lts_name_extension(c.parameters.defaults, extension);
    res->name += extension;

    /* Insert lts into the global 'processes' table. */
    IFD(cout << "Saving " << res->name << "\n");
    if (!c.processes.insert(res->name, res.delegate())) {
	stringstream errstream;

        delete res;
	errstream << "Process " << res->name + extension
                    << " already declared";
	semantic_error(c, errstream, loc);
    }
}

Result *fsp::ProcessDefNode::translate(FspDriver& c)
{
    /* property_OPT process_id process_body alpha_ext_OPT
       relabeling_OPT hiding_OPT */
    TDCS(PropertyNode, prn, children[0]);
    RDC(StringResult, id, children[1]->translate(c));
    RDC(LtsResult, body, children[4]->translate(c));
    TDCS(AlphaExtNode, aen, children[5]);
    TDCS(RelabelingNode, rln, children[6]);
    TDCS(HidingInterfNode, hin, children[7]);
    unsigned unres;

    /* The base is the process body. */

    /* Register the process name into c.unres (define is true since
       this is a process definition. */
    update_unres(c, id->val, body->val, true, loc);
#if 0
cout << "UnresolvedNames:\n";
for (unsigned int i=0; i<c.unres.size(); i++) {
    unsigned ui = c.unres.get_idx(i);

    if (ui != LtsNode::NoPriv) {
        cout << "   " << ui << " " << c.unres.get_name(i) << "\n";
    }
}
#endif

    /* Try to resolve all the unresolved nodes into the LTS. */
    unres = body->val->resolve();
    if (unres != LtsNode::NoPriv) {
        stringstream errstream;
        errstream << "process reference " << unres << " unresolved";
        semantic_error(c, errstream, loc);
    }

    /* Merge the End nodes. */
    body->val->mergeEndNodes();

    /* Extend the alphabet. */
    if (aen) {
        RDC(SetResult, alpha, aen->translate(c));
        SetS& sv = alpha->val;

        for (unsigned int i=0; i<sv.size(); i++) {
            body->val->updateAlphabet(c.actions.insert(sv[i]));
        }
        delete alpha;
    }

    /* Apply the relabeling operator. */
    if (rln) {
        RDC(RelabelingResult, rl, rln->translate(c));
        RelabelingS& rlv = rl->val;

        for (unsigned int i=0; i<rlv.size(); i++) {
            body->val->relabeling(rlv.new_labels[i], rlv.old_labels[i]);
        }
        delete rl;
    }

    /* Apply the hiding/interface operator. */
    if (hin) {
        RDC(HidingResult, hi, hin->translate(c));
        HidingS& hv = hi->val;

        body->val->hiding(hv.setv, hv.interface);
        delete hi;
    }

    if (prn) {
        /* Apply the property operator, if possible. */
        if (body->val->isDeterministic()) {
            body->val->property();
        } else {
            stringstream errstream;
            errstream << "Cannot apply the 'property' keyword since "
                << id->val << " is a non-deterministic process";
            semantic_error(c, errstream, loc);
        }
    }

    this->post_process_definition(c, body->val, id->val);
    delete body;
    delete id;

    return NULL;
}

Result *fsp::ProcessRefNode::translate(FspDriver& c)
{
    /* process_id arguments_OPT */
    RDC(StringResult, id, children[0]->translate(c));
    TDCS(ArgumentsNode, an, children[1]);
    LtsResult *lts = new LtsResult;
    IntVecResult *args = NULL;

    if (an) {
        RDC(IntVecResult, temp, children[1]->translate(c));

        args = temp;
    }

    process_ref_translate(c, loc, id->val, args ? &args->val : NULL,
                          &lts->val);

    delete id;
    if (args) {
        delete args;
    }

    return lts;
}

Result *fsp::SharingNode::translate(FspDriver& c)
{
    /* action_labels :: */
    RDC(TreeNodeVecResult, al, children[0]->translate(c));
    SetResult *result = new SetResult;

    result->val = computeActionLabels(c, SetS(), al->val, 0);
    delete al;

    return result;
}

Result *fsp::LabelingNode::translate(FspDriver& c)
{
    /* action_labels : */
    RDC(TreeNodeVecResult, al, children[0]->translate(c));
    SetResult *result = new SetResult;

    result->val = computeActionLabels(c, SetS(), al->val, 0);
    delete al;

    return result;
}

Result *fsp::PrioritySNode::translate(FspDriver& c)
{
    TDC(OperatorNode, on, children[0]);
    RDC(SetResult, se, children[1]->translate(c));
    PriorityResult *result = new PriorityResult;

    if (on->sign == ">>") {
        result->val.low = true;
    } else if (on->sign == "<<") {
        result->val.low = false;
    } else {
        assert(0);
    }
    result->val.setv = se->val;
    delete se;

    return result;
}

void fsp::CompositeBodyNode::combination(FspDriver& c, Result *r,
                                        string index, bool first)
{
    /* Translate the CompositedBodyNode using the current context. */
    RDC(LtsResult, cb, children[2]->translate(c));
    LtsResult *result = result_downcast<LtsResult>(r);

    if (first) {
        first = false;
        result->val = cb->val;
    } else {
        result->val->compose(*cb->val);
    }
    delete cb;
}

Result *fsp::CompositeBodyNode::translate(FspDriver& c)
{
    if (children.size() == 4) {
        /* sharing_OPT labeling_OPT process_ref relabel_OPT */
        TDCS(SharingNode, shn, children[0]);
        TDCS(LabelingNode, lbn, children[1]);
        RDC(LtsResult, pr, children[2]->translate(c));
        TDCS(RelabelingNode, rln, children[3]);

        /* Apply the process labeling operator. */
        if (lbn) {
            RDC(SetResult, lb, lbn->translate(c));

            pr->val->labeling(lb->val);
            delete lb;
        }

        /* Apply the process sharing operator. */
        if (shn) {
            RDC(SetResult, sh, shn->translate(c));

            pr->val->sharing(sh->val);
            delete sh;
        }

        /* Apply the relabeling operator. */
        if (rln) {
            RDC(RelabelingResult, rl, rln->translate(c));
            RelabelingS& rlv = rl->val;

            for (unsigned int i=0; i<rlv.size(); i++) {
                pr->val->relabeling(rlv.new_labels[i], rlv.old_labels[i]);
            }
            delete rl;
        }

        return pr;
    } else if (children.size() == 5) {
        /* IF expression THEN composity_body composite_else_OPT */
        RDC(IntResult, expr, children[1]->translate(c));
        TDCS(CompositeElseNode, cen, children[4]);
        LtsResult *lts = NULL;

        if (expr->val) {
            RDC(LtsResult, cb, children[3]->translate(c));

            lts = cb;
        } else if (cen) {
            RDC(LtsResult, ce, children[4]->translate(c));

            lts = ce;
        } else {
            lts = new LtsResult;
            lts->val = new Lts(LtsNode::Normal, &c.actions);
        }
        delete expr;

        return lts;
    } else if (children.size() == 6) {
        /* sharing_OPT labeling_OPT ( parallel_composition ) relabeling_OPT
         */
        TDCS(SharingNode, shn, children[0]);
        TDCS(LabelingNode, lbn, children[1]);
        RDC(LtsVecResult, pc, children[3]->translate(c));
        TDCS(RelabelingNode, rln, children[5]);
        LtsResult *lts = new LtsResult;

        /* Apply the process labeling operator to each component process
           separately, before parallel composition. */
        if (lbn) {
            RDC(SetResult, lb, lbn->translate(c));

            for (unsigned int k=0; k<pc->val.size(); k++) {
                pc->val[k]->labeling(lb->val);
            }
            delete lb;
        }

        /* Apply the process sharing operator (same way). */
        if (shn) {
            RDC(SetResult, sh, shn->translate(c));

            for (unsigned int k=0; k<pc->val.size(); k++) {
                pc->val[k]->sharing(sh->val);
            }
            delete sh;
        }
        /* Apply the relabeling operator (same way). */
        if (rln) {
            RDC(RelabelingResult, rl, rln->translate(c));
            RelabelingS& rlv = rl->val;

            for (unsigned int k=0; k<pc->val.size(); k++) {
                for (unsigned int i=0; i<rlv.size(); i++) {
                    pc->val[k]->relabeling(rlv.new_labels[i],
                                           rlv.old_labels[i]);
                }
            }
            delete rl;
        }

        /* Apply parallel composition. */
        assert(pc->val.size());
        lts->val = pc->val[0];
        for (unsigned int k=1; k<pc->val.size(); k++) {
            lts->val->compose(*pc->val[k]);
        }
        delete pc;

        return lts;
    } else if (children.size() == 3) {
        /* FORALL index_ranges composite_body */
        RDC(TreeNodeVecResult, ir, children[1]->translate(c));
        LtsResult *lts = new LtsResult;

        /* Only translate 'index_ranges', while 'composite_body'
           will be translated in the loop below. */
        for_each_combination(c, lts, ir->val, this);
        delete ir;

        return lts;
    } else {
        assert(0);
    }

    return NULL;
}

Result *fsp::CompositeElseNode::translate(FspDriver& c)
{
    /* ELSE composite_body */
    RDC(LtsResult, cb, children[1]->translate(c));

    return cb;
}

Result *fsp::ParallelCompNode::translate(FspDriver& c)
{
    /* composite_body || composite_body || .. || composite_body */
    LtsVecResult *result = new LtsVecResult;

    for (unsigned int i=0; i<children.size(); i+=2) {
        RDC(LtsResult, cb, children[i]->translate(c));

        result->val.push_back(cb->val);
        delete cb;
    }

    return result;
}

Result *fsp::CompositeDefNode::translate(FspDriver& c)
{
    /* || process_id param_OPT = composite_body priority_OPT hiding_OPT . */
    RDC(StringResult, id, children[1]->translate(c));
    RDC(LtsResult, body, children[4]->translate(c));
    TDCS(PrioritySNode, prn, children[5]);
    TDCS(HidingInterfNode, hin, children[6]);

    /* The base is the composite body. */

    /* Apply the priority operator. */
    if (prn) {
        RDC(PriorityResult, pr, prn->translate(c));

        body->val->priority(pr->val.setv, pr->val.low);
        delete pr;
    }

    /* Apply the hiding/interface operator. */
    if (hin) {
        RDC(HidingResult, hi, hin->translate(c));
        HidingS& hv = hi->val;

        body->val->hiding(hv.setv, hv.interface);
        delete hi;
    }

    this->post_process_definition(c, body->val, id->val);
    delete id;
    delete body;

    return NULL;
}

