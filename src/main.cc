#include <iostream>
#include <list>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <boost/random/uniform_int.hpp>
#include <yaml-cpp/yaml.h>
#include "context.h"
#include "simulation.h"
#include "link.h"
#include "node.h"
#include "distributions.h"
#include "packet.h"

namespace po = boost::program_options;
typedef std::unordered_map<std::string, PILO::Node> node_map;
typedef std::unordered_map<std::string, PILO::Link> link_map;

int main(int argc, char* argv[]) {
    std::string topology;
    std::string configuration;
    uint32_t seed;
    PILO::Time refresh;
    PILO::BPS bw;
    PILO::Time end_time;
    PILO::Time measure;
    PILO::Time mttf;
    PILO::Time mttr;
    PILO::Time gossip;
    std::unordered_map<PILO::Time, double> converged;
    bool one_link = false;
    bool crit_link = false;
    std::string fail_link;
    int flow_limit = 1;

    // Argument parsing
    po::options_description args("PILO simulation");
    args.add_options()
        ("help,h", "Display help")
        ("topology,t", po::value<std::string>(&topology),
           "Simulation topology")
        ("configuration,c", po::value<std::string>(&configuration),
           "Simulation parameters")
        ("seed,s", po::value<uint32_t>(&seed)->default_value(42),
            "Random seed")
        ("refresh,p", po::value<PILO::Time>(&refresh)->default_value(300.0),
            "Controller <--> Switch refresh timeout")
        ("bandwidth,b", po::value<PILO::Time>(&bw)->default_value(1e10),
            "Link bandwidth")
        ("end,e", po::value<PILO::Time>(&end_time)->default_value(36000.0),
            "End time")
        ("measure,m", po::value<PILO::Time>(&measure)->default_value(10.0),
            "Measurement frequency")
        ("mttf,f", po::value<PILO::Time>(&mttf)->default_value(600.0),
            "Mean time to failure")
        ("mttr,r", po::value<PILO::Time>(&mttr)->default_value(300.0),
            "Mean time to recovery")
        ("gossip,g", po::value<PILO::Time>(&gossip)->default_value(600.0),
            "Time between Gossip")
        ("one,o",  "Simulate single link failure")
        ("critlinks,i", "Only fail switch <--> switch links")
        ("fail", po::value<std::string>(&fail_link),
            "Fail a specific link")
        ("l,limit", po::value<int>(&flow_limit)->default_value(100),
            "TE (L)imit");
    po::variables_map vmap;
    po::store(po::command_line_parser(argc, argv).options(args).run(), vmap);
    po::notify(vmap);
    if (vmap.count("help")) {
        std::cerr << args << std::endl;
        return 0;
    }

    if (!vmap.count("topology")) {
        std::cerr << "Topology not specified" << std::endl;
        return 0;
    }

    if (!vmap.count("configuration")) {
        std::cerr << "Configuration not specified" << std::endl;
        return 0;
    }

    one_link = vmap.count("one");
    crit_link = vmap.count("critlinks");

    std::cout << "Simulation setting limit to " << flow_limit << std::endl;
    PILO::Simulation simulation(seed, configuration, topology, end_time, refresh, gossip, bw, flow_limit);
    simulation.set_all_links_up_silent();
    simulation.install_all_routes();
    std::cout << "Pre run check = " << simulation.check_routes() << std::endl;

    //auto link = simulation.random_link();
    //simulation._context.schedule(11.0, [&](PILO::Time) {simulation.set_link_down(link);});

    for (PILO::Time time = measure; time <= end_time; time+=measure) {
        simulation._context.schedule(time, [&](PILO::Time t) {converged[t] = simulation.check_routes();});
    }

    std::cout << "Setting up trace" << std::endl;

    // Exponential as a way to get Poisson
    auto mttf_distro = PILO::ExponentialDistribution<PILO::Time>(1.0 / (1000.0 * mttf), simulation.rng());
    auto mttr_distro = PILO::ExponentialDistribution<PILO::Time>(1.0 / (1000.0 * mttr), simulation.rng());

    PILO::Time last_fail = 0;
    size_t tsize = 0;
    if (vmap.count("fail")) {
        auto link = simulation.get_link(fail_link);
        last_fail += mttf_distro.next();
        std::cout << last_fail << "  " << link->name() << "  down" << std::endl;
        simulation._context.scheduleAbsolute(last_fail, [&simulation, link](PILO::Time t) {
                            std::cout << simulation._context.now() << "  Setting down " << link->name() << std::endl;
                                simulation.set_link_down(link);});
    } else {
        do {
            std::shared_ptr<PILO::Link> link;
            if (crit_link) {
                link = simulation.random_switch_link();
            } else {
                link = simulation.random_link();
            }
            last_fail += mttf_distro.next();
            PILO::Time recovery = last_fail + mttr_distro.next();
            std::cout << last_fail << "  " << link->name() << "  down" << std::endl;
            std::cout << recovery << "  " << link->name() << "  up" << std::endl;
            simulation._context.scheduleAbsolute(last_fail, [&simulation, link](PILO::Time t) {
                                std::cout << simulation._context.now() << "  Setting down " << link->name() << std::endl;
                                simulation.set_link_down(link);});
            if (!one_link) {
                simulation._context.scheduleAbsolute(recovery, [&simulation, link](PILO::Time t) {
                                    std::cout << simulation._context.now() << "  Setting up " << link->name() << std::endl;
                                    simulation.set_link_up(link);});
            }
            tsize++;

        } while (last_fail < end_time && !one_link);
        std::cout << "Done setting up trace " << tsize << std::endl;
    }

    simulation.run();
    std::cout << "Fin." << std::endl;
    std::cout << "Convergence " << std::endl;
    for (PILO::Time time = measure; time <= end_time; time+=measure) {
        std::cout << " !  " << time << " " << converged.at(time) << std::endl;
    }

    simulation.dump_bw_used();
    return 0;
}
