#include <iostream>
#include <thread>
#include <functional>


class ZeefanExecutorV1
{
public:
    void execute(std::function<void()>func)
    {
        std::thread(func).detach();
    }
};

void myFuntion()
{
    std::cout<<"thread running from function " << std::endl;
}


int main()
{
    ZeefanExecutorV1 obj;
    obj.execute(myFuntion);

    std::cout << std::endl;

    obj.execute([](){
       std::cout<<"thread running from lambda"<<std::endl;
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}