#include <iostream>
#include <iomanip>
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
    std::unordered_map<PILO::Time, uint32_t> max_load;
    bool one_link = false;
    bool crit_link = false;
    std::string fail_link;
    int flow_limit = 1;
    double drop_probablity = 0.0;
    bool fastforward;
    bool te;
    std::unique_ptr<PILO::Distribution<bool>> link_drop_distribution;
    std::unique_ptr<PILO::Distribution<bool>> ctrl_drop_distribution;
    //
    // Argument parsing
    po::options_description args("PILO simulation");
    args.add_options()("help,h", "Display help")("topology,t", po::value<std::string>(&topology),
                                                 "Simulation topology")(
        "configuration,c", po::value<std::string>(&configuration), "Simulation parameters")(
        "seed,s", po::value<uint32_t>(&seed)->default_value(42), "Random seed")(
        "refresh,p", po::value<PILO::Time>(&refresh)->default_value(300.0), "Controller <--> Switch refresh timeout")(
        "bandwidth,b", po::value<PILO::Time>(&bw)->default_value(1e10), "Link bandwidth")(
        "end,e", po::value<PILO::Time>(&end_time)->default_value(36000.0), "End time")(
        "measure,m", po::value<PILO::Time>(&measure)->default_value(10.0), "Measurement frequency")(
        "mttf,f", po::value<PILO::Time>(&mttf)->default_value(600.0), "Mean time to failure")(
        "mttr,r", po::value<PILO::Time>(&mttr)->default_value(300.0), "Mean time to recovery")(
        "gossip,g", po::value<PILO::Time>(&gossip)->default_value(600.0), "Time between Gossip")(
        "one,o", "Simulate single link failure")("critlinks,i", "Only fail switch <--> switch links")(
        "fail", po::value<std::string>(&fail_link), "Fail a specific link")(
        "limit,l", po::value<int>(&flow_limit)->default_value(100), "TE (L)imit")(
        "cdrop,w", "Drop at controller rather than link")(
        "drop,d", po::value<double>(&drop_probablity)->default_value(0.0), "Drop messages with some probability")(
        "fastforward", "Fast-forward to when failures happen")("te", "Measure link utilization for TE");
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

    fastforward = !(!vmap.count("fastforward"));
    te = !(!vmap.count("te"));

    boost::mt19937 rng(seed);

    if ((!vmap.count("cdrop")) && vmap.count("drop")) {
        std::cout << "Link drop enabled, probability " << drop_probablity << std::endl;
        link_drop_distribution = std::make_unique<PILO::BernoulliDistribution>(1.0 - drop_probablity, rng);
    } else {
        link_drop_distribution = std::make_unique<PILO::ConstantDistribution<bool>>(true);
    }

    if (vmap.count("cdrop") && vmap.count("drop")) {
        std::cout << "Ctrl drop enabled, probability " << drop_probablity << std::endl;
        ctrl_drop_distribution = std::make_unique<PILO::BernoulliDistribution>(1.0 - drop_probablity, rng);
    } else {
        ctrl_drop_distribution = std::make_unique<PILO::ConstantDistribution<bool>>(true);
    }

    one_link = vmap.count("one");
    crit_link = vmap.count("critlinks");

    std::cout << "Simulation setting limit to " << flow_limit << std::endl;
    PILO::Simulation simulation(seed, configuration, topology, end_time, refresh, gossip, bw, flow_limit,
                                std::move(link_drop_distribution), std::move(ctrl_drop_distribution));
    simulation.set_all_links_up_silent();
    simulation.install_all_routes();
    std::cout << "Pre run check = " << simulation.check_routes() << std::endl;

    std::cout << "Setting up trace" << std::endl;

    // Exponential as a way to get Poisson
    auto mttf_distro = PILO::ExponentialDistribution<PILO::Time>(1.0 / (1000.0 * mttf), simulation.rng());
    auto mttr_distro = PILO::ExponentialDistribution<PILO::Time>(1.0 / (1000.0 * mttr), simulation.rng());
    PILO::Time first_fail = 0;
    PILO::Time last_fail = 0;
    size_t tsize = 0;
    if (vmap.count("fail")) {
        auto link = simulation.get_link(fail_link);
        last_fail += mttf_distro.next();
        std::cout << last_fail << "  " << link->name() << "  down" << std::endl;
        first_fail = last_fail;
        simulation._context.scheduleAbsolute(last_fail, [&simulation, link](PILO::Time t) {
            std::cout << simulation._context.now() << "  Setting down " << link->name() << std::endl;
            simulation.set_link_down(link);
        });
    } else {
        do {
            std::shared_ptr<PILO::Link> link;
            if (crit_link) {
                link = simulation.random_switch_link();
            } else {
                link = simulation.random_link();
            }
            last_fail += mttf_distro.next();
            if (first_fail == 0) {
                first_fail = last_fail;
            }
            PILO::Time recovery = last_fail + mttr_distro.next();
            std::cout << last_fail << "  " << link->name() << "  down" << std::endl;
            std::cout << recovery << "  " << link->name() << "  up" << std::endl;
            simulation._context.scheduleAbsolute(last_fail, [&simulation, link](PILO::Time t) {
                std::cout << simulation._context.now() << "  Setting down " << link->name() << std::endl;
                simulation.set_link_down(link);
            });
            if (!one_link) {
                simulation._context.scheduleAbsolute(recovery, [&simulation, link](PILO::Time t) {
                    std::cout << simulation._context.now() << "  Setting up " << link->name() << std::endl;
                    simulation.set_link_up(link);
                });
            }
            tsize++;

        } while (last_fail < end_time && !one_link);
        std::cout << "Done setting up trace " << tsize << std::endl;
    }
    std::list<PILO::Time> samples;
    for (PILO::Time time = (fastforward ? first_fail : measure); time <= end_time; time += measure) {
        simulation._context.schedule(time, [&](PILO::Time t) {
            converged[t] = simulation.check_routes();
            samples.push_back(t);
        });
        if (te) {
            simulation._context.schedule(time, [&](PILO::Time t) {
                simulation.dump_link_usage();
                max_load[t] = simulation.max_link_usage();
                std::cout << t << " now" << std::endl;
            });
        }
    }

    simulation.run();
    std::cout << "Fin." << std::endl;
    std::cout << "Convergence " << std::endl;
    for (auto time : samples) {
        std::cout << " !  " << std::setprecision(5) << time << " " << std::setprecision(5) << converged.at(time);
        if (te) {
            std::cout << " " << max_load.at(time);
        }
        std::cout << std::endl;
    }

    simulation.dump_bw_used();
    return 0;
}
