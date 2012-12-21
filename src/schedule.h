
#ifndef NINJA_SCHEDULE_H_
#define NINJA_SCHEDULE_H_

#include <set>
#include <queue>

#include "graph.h"

struct Scheduler {
  Scheduler();

  void Schedule(Edge* edge);
  Edge* NextUnit();
  int UnitsWaiting();

private:
  std::set<Edge*> ready_;
  std::priority_queue<Edge*, std::vector<Edge*>, EdgeWeightComparison> prioritized_;
};

#endif

