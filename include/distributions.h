#include <yaml-cpp/yaml.h>
#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/random/uniform_int.hpp>
#ifndef __DISTRIBUTIONS_H__
#define __DISTRIBUTIONS_H__
// Random distributions of various kinds.
namespace {
     static const std::string DISTRO      = "distro";
     static const std::string NORMAL      = "normal";
     static const std::string CONSTANT    = "constant";
     static const std::string EXPONENTIAL = "exponential";
     static const std::string MEAN_KEY    = "mean";
     static const std::string SIGMA_KEY   = "stdev";
     static const std::string SHAPE_KEY   = "shape";
     static const PILO::Time  CONV_FACTOR = 1000.0; // Conversion from Python to s.
}
namespace PILO {

    template <typename T> class ConstantDistribution;
    template <typename T> class NormalDistribution;
    template <typename T> class ExponentialDistribution;
    template <typename T>
    class Distribution {
        public:
            // Get the next value from this distribution.
            virtual T next() = 0;

            // Convert a YAML node into a distribution.
            static Distribution<T>* get_distribution(const YAML::Node& node, boost::mt19937& rng) {
                if (node[DISTRO].as<std::string>() == NORMAL) {
                    return new NormalDistribution<T>(node, rng);
                } else if (node[DISTRO].as<std::string>() == CONSTANT) {
                    return new ConstantDistribution<T>(node);
                } else if (node[DISTRO].as<std::string>() == EXPONENTIAL) {
                    return new ExponentialDistribution<T>(node, rng);
                } else {
                    assert(false);
                    return NULL;
                }
            }
    };


    template<typename T>
    class ConstantDistribution : public Distribution<T> {
        private:
            T _value;
        public:
            ConstantDistribution(const YAML::Node& node) {
                _value = node[MEAN_KEY].as<T>() / CONV_FACTOR;
            }

            virtual T next() {
                return _value;
            }
    };

    template<typename T>
    class NormalDistribution : public Distribution<T> {
        private:
            boost::normal_distribution<T> _distro;
            boost::variate_generator<boost::mt19937&,
                                           boost::normal_distribution<T>> _var;
        public:
            NormalDistribution(const YAML::Node& node, boost::mt19937& rng) :
                _distro(node[MEAN_KEY].as<T>(), node[SIGMA_KEY].as<T>()),
                _var(rng, _distro) {
            }

            virtual T next() {
                // We treat returns of this type as meaning ms in Python. Convert to S.
                return _var() / CONV_FACTOR;
            }
    };

    template<typename T>
    class ExponentialDistribution : public Distribution<T> {
        private:
            boost::exponential_distribution<T> _distro;
            boost::variate_generator<boost::mt19937&,
                                           boost::exponential_distribution<T>> _var;
        public:
            ExponentialDistribution(const YAML::Node& node, boost::mt19937& rng) :
                _distro(node[SHAPE_KEY].as<T>()),
                _var(rng, _distro) {
            }

            ExponentialDistribution(const T shape, boost::mt19937& rng) :
                _distro(shape),
                _var(rng, _distro) {
            }

            virtual T next() {
                // We treat returns of this type as meaning ms in Python. Convert to S.
                return _var() / CONV_FACTOR;
            }
    };

    class UniformIntDistribution : public Distribution<int32_t> {
        private:
            boost::uniform_smallint<int32_t> _distro;
            boost::variate_generator<boost::mt19937&,
                    boost::uniform_smallint<int32_t>> _var;
        public:
            UniformIntDistribution(const int32_t min, const int32_t max, boost::mt19937& rng) :
                _distro(min, max),
                _var(rng, _distro) {
            }

            virtual int32_t next() {
                // Not for time like things, no conversion
                return _var();
            }
    };
}
#endif
