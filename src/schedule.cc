
#include <assert.h>
#include "schedule.h"
#include "explain.h"

using namespace std;

Scheduler::Scheduler() 
{
}

void Scheduler::Schedule(Edge* edge)
{
  assert(edge);

  set<Edge*>::iterator it = ready_.find(edge); 
  if (it == ready_.end())
  {
    // Non-idempotent, enqueue
    ready_.insert(edge);
    prioritized_.push(edge);
  }
}

Edge* Scheduler::NextUnit()
{
  if (ready_.empty())
  {
    return NULL;
  }

  Edge* edge = prioritized_.top();

  EXPLAIN("Dispatching [%d] - %s", edge->weight(), edge->GetDescription().c_str());

  prioritized_.pop();
  ready_.erase(edge);

  return edge;
}

int Scheduler::UnitsWaiting()
{
  return ready_.size();
}

