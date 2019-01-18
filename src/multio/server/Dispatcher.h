
#ifndef multio_server_Dispatcher_H
#define multio_server_Dispatcher_H

#include <map>

#include "multio/server/Message.h"
#include "multio/server/Plan.h"
#include "multio/server/PlanAssembler.h"
#include "multio/server/Transport.h"

namespace multio {
namespace server {

class Dispatcher {
public:
    Dispatcher(const Transport& trans);

    std::string registerPlan(const Message& msg);

    void feedPlan(std::shared_ptr<Message> msg);

    void listen();

private:  // members
    const Transport& transport_;

    PlanAssembler planAssembler_;

    std::map<std::string, Plan> registeredPlans_;

private:  // methods
    bool allPartsArrived(unsigned counter) const;

    void print(std::ostream& os) const;

    friend std::ostream& operator<<(std::ostream& os, const Dispatcher& dpatch) {
        dpatch.print(os);
        return os;
    }
};

}  // namespace server
}  // namespace multio

#endif
