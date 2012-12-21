// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "state.h"

#include <assert.h>
#include <stdio.h>

#include "edit_distance.h"
#include "graph.h"
#include "metrics.h"
#include "util.h"
#include "schedule.h"

void Pool::EdgeScheduled(const Edge& edge) {
  if (depth_ != 0)
    current_use_ += edge.weight();
}

void Pool::EdgeFinished(const Edge& edge) {
  if (depth_ != 0)
    current_use_ -= edge.weight();
}

void Pool::DelayEdge(Edge* edge) {
  assert(depth_ != 0);
  delayed_.push_back(edge);
}

void Pool::RetrieveReadyEdges(set<Edge*>* ready_queue) {
  while (!delayed_.empty()) {
    Edge* edge = delayed_.front();
    if (current_use_ + edge->weight() > depth_)
      break;
    delayed_.pop_front();
    ready_queue->insert(edge);
    EdgeScheduled(*edge);
  }
}

void Pool::RetrieveReadyEdges(Scheduler& scheduler) {
  while (!delayed_.empty())
  {
    Edge* edge = delayed_.front();
    if (current_use_ + edge->weight() > depth_)
      break;
    delayed_.pop_front();
    scheduler.Schedule(edge);
    EdgeScheduled(*edge);
  }
}

void Pool::Dump() const {
  printf("%s (%d/%d) ->\n", name_.c_str(), current_use_, depth_);
  for (deque<Edge*>::const_iterator it = delayed_.begin();
       it != delayed_.end(); ++it)
  {
    printf("\t");
    (*it)->Dump();
  }
}

Pool State::kDefaultPool("", 0);
const Rule State::kPhonyRule("phony");

State::State() {
  AddRule(&kPhonyRule);
  AddPool(&kDefaultPool);
}

void State::AddRule(const Rule* rule) {
  assert(LookupRule(rule->name()) == NULL);
  rules_[rule->name()] = rule;
}

const Rule* State::LookupRule(const string& rule_name) {
  map<string, const Rule*>::iterator i = rules_.find(rule_name);
  if (i == rules_.end())
    return NULL;
  return i->second;
}

void State::AddPool(Pool* pool) {
  assert(LookupPool(pool->name()) == NULL);
  pools_[pool->name()] = pool;
}

Pool* State::LookupPool(const string& pool_name) {
  map<string, Pool*>::iterator i = pools_.find(pool_name);
  if (i == pools_.end())
    return NULL;
  return i->second;
}

Edge* State::AddEdge(const Rule* rule, Pool* pool) {
  Edge* edge = new Edge();
  edge->rule_ = rule;
  edge->pool_ = pool;
  edge->env_ = &bindings_;
  edges_.push_back(edge);
  return edge;
}

Node* State::GetNode(StringPiece path) {
  Node* node = LookupNode(path);
  if (node)
    return node;
  node = new Node(path.AsString());
  paths_[node->path()] = node;
  return node;
}

Node* State::LookupNode(StringPiece path) {
  METRIC_RECORD("lookup node");
  Paths::iterator i = paths_.find(path);
  if (i != paths_.end())
    return i->second;
  return NULL;
}

Node* State::SpellcheckNode(const string& path) {
  const bool kAllowReplacements = true;
  const int kMaxValidEditDistance = 3;

  int min_distance = kMaxValidEditDistance + 1;
  Node* result = NULL;
  for (Paths::iterator i = paths_.begin(); i != paths_.end(); ++i) {
    int distance = EditDistance(
        i->first, path, kAllowReplacements, kMaxValidEditDistance);
    if (distance < min_distance && i->second) {
      min_distance = distance;
      result = i->second;
    }
  }
  return result;
}

void State::AddIn(Edge* edge, StringPiece path) {
  Node* node = GetNode(path);
  edge->inputs_.push_back(node);
  node->AddOutEdge(edge);
}

void State::AddOut(Edge* edge, StringPiece path) {
  Node* node = GetNode(path);
  edge->outputs_.push_back(node);
  if (node->in_edge()) {
    Warning("multiple rules generate %s. "
            "build will not be correct; continuing anyway",
            path.AsString().c_str());
  }
  node->set_in_edge(edge);
}

bool State::AddDefault(StringPiece path, string* err) {
  Node* node = LookupNode(path);
  if (!node) {
    *err = "unknown target '" + path.AsString() + "'";
    return false;
  }
  defaults_.push_back(node);
  return true;
}

vector<Node*> State::RootNodes(string* err) {
  vector<Node*> root_nodes;
  // Search for nodes with no output.
  for (vector<Edge*>::iterator e = edges_.begin(); e != edges_.end(); ++e) {
    for (vector<Node*>::iterator out = (*e)->outputs_.begin();
         out != (*e)->outputs_.end(); ++out) {
      if ((*out)->out_edges().empty())
        root_nodes.push_back(*out);
    }
  }

  if (!edges_.empty() && root_nodes.empty())
    *err = "could not determine root nodes of build graph";

  assert(edges_.empty() || !root_nodes.empty());
  return root_nodes;
}

vector<Node*> State::DefaultNodes(string* err) {
  return defaults_.empty() ? RootNodes(err) : defaults_;
}

void State::Reset() {
  for (Paths::iterator i = paths_.begin(); i != paths_.end(); ++i)
    i->second->ResetState();
  for (vector<Edge*>::iterator e = edges_.begin(); e != edges_.end(); ++e)
    (*e)->outputs_ready_ = false;
}

void State::Dump() {
  for (Paths::iterator i = paths_.begin(); i != paths_.end(); ++i) {
    Node* node = i->second;
    printf("%s %s\n",
           node->path().c_str(),
           node->status_known() ? (node->dirty() ? "dirty" : "clean")
                                : "unknown");
  }
  if (!pools_.empty()) {
    printf("resource_pools:\n");
    for (map<string, Pool*>::const_iterator it = pools_.begin();
         it != pools_.end(); ++it)
    {
      if (!it->second->name().empty()) {
        it->second->Dump();
      }
    }
  }
}
