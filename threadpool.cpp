#include <iostream>
#include <thread>

class ZeefanExecutorV1
{
public:
    void execute(void(*func)())
    {
        std::thread(func).detach();
    }
};

void myFuntion()
{
    std::cout<<"thread running" << std::endl;
}


int main()
{
    ZeefanExecutorV1 obj;
    obj.execute(myFuntion);

    return 0;
}