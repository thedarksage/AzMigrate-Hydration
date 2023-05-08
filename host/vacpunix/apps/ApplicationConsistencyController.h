#ifndef APPLICATIONCONSISTENCYCONTROLLER_H
#define APPLICATIONCONSISTENCYCONTROLLER_H
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

class ApplicationConsistencyController;

typedef boost::shared_ptr<ApplicationConsistencyController> ApplicationConsistencyControllerPtr;
class ApplicationConsistencyController
{
    static ApplicationConsistencyControllerPtr m_instancePtr;
    static boost::mutex m_instanceMutex;

public:
    bool Discover();
    bool Freeze();
    bool Unfreeze();

    ApplicationConsistencyController();
    ~ApplicationConsistencyController();

    static ApplicationConsistencyControllerPtr getInstance();
};

#endif
