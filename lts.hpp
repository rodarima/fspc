#ifndef __LTS__H__
#define __LTS__H__

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <set>

#include "symbols_table.hpp"

using namespace std;


struct Edge {
    int dest;
    int action;
};

struct LtsNode {
    vector<Edge> children;
    unsigned int type; /* set if the node is an END node */

    static const int Normal = 0;
    static const int End = 1;
    static const int Error = 2;
};

typedef void (*LtsVisitFunction)(int, const struct LtsNode&, void*);

struct LtsVisitObject {
    LtsVisitFunction vfp;
    void * opaque;
};

class Lts: public SymbolValue {
    vector<LtsNode> nodes;
    ActionsTable * atp;
    int ntr;	/* Number of transactions */

    set<int> alphabet;

    void compose(const Lts& p, const Lts& q);
    void compositionReduce(const vector<LtsNode>& product);
	
    friend void lts_convert(struct ProcessNode * pnp, void * opaque);

  public:
    Lts(struct ActionsTable * p) : atp(p), ntr(0) { }
    Lts(int, struct ActionsTable *); /* One state Lts: Stop, End or Error */
    Lts(const struct ProcessNode *, struct ActionsTable *);
    Lts(const char * filename);  // TODO remove or update it
    Lts(const Lts& p, const Lts& q); /* Parallel composition */
    int numStates() const { return nodes.size(); }
    int numTransitions() const { return ntr; }
    int deadlockAnalysis() const;
    int terminalSets() const;
    Lts& compose(const Lts& q);
    Lts& labeling(const SetValue& labels);
    Lts& labeling(const string& label);
    Lts& sharing(const SetValue& labels);
    Lts& relabeling(const SetValue& newlabels, const SetValue& oldlabels);
    Lts& relabeling(const SetValue& newlabels, const string& oldlabel);
    void visit(const struct LtsVisitObject&) const;
    void graphvizOutput(const char * filename) const;

    void updateAlphabet(int action);
    int lookupAlphabet(int action) const;
    void printAlphabet() const;


    void print() const;
    int type() const { return SymbolValue::Lts; }
    SymbolValue * clone() const;
};

Lts * err_if_not_lts(SymbolValue * svp);

#endif
