#include <list>
#include "controller.h"
#ifndef __COORD_CONTROLLER_H__
#define __COORD_CONTROLLER_H__
namespace PILO {
    class CoordinationController;
    
    class Coordinator {
        friend class Simulation;
        public:
            void RegisterController(CoordinationController*);
            static std::shared_ptr<Coordinator> GetInstance() {
                if (!_instance) {
                    _instance = std::make_shared<Coordinator>();
                }
                return _instance;
            }
            void receive(CoordinationController* controller, 
                    std::shared_ptr<Packet> packet, Link* link);
            Coordinator() : _rtt(0.0), _lastTime(0.0) {}
        protected:
            void send_to_controller(std::shared_ptr<Packet> packet, Link* link);
            void set_rtt(double rtt) { _rtt = rtt; }
            void set_context(Context* context);
            std::list<CoordinationController*> _controllers;
            static std::shared_ptr<Coordinator> _instance;
            std::unordered_map<std::string, uint32_t> _lastSeen; 
            double _rtt;
            Time _lastTime;
            Context* _context;
    };

	class CoordinationController : public Controller {
	    public:    
            CoordinationController(Context& context,
                 const std::string& name,
                 const Time referesh,
                 const Time gossip,
                 Distribution<bool>* drop);
            virtual void receive(std::shared_ptr<Packet> packet, Link* link);
            virtual void receive_coordinator(std::shared_ptr<Packet> packet,
                                             Link* link);
	    protected:
            std::shared_ptr<Coordinator> _coordinator;
	};
}
#endif
