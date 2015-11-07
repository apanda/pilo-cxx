#include "node.h"
#include "link.h"
#include "packet.h"
#include "controller.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <forward_list>
#include <memory>
#include <boost/algorithm/string.hpp>
#include <igraph/igraph.h>  // Graph processing (for the masses).
#ifndef __TE_CONTROLLER_H__
#define __TE_CONTROLLER_H__

namespace PILO {
class Switch;

// A class to keep logs. Currently this is very inefficient in terms of memory, however
// the memory inefficiency is lower than my laziness.
class Log;

// Equivalent to TEController in Python
class TeController : public Controller {
    // The simulation is our friend
    friend class Simulation;

   public:
    TeController(Context& context, const std::string& name, const Time referesh, const Time gossip, const int max_load,
                 Distribution<bool>* drop);

    // igraph is not C++, and allocates memory. So be nice and remove things.
    virtual ~TeController() {}

   protected:
    const int _maxLoad;
    // Compute paths, return a diff of what needs to be fixed.
    virtual std::pair<flowtable_db, deleted_entries> compute_paths();
};
}
#endif
